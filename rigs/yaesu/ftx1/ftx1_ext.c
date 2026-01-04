/*
 * Hamlib Yaesu backend - FTX-1 Extended Menu and Setup Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for extended menu access.
 *
 * CAT Commands in this file:
 *   EX P1 P2...;     - Extended Menu Read/Write
 *   MN P1P2P3;       - Menu Number Select (for menu navigation)
 *   QI;              - QMB Store [ACCEPTED but NON-FUNCTIONAL in firmware]
 *
 * Extended Menu Numbers (FTX-1 CAT spec page 10-13):
 *   Format: EX P1 P2 P3 P4; where P1=group, P2=section, P3=item, P4=value
 *
 * SPA-1 Specific Settings:
 *   EX030104 TUNER SELECT: 0=INT, 1=INT(FAST), 2=EXT, 3=ATAS
 *            - INT and INT(FAST) require SPA-1 amplifier
 *   EX030503-09 TX GENERAL (field head power limits):
 *            - HF/50M: 5-10W, V/U: 5-100W (field head only)
 *   EX030705-11 OPTION (SPA-1 power limits):
 *            - HF/50M: 5-100W, V/U: 5-50W (SPA-1 only)
 *
 * NOTE: Extended menu structure is radio-specific and may vary by firmware.
 *       Refer to CAT manual for exact menu numbers and parameter ranges.
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/*
 * SPA-1 specific EX menu item definitions
 * Format: EX P1 P2 P3 P4; (group, section, item, value)
 */

/* TUNER SELECT: EX030104 - values 0 (INT) and 1 (INT FAST) require SPA-1 */
#define FTX1_EX_TUNER_SELECT_GROUP    3
#define FTX1_EX_TUNER_SELECT_SECTION  1
#define FTX1_EX_TUNER_SELECT_ITEM     4

/* TX GENERAL power limits (field head): EX0305xx */
#define FTX1_EX_TX_GENERAL_GROUP      3
#define FTX1_EX_TX_GENERAL_SECTION    5

/* OPTION power limits (SPA-1): EX0307xx */
#define FTX1_EX_OPTION_GROUP          3
#define FTX1_EX_OPTION_SECTION        7

/*
 * ftx1_check_ex_spa1_guardrail - Check if EX menu setting requires SPA-1
 *
 * Returns: RIG_OK if allowed, -RIG_ENAVAIL if SPA-1 required but not present
 */
static int ftx1_check_ex_spa1_guardrail(RIG *rig, int group, int section, int item, int value)
{
    /*
     * TUNER SELECT (EX030104): Values 0 (INT) and 1 (INT FAST) require SPA-1
     * Internal tuner is only available in SPA-1 amplifier
     */
    if (group == FTX1_EX_TUNER_SELECT_GROUP &&
        section == FTX1_EX_TUNER_SELECT_SECTION &&
        item == FTX1_EX_TUNER_SELECT_ITEM)
    {
        if ((value == 0 || value == 1) && !ftx1_has_spa1(rig))
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: TUNER SELECT INT/INT(FAST) requires SPA-1 amplifier\n",
                      __func__);
            return -RIG_ENAVAIL;
        }
    }

    /*
     * OPTION section (EX0307xx): SPA-1 max power settings
     * These settings only apply when SPA-1 is connected
     */
    if (group == FTX1_EX_OPTION_GROUP && section == FTX1_EX_OPTION_SECTION)
    {
        if (!ftx1_has_spa1(rig))
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: OPTION power settings (EX0307%02d) require SPA-1\n",
                      __func__, item);
            return -RIG_ENAVAIL;
        }
    }

    return RIG_OK;
}

/* Set Extended Menu parameter (EX MMNN PPPP;) */
int ftx1_set_ext_parm(RIG *rig, int menu, int submenu, int value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: menu=%d submenu=%d value=%d\n", __func__,
              menu, submenu, value);

    /* Check SPA-1 guardrails for specific menu items */
    ret = ftx1_check_ex_spa1_guardrail(rig, menu / 100, (menu % 100), submenu, value);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Format: EX MM NN PPPP; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX%02d%02d%04d;",
             menu, submenu, value);
    return newcat_set_cmd(rig);
}

