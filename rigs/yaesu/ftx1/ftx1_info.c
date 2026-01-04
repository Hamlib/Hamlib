/*
 * Hamlib Yaesu backend - FTX-1 Display and Information Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for radio info and display control.
 *
 * CAT Commands in this file:
 *   AI P1;           - Auto Information (0=off, 1=on)
 *   ID;              - Radio ID (returns 0840 for FTX-1)
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

#define FTX1_ID "0840"  /* Radio ID for FTX-1 */

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
        char *endptr;
        strncpy(freq_str, priv->ret_data + 2, 9);
        freq_str[9] = '\0';
        *freq = strtod(freq_str, &endptr);
        if (endptr == freq_str)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: failed to parse frequency from '%s'\n",
                      __func__, freq_str);
            return -RIG_EPROTO;
        }
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
        char *endptr;
        strncpy(freq_str, priv->ret_data + 2, 9);
        freq_str[9] = '\0';
        *freq = strtod(freq_str, &endptr);
        if (endptr == freq_str)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: failed to parse frequency from '%s'\n",
                      __func__, freq_str);
            return -RIG_EPROTO;
        }

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

/*
 * ftx1_get_callsign - Get stored callsign (MY CALL)
 * CAT command: EX040101; Response: EX040101<callsign>;
 * Menu path: DISPLAY SETTING -> DISPLAY -> MY CALL
 * Returns up to 10 characters.
 */
int ftx1_get_callsign(RIG *rig, char *callsign, size_t callsign_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    size_t len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX040101;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: EX040101<callsign>; - extract callsign after "EX040101" (8 chars) */
    if (strlen(priv->ret_data) > 8)
    {
        strncpy(callsign, priv->ret_data + 8, callsign_len - 1);
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

/*
 * ftx1_set_callsign - Set stored callsign (MY CALL)
 * CAT command: EX040101<callsign>;
 * Menu path: DISPLAY SETTING -> DISPLAY -> MY CALL
 * Max 10 characters.
 */
int ftx1_set_callsign(RIG *rig, const char *callsign)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char safe_call[11];  /* Max 10 chars + null */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: callsign='%s'\n", __func__,
              callsign ? callsign : "(null)");

    if (callsign == NULL)
    {
        callsign = "";
    }

    /* Copy and truncate to 10 chars max */
    strncpy(safe_call, callsign, 10);
    safe_call[10] = '\0';

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX040101%s;", safe_call);
    return newcat_set_cmd(rig);
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
    char *endptr;
    char mode_char;
    char vfo_char;

    (void)rig;  /* Unused for now */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: resp='%s'\n", __func__, resp);

    /* Reparse from string rather than re-query */
    if (strlen(resp) < 27)
    {
        return -RIG_EPROTO;
    }

    /* Extract frequency using strtod for validation */
    strncpy(freq_str, resp + 2, 9);
    freq_str[9] = '\0';
    freq = strtod(freq_str, &endptr);
    if (endptr == freq_str)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse frequency from '%s'\n",
                  __func__, freq_str);
        return -RIG_EPROTO;
    }

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
    int p1 = ftx1_vfo_to_p1(rig, vfo);
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
    int p1 = ftx1_vfo_to_p1(rig, vfo);
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

/*
 * =============================================================================
 * GP Command: GP OUT A/B/C/D (Digital Output Pins)
 * =============================================================================
 * CAT format: GP P1 P2 P3 P4;
 *   P1 = GP OUT A (0=LOW, 1=HIGH)
 *   P2 = GP OUT B (0=LOW, 1=HIGH)
 *   P3 = GP OUT C (0=LOW, 1=HIGH)
 *   P4 = GP OUT D (0=LOW, 1=HIGH)
 *
 * Read response: GP0000 to GP1111 (4 binary digits)
 * Set command: GPnnnn;
 *
 * Note: Requires TUN/LIN PORT SELECT menu to be set to "GPO".
 * Output is 5V CMOS level, max 3mA per pin.
 */

/*
 * ftx1_set_gp_out - Set GP OUT pin states
 *
 * Parameters:
 *   out_a, out_b, out_c, out_d - pin states (0=LOW, 1=HIGH)
 */
int ftx1_set_gp_out(RIG *rig, int out_a, int out_b, int out_c, int out_d)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: A=%d B=%d C=%d D=%d\n",
              __func__, out_a, out_b, out_c, out_d);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GP%d%d%d%d;",
             out_a ? 1 : 0, out_b ? 1 : 0, out_c ? 1 : 0, out_d ? 1 : 0);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_gp_out - Get GP OUT pin states
 *
 * Returns all 4 pin states via output parameters.
 */
