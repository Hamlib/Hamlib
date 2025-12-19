/*
 * Hamlib Yaesu backend - FTX-1 Scan and Band Operations Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for scanning and band selection.
 *
 * ===========================================================================
 * SPECIAL ACKNOWLEDGMENT - JEREMY MILLER (KO4SSD)
 * ===========================================================================
 * The mode-specific tuning step implementation using EX0306 is based on the
 * excellent work of Jeremy Miller (KO4SSD) in Hamlib PR #1826. Jeremy figured
 * out that the FTX-1 supports mode-aware dial steps via the EX0306 extended
 * menu command, which provides finer control than the basic TS command.
 *
 * His implementation properly handles:
 *   - SSB/CW mode: 5Hz, 10Hz, 20Hz steps (EX030601)
 *   - RTTY/PSK mode: 5Hz, 10Hz, 20Hz steps (EX030602)
 *   - FM mode: 5kHz to 25kHz steps + Auto (EX030603)
 *
 * Thank you, Jeremy, for this valuable contribution to FTX-1 support!
 * Jeremy's original implementation: https://github.com/Hamlib/Hamlib/pull/1826
 * ===========================================================================
 *
 * CAT Commands in this file:
 *   SC P1;           - Scan Control (0=stop, 1=up, 2=down)
 *   BS P1P2;         - Band Select (P1P2=band code)
 *   BU P1;           - Band Up (P1=VFO, cycles to higher band)
 *   BD P1;           - Band Down (P1=VFO, cycles to lower band)
 *   UP;              - Frequency/Channel Up
 *   DN;              - Frequency/Channel Down
 *   TS P1P2;         - Tuning Step (00-14 code for step size)
 *   EX0306xx;        - Mode-specific Dial Step (Jeremy Miller's discovery)
 *   ZI P1;           - Zero In (P1=VFO 0/1, set-only)
 *
 * Band Codes (BS command):
 *   00 = 160m (1.8MHz)     08 = 15m (21MHz)
 *   01 = 80m (3.5MHz)      09 = 12m (24MHz)
 *   02 = 60m (5MHz)        10 = 10m (28MHz)
 *   03 = 40m (7MHz)        11 = 6m (50MHz)
 *   04 = 30m (10MHz)       12 = GEN (general coverage)
 *   05 = 20m (14MHz)       13 = MW (AM broadcast)
 *   06 = 17m (18MHz)       14 = AIR (airband)
 *   07 = (reserved)
 *
 * Tuning Step Codes:
 *   00=1Hz, 01=2.5Hz, 02=5Hz, 03=10Hz, 04=25Hz, 05=50Hz, 06=100Hz
 *   07=250Hz, 08=500Hz, 09=1kHz, 10=2.5kHz, 11=5kHz, 12=10kHz
 *   13=50kHz, 14=100kHz
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "../ftx1.h"

/* Band codes */
#define FTX1_BAND_160M   0
#define FTX1_BAND_80M    1
#define FTX1_BAND_60M    2
#define FTX1_BAND_40M    3
#define FTX1_BAND_30M    4
#define FTX1_BAND_20M    5
#define FTX1_BAND_17M    6
#define FTX1_BAND_15M    8
#define FTX1_BAND_12M    9
#define FTX1_BAND_10M    10
#define FTX1_BAND_6M     11
#define FTX1_BAND_GEN    12
#define FTX1_BAND_MW     13
#define FTX1_BAND_AIR    14
#define FTX1_BAND_MAX    14

/* Tuning step values in Hz */
static const int ftx1_tuning_steps[] = {
    1, 2, 5, 10, 25, 50, 100, 250, 500, 1000,
    2500, 5000, 10000, 50000, 100000
};
#define FTX1_TS_COUNT 15

/* Set Scan mode (SC P1 P2;) - P1=VFO, P2=scan mode */
int ftx1_set_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;  /* VFO: 0=MAIN, 1=SUB */
    int p2;  /* Scan mode: 0=OFF, 1=UP, 2=DOWN */

    (void)ch;

    p1 = FTX1_VFO_TO_P1(vfo);

    switch (scan) {
        case RIG_SCAN_STOP:
            p2 = 0;
            break;
        case RIG_SCAN_MEM:
        case RIG_SCAN_VFO:
            p2 = 1;  /* Scan up */
            break;
        default:
            p2 = 0;
            break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s scan=%d p1=%d p2=%d\n", __func__,
              rig_strvfo(vfo), scan, p1, p2);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC%d%d;", p1, p2);
    return newcat_set_cmd(rig);
}

/* Get Scan status (SC P1;) - P1=VFO, response: SC P1 P2 */
int ftx1_get_scan(RIG *rig, vfo_t vfo, scan_t *scan, int *ch)
{
    int ret, p1, p2;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    p1 = FTX1_VFO_TO_P1(vfo);
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: SC P1 P2 (e.g., SC00 or SC01 or SC02) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *scan = (p2 == 0) ? RIG_SCAN_STOP : RIG_SCAN_VFO;
    *ch = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d p2=%d scan=%d\n", __func__, p1, p2, *scan);

    return RIG_OK;
}