/* Get Extended Menu parameter */
int ftx1_get_ext_parm(RIG *rig, int menu, int submenu, int *value)
{
    int ret, mm, nn, val;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: menu=%d submenu=%d\n", __func__,
              menu, submenu);

    /* Query format: EX MM NN; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX%02d%02d;", menu, submenu);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: EX MM NN PPPP; */
    if (sscanf(priv->ret_data + 2, "%02d%02d%04d", &mm, &nn, &val) != 3) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *value = val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: value=%d\n", __func__, *value);

    return RIG_OK;
}

/*
 * FTX-1 Specific EX Command Functions
 *
 * The FTX-1 uses 6-digit EX command format: EX P1 P2 P3 P4;
 *   P1 = group (01-07)
 *   P2 = section (01-07)
 *   P3 = item (01-26)
 *   P4 = value (variable digits)
 */

/*
 * ftx1_set_ex_menu - Set FTX-1 extended menu parameter with SPA-1 guardrails
 *
 * Format: EX P1P2P3 value; (e.g., EX030104 for TUNER SELECT)
 *
 * Parameters:
 *   group   - P1: 1-7 (menu group)
 *   section - P2: 1-7 (section within group)
 *   item    - P3: 1-26 (item within section)
 *   value   - P4: value to set
 *   digits  - number of digits for value (from CAT spec)
 */
int ftx1_set_ex_menu(RIG *rig, int group, int section, int item, int value, int digits)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    char fmt[32];

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: group=%d section=%d item=%d value=%d digits=%d\n",
              __func__, group, section, item, value, digits);

    /* Check SPA-1 guardrails */
    ret = ftx1_check_ex_spa1_guardrail(rig, group, section, item, value);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Build format string based on value digits */
    SNPRINTF(fmt, sizeof(fmt), "EX%%02d%%02d%%02d%%0%dd;", digits);
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), fmt, group, section, item, value);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_ex_menu - Get FTX-1 extended menu parameter
 */
int ftx1_get_ex_menu(RIG *rig, int group, int section, int item, int *value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: group=%d section=%d item=%d\n",
              __func__, group, section, item);

    /* Query format: EX P1 P2 P3; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX%02d%02d%02d;",
             group, section, item);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: EX P1 P2 P3 P4; - parse value after 8 chars (EX + 6 digits) */
    if (strlen(priv->ret_data) > 8)
    {
        char *valstr = priv->ret_data + 8;
        /* Remove trailing semicolon */
        char *semi = strchr(valstr, ';');
        if (semi) *semi = '\0';

        char *endptr;
        long parsed_value = strtol(valstr, &endptr, 10);
        if (endptr == valstr)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: failed to parse value from '%s'\n",
                      __func__, valstr);
            return -RIG_EPROTO;
        }
        *value = (int)parsed_value;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: value=%d\n", __func__, *value);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * ftx1_set_tuner_select - Set antenna tuner type with SPA-1 guardrail
 *
 * Values: 0=INT, 1=INT(FAST), 2=EXT, 3=ATAS
 * INT and INT(FAST) require SPA-1 amplifier
 */
int ftx1_set_tuner_select(RIG *rig, int tuner_type)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: tuner_type=%d\n", __func__, tuner_type);

    if (tuner_type < 0 || tuner_type > 3)
    {
        return -RIG_EINVAL;
    }

    /*
     * GUARDRAIL: INT (0) and INT(FAST) (1) tuner types require SPA-1
     */
    if ((tuner_type == 0 || tuner_type == 1) && !ftx1_has_spa1(rig))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: internal tuner (INT/INT FAST) requires SPA-1 amplifier\n",
                  __func__);
        return -RIG_ENAVAIL;
    }

    /* EX030104 = group 3, section 1, item 4, 1 digit */
    return ftx1_set_ex_menu(rig, 3, 1, 4, tuner_type, 1);
}

/*
 * ftx1_get_tuner_select - Get antenna tuner type setting
 */
int ftx1_get_tuner_select(RIG *rig, int *tuner_type)
{
    return ftx1_get_ex_menu(rig, 3, 1, 4, tuner_type);
}

