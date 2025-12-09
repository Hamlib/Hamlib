/*
 * Hamlib Yaesu backend - FTX-1 Extended Menu and Setup Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for extended menu access.
 *
 * CAT Commands in this file:
 *   EX P1 P2...;     - Extended Menu Read/Write
 *   MN P1P2P3;       - Menu Number Select (for menu navigation)
 *   QI;              - Quick Command for menu
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

/* Extern from ftx1.c for SPA-1 detection */
extern int ftx1_has_spa1(void);
extern int ftx1_get_head_type(void);

/* Head type constants (must match ftx1.c) */
#define FTX1_HEAD_UNKNOWN   0
#define FTX1_HEAD_FIELD     1
#define FTX1_HEAD_SPA1      2

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
static int ftx1_check_ex_spa1_guardrail(int group, int section, int item, int value)
{
    /*
     * TUNER SELECT (EX030104): Values 0 (INT) and 1 (INT FAST) require SPA-1
     * Internal tuner is only available in SPA-1 amplifier
     */
    if (group == FTX1_EX_TUNER_SELECT_GROUP &&
        section == FTX1_EX_TUNER_SELECT_SECTION &&
        item == FTX1_EX_TUNER_SELECT_ITEM)
    {
        if ((value == 0 || value == 1) && !ftx1_has_spa1())
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
        if (!ftx1_has_spa1())
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
    ret = ftx1_check_ex_spa1_guardrail(menu / 100, (menu % 100), submenu, value);
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
    ret = ftx1_check_ex_spa1_guardrail(group, section, item, value);
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

        *value = atoi(valstr);
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
    if ((tuner_type == 0 || tuner_type == 1) && !ftx1_has_spa1())
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
    int head_type = ftx1_get_head_type();

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
    int head_type = ftx1_get_head_type();

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

/* Quick Info Command (QI;) - query current menu/state info */
int ftx1_quick_info(RIG *rig, char *info, size_t info_len)
{
    int ret;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QI;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: QI data; - extract info after "QI" */
    if (strlen(priv->ret_data) > 2) {
        strncpy(info, priv->ret_data + 2, info_len - 1);
        info[info_len - 1] = '\0';
        /* Remove trailing semicolon if present */
        size_t len = strlen(info);
        if (len > 0 && info[len - 1] == ';') {
            info[len - 1] = '\0';
        }
    } else {
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