/* Scan up (SC1;) */
int ftx1_scan_up(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC1;");
    return newcat_set_cmd(rig);
}

/* Scan down (SC2;) */
int ftx1_scan_down(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC2;");
    return newcat_set_cmd(rig);
}

/* Stop scan (SC0;) */
int ftx1_scan_stop(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC0;");
    return newcat_set_cmd(rig);
}

/* Set Band (BS P1P2;) */
int ftx1_set_band(RIG *rig, vfo_t vfo, int band_code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B) ? 1 : 0;

    if (band_code < 0 || band_code > FTX1_BAND_MAX) {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s band_code=%d\n", __func__,
              rig_strvfo(vfo), band_code);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BS%1d%02d;", p1, band_code);
    return newcat_set_cmd(rig);
}

/* Get Band */
int ftx1_get_band(RIG *rig, vfo_t vfo, int *band_code)
{
    int ret, p1, band;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    p1 = (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B) ? 1 : 0;
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BS%1d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%02d", &p1, &band) != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *band_code = band;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: band=%d\n", __func__, *band_code);

    return RIG_OK;
}

/* Frequency Up (UP;) */
int ftx1_freq_up(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "UP;");
    return newcat_set_cmd(rig);
}

/* Frequency Down (DN;) */
int ftx1_freq_down(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DN;");
    return newcat_set_cmd(rig);
}

/*
 * Band Up (BU P1;)
 * CAT command: BU P1; (P1=0 for MAIN VFO)
 * Cycles to the next higher amateur band
 */
int ftx1_band_up(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BU0;");
    return newcat_set_cmd(rig);
}

/*
 * Band Down (BD P1;)
 * CAT command: BD P1; (P1=0 for MAIN VFO)
 * Cycles to the next lower amateur band
 */
int ftx1_band_down(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BD0;");
    return newcat_set_cmd(rig);
}

/* Set Tuning Step (TS P1P2;) */
int ftx1_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B) ? 1 : 0;
    int step_code = -1;

    /* Find closest matching step code */
    for (int i = 0; i < FTX1_TS_COUNT; i++) {
        if (ftx1_tuning_steps[i] >= ts) {
            step_code = i;
            break;
        }
    }
    if (step_code < 0) {
        step_code = FTX1_TS_COUNT - 1;  /* Use largest step */
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s ts=%ld step_code=%d\n", __func__,
              rig_strvfo(vfo), (long)ts, step_code);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TS%1d%02d;", p1, step_code);
    return newcat_set_cmd(rig);
}

/* Get Tuning Step */
int ftx1_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    int ret, p1, step_code;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    p1 = (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B) ? 1 : 0;
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TS%1d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%02d", &p1, &step_code) != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    if (step_code >= 0 && step_code < FTX1_TS_COUNT) {
        *ts = ftx1_tuning_steps[step_code];
    } else {
        *ts = 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: step_code=%d ts=%ld\n", __func__,
              step_code, (long)*ts);

    return RIG_OK;
}

/* VFO operation wrapper for UP/DN */
int ftx1_vfo_op_scan(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    (void)vfo;

    switch (op) {
        case RIG_OP_UP:
            return ftx1_freq_up(rig);
        case RIG_OP_DOWN:
            return ftx1_freq_down(rig);
        case RIG_OP_BAND_UP:
            return ftx1_band_up(rig);
        case RIG_OP_BAND_DOWN:
            return ftx1_band_down(rig);
        default:
            return -RIG_EINVAL;
    }
}

/*
 * ftx1_zero_in - Zero In (ZI P1;)
 * CAT command: ZI P1; (P1=VFO 0/1, set-only)
 * Clears the RIT/XIT offset for the specified VFO
 */
int ftx1_zero_in(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ZI%d;", p1);
    return newcat_set_cmd(rig);
}

/*
 * ===========================================================================
 * Mode-specific Tuning Step functions using EX0306
 * Implementation by Jeremy Miller (KO4SSD) - Hamlib PR #1826
 *
 * The FTX-1 has mode-specific dial step settings accessed via the EX0306
 * extended menu command. This provides finer control than the basic TS command.
 *
 * EX0306 Settings:
 *   01 = SSB/CW DIAL STEP:  0=5Hz, 1=10Hz, 2=20Hz
 *   02 = RTTY/PSK DIAL STEP: 0=5Hz, 1=10Hz, 2=20Hz
 *   03 = FM DIAL STEP: 0=5kHz, 1=6.25kHz, 2=10kHz, 3=12.5kHz, 4=20kHz, 5=25kHz, 6=Auto
 * ===========================================================================
 */

/* EX0306 setting numbers by mode category */
#define FTX1_EX0306_SSB_CW   1
#define FTX1_EX0306_RTTY_PSK 2
#define FTX1_EX0306_FM       3

/*
 * ftx1_get_ts_ex - Get mode-specific tuning step via EX0306
 *
 * Implementation by Jeremy Miller (KO4SSD) - PR #1826
 * Queries the appropriate EX0306 setting based on current mode.
 */