/*
 * ftx1_set_max_power - Set max power limit with head type awareness
 *
 * TX GENERAL (field head) EX0305xx:
 *   03: HF MAX POWER      5-10W
 *   04: 50M MAX POWER     5-10W
 *   05: 70M MAX POWER     5-60W
 *   06: 144M MAX POWER    5-100W
 *   07: 430M MAX POWER    5-100W
 *   08: AM HF/50 MAX PWR  5-25W
 *   09: AM V/U MAX POWER  5-25W
 *
 * OPTION (SPA-1) EX0307xx:
 *   05: HF MAX POWER      5-100W
 *   06: 50M MAX POWER     5-100W
 *   07: 70M MAX POWER     5-50W
 *   08: 144M MAX POWER    5-50W
 *   09: 430M MAX POWER    5-50W
 *   10: AM MAX POWER      5-25W
 *   11: AM V/U MAX POWER  5-13W
 */
int ftx1_set_max_power(RIG *rig, int band_item, int watts)
{
    int head_type = ftx1_get_head_type(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: band_item=%d watts=%d head_type=%d\n",
              __func__, band_item, watts, head_type);

    if (head_type == FTX1_HEAD_SPA1)
    {
        /* Use OPTION section (EX0307xx) for SPA-1 */
        return ftx1_set_ex_menu(rig, 3, 7, band_item, watts, 3);
    }
    else
    {
        /* Use TX GENERAL section (EX0305xx) for field head */
        return ftx1_set_ex_menu(rig, 3, 5, band_item, watts, 3);
    }
}

/*
 * ftx1_get_max_power - Get max power limit based on head type
 */
int ftx1_get_max_power(RIG *rig, int band_item, int *watts)
{
    int head_type = ftx1_get_head_type(rig);

    if (head_type == FTX1_HEAD_SPA1)
    {
        return ftx1_get_ex_menu(rig, 3, 7, band_item, watts);
    }
    else
    {
        return ftx1_get_ex_menu(rig, 3, 5, band_item, watts);
    }
}

/* Set Menu Number (MN P1P2P3;) */
int ftx1_set_menu(RIG *rig, int menu_num)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: menu_num=%d\n", __func__, menu_num);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MN%03d;", menu_num);
    return newcat_set_cmd(rig);
}

/* Get Menu Number */
int ftx1_get_menu(RIG *rig, int *menu_num)
{
    int ret, val;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MN;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%03d", &val) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *menu_num = val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: menu_num=%d\n", __func__, *menu_num);

    return RIG_OK;
}

/*
 * QMB Store (QI;)
 * NOTE: This function was originally misnamed "quick_info" but QI; is actually
 * the QMB Store command per the CAT manual. However, QI is ACCEPTED but
 * NON-FUNCTIONAL in firmware v1.08+ - the command is parsed but has no effect.
 * Kept for compatibility in case future firmware fixes this.
 */
int ftx1_quick_info(RIG *rig, char *info, size_t info_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s (NOTE: QI is non-functional in firmware)\n", __func__);

    /* QI; is QMB Store - a set-only command that returns empty, not data */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QI;");

    /* Since QI is non-functional and returns empty, just return empty info */
    if (info && info_len > 0) {
        info[0] = '\0';
    }

    return RIG_OK;
}

/*
 * Extended parameter definitions - common menu items
 * These are placeholders - actual menu numbers depend on firmware version
 */

/* Audio menu items (01xx) */
#define FTX1_EX_AF_GAIN_MIN     0100
#define FTX1_EX_RF_GAIN_DEFAULT 0101
#define FTX1_EX_AGC_FAST        0102
#define FTX1_EX_AGC_MID         0103
#define FTX1_EX_AGC_SLOW        0104

/* TX menu items (02xx) */
#define FTX1_EX_MIC_GAIN        0200
#define FTX1_EX_TX_POWER        0201
#define FTX1_EX_VOX_GAIN        0202
#define FTX1_EX_VOX_DELAY       0203
#define FTX1_EX_PROC_LEVEL      0204

/* Display menu items (03xx) */
#define FTX1_EX_DIMMER          0300
#define FTX1_EX_CONTRAST        0301
#define FTX1_EX_BACKLIGHT       0302

/* CW menu items (04xx) */
#define FTX1_EX_CW_SPEED        0400
#define FTX1_EX_CW_PITCH        0401
#define FTX1_EX_CW_WEIGHT       0402
#define FTX1_EX_CW_DELAY        0403

