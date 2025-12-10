/*
 * Hamlib Yaesu backend - FTX-1 Display and Information Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for radio info and display control.
 *
 * CAT Commands in this file:
 *   AI P1;           - Auto Information (0=off, 1=on)
 *   ID;              - Radio ID (returns 0763 for FTX-1)
 *   IF;              - Information Query (composite status)
 *   OI;              - Opposite Band Information
 *   DA P1;           - Dimmer (display brightness 0-15)
 *   LK P1;           - Lock (0=off, 1=lock)
 *   CS P1;           - Callsign display
 *   NM P1;           - Name/Label display
 *   DT P1 P2...;     - Date/Time (P1=0 date/1 time)
 *   IS P1 P2 P3 P4;  - IF Shift (P1=VFO, P2=on/off, P3=dir, P4=freq)
 *
 * IF Command Response Format (27+ characters):
 *   Position  Content
 *   01-02     "IF"
 *   03-11     Frequency (9 digits)
 *   12-16     Clarifier offset (5 chars, signed)
 *   17        RX clarifier status
 *   18        TX clarifier status
 *   19        Mode
 *   20        VFO Memory status
 *   21        CTCSS status
 *   22        Simplex/Split
 *   23-24     Tone number
 *   25        Shift direction
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

#define FTX1_ID "0763"  /* Radio ID for FTX-1 */

#define FTX1_DIMMER_MIN 0
#define FTX1_DIMMER_MAX 15

/* Set Auto-Information mode (AI P1;) */
int ftx1_set_trn(RIG *rig, int trn)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = trn ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: trn=%d\n", __func__, trn);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AI%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get Auto-Information mode */
int ftx1_get_trn(RIG *rig, int *trn)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AI;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *trn = p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: trn=%d\n", __func__, *trn);

    return RIG_OK;
}

/* Get Radio ID (ID;) */
int ftx1_get_info(RIG *rig, char *info, size_t info_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    size_t len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ID;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: ID xxxx; - extract ID after "ID" */
    if (strlen(priv->ret_data) > 2)
    {
        strncpy(info, priv->ret_data + 2, info_len - 1);
        info[info_len - 1] = '\0';
        /* Remove trailing semicolon */
        len = strlen(info);

        if (len > 0 && info[len - 1] == ';')
        {
            info[len - 1] = '\0';
        }
    }
    else
    {
        info[0] = '\0';
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: info='%s'\n", __func__, info);

    return RIG_OK;
}

/* Get composite information (IF;) */
int ftx1_get_if_info(RIG *rig, freq_t *freq, rmode_t *mode, vfo_t *vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IF;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: IF response='%s'\n", __func__,
              priv->ret_data);

    /* Parse IF response - minimum 27 characters */
    if (strlen(priv->ret_data) < 27)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: IF response too short '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Extract frequency (positions 3-11, 9 digits) */
    if (freq)
    {
        char freq_str[10];
        strncpy(freq_str, priv->ret_data + 2, 9);
        freq_str[9] = '\0';
        *freq = atof(freq_str);
    }

    /* Extract mode (position 19) */
    if (mode)
    {
        char mode_char = priv->ret_data[18];

        switch (mode_char)
        {
        case '1': *mode = RIG_MODE_LSB; break;
        case '2': *mode = RIG_MODE_USB; break;
        case '3': *mode = RIG_MODE_CW; break;
        case '4': *mode = RIG_MODE_FM; break;
        case '5': *mode = RIG_MODE_AM; break;
        case '6': *mode = RIG_MODE_RTTY; break;
        case '7': *mode = RIG_MODE_CWR; break;
        case '8': *mode = RIG_MODE_PKTLSB; break;
        case '9': *mode = RIG_MODE_RTTYR; break;
        case 'A': *mode = RIG_MODE_PKTFM; break;
        case 'B': *mode = RIG_MODE_FMN; break;
        case 'C': *mode = RIG_MODE_PKTUSB; break;
        default:  *mode = RIG_MODE_NONE; break;
        }
    }

    /* Extract VFO/Memory (position 20) */
    if (vfo)
    {
        char vfo_char = priv->ret_data[19];
        *vfo = (vfo_char == '1') ? RIG_VFO_MEM : RIG_VFO_CURR;
    }

    return RIG_OK;
}

