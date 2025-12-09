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
 * Extended Menu Numbers:
 *   The FTX-1 has extensive menu settings accessible via EX command.
 *   Format: EX MMNN PPPP; where MM=menu, NN=submenu, PPPP=parameter
 *
 * Common menu areas:
 *   - Audio settings (01xx)
 *   - TX settings (02xx)
 *   - Display settings (03xx)
 *   - CW settings (04xx)
 *   - Data mode settings (05xx)
 *   - RTTY settings (06xx)
 *   - Band settings (07xx)
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

/* Set Extended Menu parameter (EX MMNN PPPP;) */
int ftx1_set_ext_parm(RIG *rig, int menu, int submenu, int value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: menu=%d submenu=%d value=%d\n", __func__,
              menu, submenu, value);

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