/* Data mode menu items (05xx) */
#define FTX1_EX_DATA_MODE       0500
#define FTX1_EX_DATA_SHIFT      0501
#define FTX1_EX_DATA_LCUT       0502
#define FTX1_EX_DATA_HCUT       0503

/* Helper to set a named parameter (wrapper for common operations) */
int ftx1_set_ext_named(RIG *rig, const char *name, int value)
{
    int menu, submenu;

    /* Map name to menu/submenu - this is radio-specific */
    if (strcmp(name, "MIC_GAIN") == 0) {
        menu = 2; submenu = 0;
    } else if (strcmp(name, "TX_POWER") == 0) {
        menu = 2; submenu = 1;
    } else if (strcmp(name, "VOX_GAIN") == 0) {
        menu = 2; submenu = 2;
    } else if (strcmp(name, "CW_SPEED") == 0) {
        menu = 4; submenu = 0;
    } else if (strcmp(name, "CW_PITCH") == 0) {
        menu = 4; submenu = 1;
    } else if (strcmp(name, "DIMMER") == 0) {
        menu = 3; submenu = 0;
    } else {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown parameter '%s'\n", __func__, name);
        return -RIG_EINVAL;
    }

    return ftx1_set_ext_parm(rig, menu, submenu, value);
}

/* Helper to get a named parameter */
int ftx1_get_ext_named(RIG *rig, const char *name, int *value)
{
    int menu, submenu;

    if (strcmp(name, "MIC_GAIN") == 0) {
        menu = 2; submenu = 0;
    } else if (strcmp(name, "TX_POWER") == 0) {
        menu = 2; submenu = 1;
    } else if (strcmp(name, "VOX_GAIN") == 0) {
        menu = 2; submenu = 2;
    } else if (strcmp(name, "CW_SPEED") == 0) {
        menu = 4; submenu = 0;
    } else if (strcmp(name, "CW_PITCH") == 0) {
        menu = 4; submenu = 1;
    } else if (strcmp(name, "DIMMER") == 0) {
        menu = 3; submenu = 0;
    } else {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown parameter '%s'\n", __func__, name);
        return -RIG_EINVAL;
    }

    return ftx1_get_ext_parm(rig, menu, submenu, value);
}

/* Read all menu parameters into a buffer (for diagnostics) */
int ftx1_dump_ext_menu(RIG *rig, char *buf, size_t buf_len)
{
    int ret;
    int offset = 0;
    int value;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Iterate through known menu ranges */
    for (int menu = 1; menu <= 7; menu++) {
        for (int submenu = 0; submenu <= 10; submenu++) {
            ret = ftx1_get_ext_parm(rig, menu, submenu, &value);
            if (ret == RIG_OK) {
                int n = snprintf(buf + offset, buf_len - offset,
                                "EX%02d%02d=%04d\n", menu, submenu, value);
                if (n > 0 && (size_t)n < buf_len - offset) {
                    offset += n;
                }
            }
        }
    }

    return offset;
}

/*
 * =============================================================================
 * SS Command: Spectrum Scope Settings
 * =============================================================================
 * CAT format: SS P1 P2 [value];
 *   P1 = 0 (fixed)
 *   P2 = parameter type (0-7)
 *
 * P2 Parameter Types:
 *   0 = SPEED    - Scope update speed
 *   1 = PEAK     - Peak hold
 *   2 = MARKER   - Marker display
 *   3 = COLOR    - Display color
 *   4 = LEVEL    - Reference level (returns format like -05.0)
 *   5 = SPAN     - Frequency span
 *   6 = MODE     - Scope mode
 *   7 = AF-FFT   - Audio FFT display
 *
 * Read: SS0X; returns SS0Xvalue; (value format varies by parameter)
 * Set: SS0Xvalue; (set-only for some parameters)
 *
 * Verified working 2025-12-09 via direct serial testing
 */

/* Spectrum scope parameter types */
#define FTX1_SS_SPEED   0
#define FTX1_SS_PEAK    1
#define FTX1_SS_MARKER  2
#define FTX1_SS_COLOR   3
#define FTX1_SS_LEVEL   4
#define FTX1_SS_SPAN    5
#define FTX1_SS_MODE    6
#define FTX1_SS_AFFFT   7