int ftx1_get_gp_out(RIG *rig, int *out_a, int *out_b, int *out_c, int *out_d)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2, p3, p4;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GP;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: GPnnnn where each n is 0 or 1 */
    if (sscanf(priv->ret_data + 2, "%1d%1d%1d%1d", &p1, &p2, &p3, &p4) != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *out_a = p1;
    *out_b = p2;
    *out_c = p3;
    *out_d = p4;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: A=%d B=%d C=%d D=%d\n",
              __func__, *out_a, *out_b, *out_c, *out_d);

    return RIG_OK;
}

/*
 * =============================================================================
 * SF Command: Sub Dial (FUNC Knob) Function Assignment
 * =============================================================================
 * CAT format: SF P1 P2;
 *   P1 = VFO (0=MAIN, always 0 for this radio)
 *   P2 = Function code (single hex char 0-H):
 *        0 = None (-)
 *        1 = SCOPE LEVEL
 *        2 = PEAK
 *        3 = COLOR
 *        4 = CONTRAST
 *        5 = DIMMER
 *        6 = Reserved (-)
 *        7 = MIC GAIN
 *        8 = PROC LEVEL
 *        9 = AMC LEVEL
 *        A = VOX GAIN
 *        B = VOX DELAY
 *        C = Reserved (-)
 *        D = RF POWER
 *        E = MONI LEVEL
 *        F = CW SPEED
 *        G = CW PITCH
 *        H = BK-DELAY
 *
 * Read command: SF0;
 * Read response: SF0X (e.g., SF00, SF07, SF0D, SF0G)
 * Set command: SF0X; (WARNING: Some values may affect SPA-1 detection)
 */

/* FUNC knob function codes */
#define FTX1_SF_NONE        0x00
#define FTX1_SF_SCOPE_LEVEL 0x01
#define FTX1_SF_PEAK        0x02
#define FTX1_SF_COLOR       0x03
#define FTX1_SF_CONTRAST    0x04
#define FTX1_SF_DIMMER      0x05
#define FTX1_SF_MIC_GAIN    0x07
#define FTX1_SF_PROC_LEVEL  0x08
#define FTX1_SF_AMC_LEVEL   0x09
#define FTX1_SF_VOX_GAIN    0x0A
#define FTX1_SF_VOX_DELAY   0x0B
#define FTX1_SF_RF_POWER    0x0D
#define FTX1_SF_MONI_LEVEL  0x0E
#define FTX1_SF_CW_SPEED    0x0F
#define FTX1_SF_CW_PITCH    0x10
#define FTX1_SF_BK_DELAY    0x11

/*
 * ftx1_set_func_knob - Set FUNC knob function assignment
 *
 * func_code: 0x00-0x11 (see FTX1_SF_* defines)
 *
 * CAT format: SF P1 P2; where P1=VFO (0), P2=single hex char (0-H)
 * WARNING: Setting some values may affect SPA-1 amp detection.
 */
int ftx1_set_func_knob(RIG *rig, int func_code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char func_char;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: func_code=%d (0x%02X)\n",
              __func__, func_code, func_code);

    if (func_code < 0 || func_code > 0x11)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid func_code %d\n",
                  __func__, func_code);
        return -RIG_EINVAL;
    }

    /* Convert to single hex character (0-9, A-H) */
    if (func_code < 10)
    {
        func_char = '0' + func_code;
    }
    else
    {
        func_char = 'A' + (func_code - 10);  /* A=10, B=11, ..., H=17 */
    }

    /* Format: SF0X where X is single hex char */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SF0%c;", func_char);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_func_knob - Get FUNC knob function assignment
 *
 * Response format: SF0X where X is single hex char (0-9, A-H)
 */
int ftx1_get_func_knob(RIG *rig, int *func_code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    char func_char;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SF0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: SF0X where X is single hex char (0-9, A-H) */
    if (strlen(priv->ret_data) < 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    func_char = priv->ret_data[3];

    /* Convert hex char to int */
    if (func_char >= '0' && func_char <= '9')
    {
        *func_code = func_char - '0';
    }
    else if (func_char >= 'A' && func_char <= 'H')
    {
        *func_code = func_char - 'A' + 10;  /* A=10, B=11, ..., H=17 */
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid func_char '%c' in '%s'\n",
                  __func__, func_char, priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: func_code=%d (0x%02X)\n",
              __func__, *func_code, *func_code);

    return RIG_OK;
}