/* Get opposite band info (OI;) */
int ftx1_get_oi_info(RIG *rig, freq_t *freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "OI;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* OI response format similar to IF */
    if (strlen(priv->ret_data) >= 11)
    {
        char freq_str[10];
        strncpy(freq_str, priv->ret_data + 2, 9);
        freq_str[9] = '\0';
        *freq = atof(freq_str);

        rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%f\n", __func__, *freq);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: OI response too short '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * FTX-1 DA command format: DA P1P2 P3P4 P5P6 P7P8;
 *   P1P2 = LCD Dimmer (00-15)
 *   P3P4 = LED Dimmer (00-15)
 *   P5P6 = TX/Busy LED (00-15)
 *   P7P8 = Unknown (00-15)
 *
 * For set, we need to preserve other fields, so read-modify-write.
 * For simplicity, we only expose LCD dimmer (first field).
 */

/* Set Display Dimmer (DA P1P2 P3P4 P5P6 P7P8;) - sets LCD dimmer only */
int ftx1_set_dimmer(RIG *rig, int level)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2, p3, p4;

    if (level < FTX1_DIMMER_MIN) level = FTX1_DIMMER_MIN;
    if (level > FTX1_DIMMER_MAX) level = FTX1_DIMMER_MAX;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%d\n", __func__, level);

    /* Read current values first */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DA;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        /* If read fails, just set with defaults for other fields */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DA%02d101520;", level);
        return newcat_set_cmd(rig);
    }

    /* Parse current: DA P1P2 P3P4 P5P6 P7P8; */
    if (sscanf(priv->ret_data + 2, "%2d%2d%2d%2d", &p1, &p2, &p3, &p4) != 4)
    {
        /* Parse failed, use defaults */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DA%02d101520;", level);
        return newcat_set_cmd(rig);
    }

    /* Set new LCD dimmer, preserve others */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DA%02d%02d%02d%02d;",
             level, p2, p3, p4);
    return newcat_set_cmd(rig);
}

/* Get Display Dimmer - returns LCD dimmer (first field) */
int ftx1_get_dimmer(RIG *rig, int *level)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DA;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse first field (LCD dimmer) from DA P1P2 P3P4 P5P6 P7P8; */
    if (sscanf(priv->ret_data + 2, "%2d", &val) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *level = val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%d\n", __func__, *level);

    return RIG_OK;
}

/* Set Lock (LK P1;) */
int ftx1_set_lock(RIG *rig, int lock)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = lock ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: lock=%d\n", __func__, lock);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "LK%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get Lock status */
int ftx1_get_lock(RIG *rig, int *lock)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "LK;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *lock = p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: lock=%d\n", __func__, *lock);

    return RIG_OK;
}