/*
 * ftx1_get_spectrum_scope - Get spectrum scope parameter
 *
 * Parameters:
 *   param - parameter type (0-7, use FTX1_SS_* constants)
 *   value - pointer to store value (format depends on param type)
 *
 * Returns: RIG_OK on success, negative on error
 */
int ftx1_get_spectrum_scope(RIG *rig, int param, char *value, size_t value_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: param=%d\n", __func__, param);

    if (param < 0 || param > 7)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid param %d (must be 0-7)\n",
                  __func__, param);
        return -RIG_EINVAL;
    }

    /* Query format: SS0X; where X is param type */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SS0%d;", param);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /*
     * Response format: SS0Xvalue;
     * Value starts at position 3 (after SS0X prefix)
     * Examples:
     *   SS00 -> SS0020000 (SPEED)
     *   SS01 -> SS0100000 (PEAK)
     *   SS04 -> SS04-05.0 (LEVEL, note signed decimal)
     */
    if (strlen(priv->ret_data) > 3)
    {
        const char *val_start = priv->ret_data + 3;
        size_t val_len = strlen(val_start);

        /* Remove trailing semicolon */
        if (val_len > 0 && val_start[val_len - 1] == ';')
        {
            val_len--;
        }

        if (val_len >= value_len)
        {
            val_len = value_len - 1;
        }

        strncpy(value, val_start, val_len);
        value[val_len] = '\0';

        rig_debug(RIG_DEBUG_VERBOSE, "%s: param=%d value='%s'\n",
                  __func__, param, value);
    }
    else
    {
        value[0] = '\0';
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * ftx1_set_spectrum_scope - Set spectrum scope parameter
 *
 * Parameters:
 *   param - parameter type (0-7, use FTX1_SS_* constants)
 *   value - value to set (format depends on param type)
 *
 * Returns: RIG_OK on success, negative on error
 */
int ftx1_set_spectrum_scope(RIG *rig, int param, const char *value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: param=%d value='%s'\n",
              __func__, param, value);

    if (param < 0 || param > 7)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid param %d (must be 0-7)\n",
                  __func__, param);
        return -RIG_EINVAL;
    }

    /* Set format: SS0Xvalue; where X is param type */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SS0%d%s;", param, value);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_scope_speed - Get spectrum scope update speed
 *
 * Returns speed value (typically 5-digit format like 20000)
 */
int ftx1_get_scope_speed(RIG *rig, int *speed)
{
    char value[16];
    int ret;

    ret = ftx1_get_spectrum_scope(rig, FTX1_SS_SPEED, value, sizeof(value));
    if (ret == RIG_OK)
    {
        *speed = atoi(value);
    }
    return ret;
}

/*
 * ftx1_get_scope_level - Get spectrum scope reference level
 *
 * Returns level value (can be negative, like -5.0 dB)
 */
int ftx1_get_scope_level(RIG *rig, float *level)
{
    char value[16];
    char *endptr;
    int ret;

    ret = ftx1_get_spectrum_scope(rig, FTX1_SS_LEVEL, value, sizeof(value));
    if (ret == RIG_OK)
    {
        *level = strtof(value, &endptr);
        if (endptr == value)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: failed to parse level from '%s'\n",
                      __func__, value);
            return -RIG_EPROTO;
        }
    }
    return ret;
}

/*
 * ftx1_get_scope_span - Get spectrum scope frequency span
 *
 * Returns span value
 */
int ftx1_get_scope_span(RIG *rig, int *span)
{
    char value[16];
    int ret;

    ret = ftx1_get_spectrum_scope(rig, FTX1_SS_SPAN, value, sizeof(value));
    if (ret == RIG_OK)
    {
        *span = atoi(value);
    }
    return ret;
}

/*
 * =============================================================================
 * EO Command: Encoder Offset (Dial Step)
 * =============================================================================
 * CAT format: EO P1 P2 P3 P4 P5;
 *   P1 = VFO (0=MAIN, 1=SUB)
 *   P2 = Encoder (0=MAIN dial, 1=FUNC knob)
 *   P3 = Direction (+/-)
 *   P4 = Unit (0=Hz, 1=kHz, 2=MHz)
 *   P5 = Value (000-999)
 *
 * This is a SET-ONLY command (returns empty on success)
 * Used for programmatic frequency stepping without physical dial rotation
 *
 * Example: EO00+0100; = VFO MAIN, MAIN dial, +, Hz, 100 = step up 100 Hz
 *
 * Verified working 2025-12-09 via direct serial testing
 */