int ftx1_get_ts_ex(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int err, setting_num, step_value;
    rmode_t mode;
    pbwidth_t width;
    char response[32];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !ts)
    {
        return -RIG_EINVAL;
    }

    /* Get current mode to determine which setting to query */
    if (RIG_OK != (err = rig_get_mode(rig, vfo, &mode, &width)))
    {
        return err;
    }

    /* Determine setting number based on mode (Jeremy's logic) */
    if (mode == RIG_MODE_FM || mode == RIG_MODE_FMN || mode == RIG_MODE_PKTFM)
    {
        setting_num = FTX1_EX0306_FM;
    }
    else if (mode == RIG_MODE_RTTY || mode == RIG_MODE_RTTYR ||
             mode == RIG_MODE_PKTLSB || mode == RIG_MODE_PKTUSB)
    {
        setting_num = FTX1_EX0306_RTTY_PSK;
    }
    else
    {
        setting_num = FTX1_EX0306_SSB_CW;
    }

    /* Query: EX0306[setting_num]; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX0306%02d;", setting_num);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    /* Parse response like "EX0306012;" (setting 01, value 2) */
    strncpy(response, priv->ret_data, sizeof(response) - 1);
    response[sizeof(response) - 1] = '\0';

    if (strlen(response) >= 9 && strncmp(response, "EX0306", 6) == 0)
    {
        /* Extract step value - last digit before semicolon */
        int len = strlen(response);
        if (len >= 9)
        {
            step_value = response[len - 2] - '0';
        }
        else
        {
            step_value = 1;  /* Default */
        }

        /* Convert step value to frequency based on mode (Jeremy's mapping) */
        if (setting_num == FTX1_EX0306_FM)
        {
            /* FM: 0=5kHz, 1=6.25kHz, 2=10kHz, 3=12.5kHz, 4=20kHz, 5=25kHz, 6=Auto */
            switch (step_value)
            {
                case 0: *ts = 5000; break;
                case 1: *ts = 6250; break;
                case 2: *ts = 10000; break;
                case 3: *ts = 12500; break;
                case 4: *ts = 20000; break;
                case 5: *ts = 25000; break;
                case 6: *ts = 0; break;  /* Auto */
                default: *ts = 10000; break;
            }
        }
        else
        {
            /* SSB/CW and RTTY/PSK: 0=5Hz, 1=10Hz, 2=20Hz */
            switch (step_value)
            {
                case 0: *ts = 5; break;
                case 1: *ts = 10; break;
                case 2: *ts = 20; break;
                default: *ts = 10; break;
            }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: response=%s setting=%d step=%d ts=%ld\n",
                  __func__, response, setting_num, step_value, (long)*ts);

        return RIG_OK;
    }

    /* Parsing failed, return default */
    *ts = 10;
    return RIG_OK;
}

/*
 * ftx1_set_ts_ex - Set mode-specific tuning step via EX0306
 *
 * Implementation by Jeremy Miller (KO4SSD) - PR #1826
 * Sets the appropriate EX0306 setting based on current mode.
 */
int ftx1_set_ts_ex(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int err, setting_num, step_value;
    rmode_t mode;
    pbwidth_t width;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called with ts=%ld\n", __func__, (long)ts);

    /* Get current mode to determine which setting to modify */
    if (RIG_OK != (err = rig_get_mode(rig, vfo, &mode, &width)))
    {
        return err;
    }

    /* Determine setting and convert ts to step value (Jeremy's mapping) */
    if (mode == RIG_MODE_FM || mode == RIG_MODE_FMN || mode == RIG_MODE_PKTFM)
    {
        setting_num = FTX1_EX0306_FM;
        /* FM: convert frequency to step value */
        if (ts == 0) step_value = 6;       /* Auto */
        else if (ts <= 5000) step_value = 0;
        else if (ts <= 6250) step_value = 1;
        else if (ts <= 10000) step_value = 2;
        else if (ts <= 12500) step_value = 3;
        else if (ts <= 20000) step_value = 4;
        else step_value = 5;
    }
    else if (mode == RIG_MODE_RTTY || mode == RIG_MODE_RTTYR ||
             mode == RIG_MODE_PKTLSB || mode == RIG_MODE_PKTUSB)
    {
        setting_num = FTX1_EX0306_RTTY_PSK;
        /* RTTY/PSK: 0=5Hz, 1=10Hz, 2=20Hz */
        if (ts <= 5) step_value = 0;
        else if (ts <= 10) step_value = 1;
        else step_value = 2;
    }
    else
    {
        setting_num = FTX1_EX0306_SSB_CW;
        /* SSB/CW: 0=5Hz, 1=10Hz, 2=20Hz */
        if (ts <= 5) step_value = 0;
        else if (ts <= 10) step_value = 1;
        else step_value = 2;
    }

    /* Set: EX0306[setting_num][step_value]; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX0306%02d%d;",
             setting_num, step_value);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s (setting=%d step=%d)\n",
              __func__, priv->cmd_str, setting_num, step_value);

    return newcat_set_cmd(rig);
}