/* Get Callsign (CS;) - read stored callsign */
int ftx1_get_callsign(RIG *rig, char *callsign, size_t callsign_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    size_t len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CS;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: CS callsign; - extract callsign after "CS" */
    if (strlen(priv->ret_data) > 2)
    {
        strncpy(callsign, priv->ret_data + 2, callsign_len - 1);
        callsign[callsign_len - 1] = '\0';
        /* Remove trailing semicolon if present */
        len = strlen(callsign);

        if (len > 0 && callsign[len - 1] == ';')
        {
            callsign[len - 1] = '\0';
        }
    }
    else
    {
        callsign[0] = '\0';
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: callsign='%s'\n", __func__, callsign);

    return RIG_OK;
}

/* Set Name/Label display mode (NM P1;) */
int ftx1_set_name_display(RIG *rig, int mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = mode ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, mode);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NM%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get Name/Label display mode */
int ftx1_get_name_display(RIG *rig, int *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NM;");

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

/* Decode IF response to fill rig state (utility function) */
int ftx1_decode_if_response(RIG *rig, const char *resp)
{
    freq_t freq;
    rmode_t mode;
    vfo_t vfo;
    char freq_str[10];
    char mode_char;
    char vfo_char;

    (void)rig;  /* Unused for now */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: resp='%s'\n", __func__, resp);

    /* Reparse from string rather than re-query */
    if (strlen(resp) < 27)
    {
        return -RIG_EPROTO;
    }

    /* Extract frequency */
    strncpy(freq_str, resp + 2, 9);
    freq_str[9] = '\0';
    freq = atof(freq_str);

    /* Extract mode */
    mode_char = resp[18];

    switch (mode_char)
    {
    case '1': mode = RIG_MODE_LSB; break;
    case '2': mode = RIG_MODE_USB; break;
    case '3': mode = RIG_MODE_CW; break;
    case '4': mode = RIG_MODE_FM; break;
    case '5': mode = RIG_MODE_AM; break;
    default:  mode = RIG_MODE_NONE; break;
    }

    /* Extract VFO */
    vfo_char = resp[19];
    vfo = (vfo_char == '1') ? RIG_VFO_MEM : RIG_VFO_CURR;

    /* Could update rig state cache here */
    (void)freq;
    (void)mode;
    (void)vfo;

    return RIG_OK;
}

/*
 * ftx1_set_date - Set Date
 * CAT command: DT 0 YYYYMMDD; (P1=0 for date)
 */
int ftx1_set_date(RIG *rig, int year, int month, int day)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: year=%d month=%d day=%d\n", __func__,
              year, month, day);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT0%04d%02d%02d;",
             year, month, day);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_date - Get Date
 * CAT command: DT 0; Response: DT0 YYYYMMDD;
 */
int ftx1_get_date(RIG *rig, int *year, int *month, int *day)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: DT0 YYYYMMDD; */
    if (sscanf(priv->ret_data + 3, "%4d%2d%2d", year, month, day) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: year=%d month=%d day=%d\n", __func__,
              *year, *month, *day);

    return RIG_OK;
}

/*
 * ftx1_set_time - Set Time
 * CAT command: DT 1 HHMMSS; (P1=1 for time)
 */
int ftx1_set_time(RIG *rig, int hour, int min, int sec)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: hour=%d min=%d sec=%d\n", __func__,
              hour, min, sec);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT1%02d%02d%02d;",
             hour, min, sec);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_time - Get Time
 * CAT command: DT 1; Response: DT1 HHMMSS;
 */
int ftx1_get_time(RIG *rig, int *hour, int *min, int *sec)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT1;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: DT1 HHMMSS; */
    if (sscanf(priv->ret_data + 3, "%2d%2d%2d", hour, min, sec) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: hour=%d min=%d sec=%d\n", __func__,
              *hour, *min, *sec);

    return RIG_OK;
}

/*
 * ftx1_set_if_shift - Set IF Shift
 * CAT command: IS P1 P2 P3 P4P4P4P4;
 *   P1 = VFO (0=MAIN, 1=SUB)
 *   P2 = IF Shift on/off (0/1)
 *   P3 = Direction (+/-)
 *   P4 = Shift freq (0000-1200)
 */
int ftx1_set_if_shift(RIG *rig, vfo_t vfo, int on, int shift_hz)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p2 = on ? 1 : 0;
    char p3 = (shift_hz >= 0) ? '+' : '-';
    int p4 = (shift_hz >= 0) ? shift_hz : -shift_hz;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d on=%d shift=%d\n", __func__,
              rig_strvfo(vfo), p1, on, shift_hz);

    if (p4 > 1200) p4 = 1200;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IS%d%d%c%04d;",
             p1, p2, p3, p4);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_if_shift - Get IF Shift
 * CAT command: IS P1; Response: IS P1 P2 P3 P4P4P4P4;
 */
int ftx1_get_if_shift(RIG *rig, vfo_t vfo, int *on, int *shift_hz)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, p4;
    char p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IS%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: IS P1 P2 P3 P4P4P4P4; */
    if (sscanf(priv->ret_data + 2, "%1d%1d%c%4d", &p1_resp, &p2, &p3, &p4) != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *on = p2;
    *shift_hz = (p3 == '-') ? -p4 : p4;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: on=%d shift=%d\n", __func__, *on, *shift_hz);

    return RIG_OK;
}