/* Encoder offset VFO selection */
#define FTX1_EO_VFO_MAIN    0
#define FTX1_EO_VFO_SUB     1

/* Encoder offset encoder selection */
#define FTX1_EO_ENC_MAIN    0   /* Main dial (frequency) */
#define FTX1_EO_ENC_FUNC    1   /* Function knob */

/* Encoder offset unit selection */
#define FTX1_EO_UNIT_HZ     0
#define FTX1_EO_UNIT_KHZ    1
#define FTX1_EO_UNIT_MHZ    2

/*
 * ftx1_set_encoder_offset - Send encoder offset command
 *
 * This simulates turning the dial by a specified amount.
 * Useful for programmatic frequency stepping.
 *
 * Parameters:
 *   vfo_select - 0=MAIN, 1=SUB
 *   encoder    - 0=MAIN dial, 1=FUNC knob
 *   direction  - +1 for up, -1 for down
 *   unit       - 0=Hz, 1=kHz, 2=MHz
 *   value      - step amount (0-999)
 *
 * Returns: RIG_OK on success, negative on error
 */
int ftx1_set_encoder_offset(RIG *rig, int vfo_select, int encoder,
                            int direction, int unit, int value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char dir_char;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: vfo=%d encoder=%d dir=%d unit=%d value=%d\n",
              __func__, vfo_select, encoder, direction, unit, value);

    /* Validate parameters */
    if (vfo_select < 0 || vfo_select > 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid vfo_select %d\n",
                  __func__, vfo_select);
        return -RIG_EINVAL;
    }

    if (encoder < 0 || encoder > 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid encoder %d\n",
                  __func__, encoder);
        return -RIG_EINVAL;
    }

    if (unit < 0 || unit > 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid unit %d\n", __func__, unit);
        return -RIG_EINVAL;
    }

    if (value < 0 || value > 999)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid value %d (must be 0-999)\n",
                  __func__, value);
        return -RIG_EINVAL;
    }

    dir_char = (direction >= 0) ? '+' : '-';

    /* Format: EO P1 P2 P3 P4 P5P5P5; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EO%d%d%c%d%03d;",
             vfo_select, encoder, dir_char, unit, value);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_step_frequency - Step frequency up or down by specified amount
 *
 * Convenience wrapper for ftx1_set_encoder_offset() that steps the
 * main VFO dial by a specified Hz value.
 *
 * Parameters:
 *   vfo   - RIG_VFO_MAIN, RIG_VFO_A, etc. (or RIG_VFO_CURR for main)
 *   hz    - step amount in Hz (positive=up, negative=down)
 *
 * Note: Large steps are automatically converted to kHz or MHz units
 *       for efficiency. Maximum single step is 999 MHz.
 *
 * Returns: RIG_OK on success, negative on error
 */
int ftx1_step_frequency(RIG *rig, vfo_t vfo, int hz)
{
    int vfo_select;
    int direction;
    int unit;
    int value;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s hz=%d\n",
              __func__, rig_strvfo(vfo), hz);

    /* Determine VFO (resolves currVFO properly) */
    vfo_select = ftx1_vfo_to_p1(rig, vfo);

    /* Handle direction */
    direction = (hz >= 0) ? 1 : -1;
    if (hz < 0) hz = -hz;

    /* Determine best unit for the step */
    if (hz >= 1000000 && (hz % 1000000) == 0)
    {
        unit = FTX1_EO_UNIT_MHZ;
        value = hz / 1000000;
    }
    else if (hz >= 1000 && (hz % 1000) == 0)
    {
        unit = FTX1_EO_UNIT_KHZ;
        value = hz / 1000;
    }
    else
    {
        unit = FTX1_EO_UNIT_HZ;
        value = hz;
    }

    /* Clamp to max 999 */
    if (value > 999)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: value %d exceeds max 999, clamping\n",
                  __func__, value);
        value = 999;
    }

    return ftx1_set_encoder_offset(rig, vfo_select, FTX1_EO_ENC_MAIN,
                                   direction, unit, value);
}
