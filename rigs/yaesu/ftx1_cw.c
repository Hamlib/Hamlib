/*
 * Hamlib Yaesu backend - FTX-1 CW Operations Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for CW keyer and messaging.
 *
 * CAT Commands in this file:
 *   KP P1P2;         - Keyer Paddle Ratio (00-30)
 *   KR P1;           - Keyer Reverse (0=normal, 1=reverse)
 *   KS P1P2P3;       - Keyer Speed (004-060 WPM)
 *   KY P1 P2...;     - CW Message Send (P1=0, P2=message up to 24 chars)
 *   KM P1;           - Keyer Memory Read (P1=1-5 message slot)
 *   SD P1P2P3P4;     - CW Break-in Delay (0030-3000ms)
 *   BK P1;           - Break-in mode (0=off, 1=semi, 2=full)
 *   CT P1;           - CW Tuning/Sidetone (0=off, 1=on)
 *   LM P1;           - Load Message (P1=0/1 start/stop)
 *
 * CW Message Character Set:
 *   A-Z, 0-9, standard punctuation
 *   Special prosigns encoded per Yaesu convention
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"

#define FTX1_CW_SPEED_MIN 4
#define FTX1_CW_SPEED_MAX 60
#define FTX1_CW_PADDLE_MIN 0
#define FTX1_CW_PADDLE_MAX 30
/* SD P1P2; - 2 digits, 00-30 (in 100ms increments, so 00-3000ms) */
#define FTX1_CW_DELAY_MIN 0
#define FTX1_CW_DELAY_MAX 30
#define FTX1_CW_MSG_MAX 24

/* Set Keyer Speed (KS P1P2P3;) in WPM */
int ftx1_set_keyer_speed(RIG *rig, int wpm)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    if (wpm < FTX1_CW_SPEED_MIN) wpm = FTX1_CW_SPEED_MIN;
    if (wpm > FTX1_CW_SPEED_MAX) wpm = FTX1_CW_SPEED_MAX;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: wpm=%d\n", __func__, wpm);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KS%03d;", wpm);
    return newcat_set_cmd(rig);
}

/* Get Keyer Speed */
int ftx1_get_keyer_speed(RIG *rig, int *wpm)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, speed;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KS;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%3d", &speed) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *wpm = speed;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: wpm=%d\n", __func__, *wpm);

    return RIG_OK;
}

/* Set Keyer Paddle Ratio (KP P1P2;) */
int ftx1_set_keyer_paddle(RIG *rig, int ratio)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    if (ratio < FTX1_CW_PADDLE_MIN) ratio = FTX1_CW_PADDLE_MIN;
    if (ratio > FTX1_CW_PADDLE_MAX) ratio = FTX1_CW_PADDLE_MAX;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ratio=%d\n", __func__, ratio);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KP%02d;", ratio);
    return newcat_set_cmd(rig);
}

/* Get Keyer Paddle Ratio */
int ftx1_get_keyer_paddle(RIG *rig, int *ratio)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KP;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%2d", &val) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *ratio = val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ratio=%d\n", __func__, *ratio);

    return RIG_OK;
}

/* Set Keyer Reverse (KR P1;) */
int ftx1_set_keyer_reverse(RIG *rig, int reverse)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = reverse ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: reverse=%d\n", __func__, reverse);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KR%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get Keyer Reverse */
int ftx1_get_keyer_reverse(RIG *rig, int *reverse)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KR;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *reverse = p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: reverse=%d\n", __func__, *reverse);

    return RIG_OK;
}

/* Send CW Message (KY P1 P2...;) */
int ftx1_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char safe_msg[FTX1_CW_MSG_MAX + 1];
    size_t len;

    (void)vfo;  /* Unused */

    if (msg == NULL || msg[0] == '\0')
    {
        return -RIG_EINVAL;
    }

    /* Copy and truncate message */
    strncpy(safe_msg, msg, FTX1_CW_MSG_MAX);
    safe_msg[FTX1_CW_MSG_MAX] = '\0';

    /* Convert to uppercase (CW convention) */
    len = strlen(safe_msg);
    for (size_t i = 0; i < len; i++)
    {
        if (safe_msg[i] >= 'a' && safe_msg[i] <= 'z')
        {
            safe_msg[i] = safe_msg[i] - 'a' + 'A';
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: msg='%s'\n", __func__, safe_msg);

    /* KY0 msg; - P1=0 indicates direct send */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KY0%s;", safe_msg);
    return newcat_set_cmd(rig);
}

/* Stop CW Message */
int ftx1_stop_morse(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* KY0; with no message stops sending */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KY0;");
    return newcat_set_cmd(rig);
}

/* Set CW Break-in Delay (SD P1P2;) - 2 digits, 00-30 (100ms units) */
int ftx1_set_cw_delay(RIG *rig, int val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    if (val < FTX1_CW_DELAY_MIN) val = FTX1_CW_DELAY_MIN;
    if (val > FTX1_CW_DELAY_MAX) val = FTX1_CW_DELAY_MAX;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%d\n", __func__, val);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD%02d;", val);
    return newcat_set_cmd(rig);
}

/* Get CW Break-in Delay (SD P1P2;) - 2 digits, 00-30 (100ms units) */
int ftx1_get_cw_delay(RIG *rig, int *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, delay;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%2d", &delay) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = delay;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%d\n", __func__, *val);

    return RIG_OK;
}

/* Set CW Break-in mode (BK P1;) */
int ftx1_set_cw_breakin(RIG *rig, int mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    /* mode: 0=off, 1=semi, 2=full */
    if (mode < 0 || mode > 2)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, mode);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BK%d;", mode);
    return newcat_set_cmd(rig);
}

/* Get CW Break-in mode */
int ftx1_get_cw_breakin(RIG *rig, int *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BK;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *mode = p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, *mode);

    return RIG_OK;
}

/* Set CW Sidetone/Tuning (CT P1;) */
int ftx1_set_cw_sidetone(RIG *rig, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = status ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get CW Sidetone/Tuning status */
int ftx1_get_cw_sidetone(RIG *rig, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, *status);

    return RIG_OK;
}

/*
 * ftx1_load_message - Load Message Control (LM P1;)
 * CAT command: LM P1; (P1=0/1 start/stop)
 * Note: Per spec this is a set-only command
 */
int ftx1_load_message(RIG *rig, int start)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = start ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: start=%d\n", __func__, start);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "LM%02d;", p1);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_keyer_memory - Get Keyer Memory contents (KM P1;)
 * CAT command: KM P1; Response: KM P1 <text>;
 * P1 = message slot (1-5)
 * Note: Firmware may return empty message (firmware limitation)
 */
int ftx1_get_keyer_memory(RIG *rig, int slot, char *msg, size_t msg_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    size_t len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: slot=%d\n", __func__, slot);

    if (slot < 1 || slot > 5)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KM%d;", slot);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: KM P1 <text>; - extract text after "KMx" */
    if (strlen(priv->ret_data) > 3)
    {
        strncpy(msg, priv->ret_data + 3, msg_len - 1);
        msg[msg_len - 1] = '\0';
        /* Remove trailing semicolon if present */
        len = strlen(msg);
        if (len > 0 && msg[len - 1] == ';')
        {
            msg[len - 1] = '\0';
        }
    }
    else
    {
        msg[0] = '\0';  /* Empty message (firmware limitation) */
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: msg='%s'\n", __func__, msg);

    return RIG_OK;
}
