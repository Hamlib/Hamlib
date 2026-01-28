/*
 * Hamlib Yaesu backend - FTX-1 Scan and Band Operations Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for scanning and band selection.
 *
 * Mode-specific tuning steps via EX0306:
 *   - SSB/CW mode: 5Hz, 10Hz, 20Hz steps (EX030601)
 *   - RTTY/PSK mode: 5Hz, 10Hz, 20Hz steps (EX030602)
 *   - FM mode: 5kHz to 25kHz steps + Auto (EX030603)
 *
 * CAT Commands in this file:
 *   SC P1;           - Scan Control (0=stop, 1=up, 2=down)
 *   BS P1P2;         - Band Select (P1P2=band code)
 *   BU P1;           - Band Up (P1=VFO, cycles to higher band)
 *   BD P1;           - Band Down (P1=VFO, cycles to lower band)
 *   UP;              - Frequency/Channel Up
 *   DN;              - Frequency/Channel Down
 *   TS P1P2;         - Tuning Step (00-14 code for step size)
 *   EX0306xx;        - Mode-specific Dial Step
 *   ZI P1;           - Zero In (P1=VFO 0/1, set-only)
 *
 * Band Codes (BS command) per FTX-1 CAT spec:
 *   00 = 160m (1.8MHz)     07 = 15m (21MHz)
 *   01 = 80m (3.5MHz)      08 = 12m (24.5MHz)
 *   02 = 60m (5MHz)        09 = 10m (28MHz)
 *   03 = 40m (7MHz)        10 = 6m (50MHz)
 *   04 = 30m (10MHz)       11 = GEN (70MHz/general)
 *   05 = 20m (14MHz)       12 = AIR (airband)
 *   06 = 17m (18MHz)       13 = 2m (144MHz)
 *                          14 = 70cm (430MHz)
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
#include "ftx1.h"

/* Band codes per FTX-1 CAT spec */
#define FTX1_BAND_160M   0
#define FTX1_BAND_80M    1
#define FTX1_BAND_60M    2
#define FTX1_BAND_40M    3
#define FTX1_BAND_30M    4
#define FTX1_BAND_20M    5
#define FTX1_BAND_17M    6
#define FTX1_BAND_15M    7
#define FTX1_BAND_12M    8
#define FTX1_BAND_10M    9
#define FTX1_BAND_6M     10
#define FTX1_BAND_GEN    11
#define FTX1_BAND_AIR    12
#define FTX1_BAND_2M     13
#define FTX1_BAND_70CM   14
#define FTX1_BAND_MAX    14

/*
 * Note: Tuning step (TS command) is NOT supported on FTX-1 - returns '?'.
 * Use EX0306 (DIAL_SSB_CW_STEP) menu for mode-specific dial step instead.
 * See ftx1_set_ts() and ftx1_get_ts() which delegate to EX0306 menu.
 */

/* Set Scan mode (SC P1 P2;) - P1=VFO, P2=scan mode */
int ftx1_set_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;  /* VFO: 0=MAIN, 1=SUB */
    int p2;  /* Scan mode: 0=OFF, 1=UP, 2=DOWN */

    (void)ch;

    p1 = ftx1_vfo_to_p1(rig, vfo);

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

    p1 = ftx1_vfo_to_p1(rig, vfo);
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
    int p1;

    if (band_code < 0 || band_code > FTX1_BAND_MAX) {
        return -RIG_EINVAL;
    }

    /* Resolve currVFO to actual VFO */
    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_NONE) {
        vfo = STATE(rig)->current_vfo;
    }

    p1 = (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B) ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d band_code=%d\n", __func__,
              rig_strvfo(vfo), p1, band_code);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BS%1d%02d;", p1, band_code);
    return newcat_set_cmd(rig);
}

/*
 * Get Band - BS command is set-only, so derive band from frequency
 */
int ftx1_get_band(RIG *rig, vfo_t vfo, int *band_code)
{
    int ret;
    freq_t freq;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    /* Get current frequency for the VFO */
    ret = rig_get_freq(rig, vfo, &freq);
    if (ret != RIG_OK) return ret;

    /* Determine band from frequency */
    if (freq >= 1800000 && freq < 2000000) {
        *band_code = FTX1_BAND_160M;
    } else if (freq >= 3500000 && freq < 4000000) {
        *band_code = FTX1_BAND_80M;
    } else if (freq >= 5000000 && freq < 5500000) {
        *band_code = FTX1_BAND_60M;
    } else if (freq >= 7000000 && freq < 7300000) {
        *band_code = FTX1_BAND_40M;
    } else if (freq >= 10100000 && freq < 10150000) {
        *band_code = FTX1_BAND_30M;
    } else if (freq >= 14000000 && freq < 14350000) {
        *band_code = FTX1_BAND_20M;
    } else if (freq >= 18068000 && freq < 18168000) {
        *band_code = FTX1_BAND_17M;
    } else if (freq >= 21000000 && freq < 21450000) {
        *band_code = FTX1_BAND_15M;
    } else if (freq >= 24890000 && freq < 24990000) {
        *band_code = FTX1_BAND_12M;
    } else if (freq >= 28000000 && freq < 29700000) {
        *band_code = FTX1_BAND_10M;
    } else if (freq >= 50000000 && freq < 54000000) {
        *band_code = FTX1_BAND_6M;
    } else if (freq >= 70000000 && freq < 76000000) {
        *band_code = FTX1_BAND_GEN;
    } else if (freq >= 118000000 && freq < 137000000) {
        *band_code = FTX1_BAND_AIR;
    } else if (freq >= 144000000 && freq < 148000000) {
        *band_code = FTX1_BAND_2M;
    } else if (freq >= 430000000 && freq < 450000000) {
        *band_code = FTX1_BAND_70CM;
    } else {
        /* Default to GEN for out-of-band frequencies */
        *band_code = FTX1_BAND_GEN;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%.0f band=%d\n", __func__, freq, *band_code);

    return RIG_OK;
}

/*
 * Convert Hamlib hamlib_band_t enum to FTX-1 band code
 * Returns -1 if band not supported by FTX-1
 */
static int hamlib_band_to_ftx1(hamlib_band_t band)
{
    switch (band) {
        case RIG_BAND_160M:   return FTX1_BAND_160M;
        case RIG_BAND_80M:    return FTX1_BAND_80M;
        case RIG_BAND_60M:    return FTX1_BAND_60M;
        case RIG_BAND_40M:    return FTX1_BAND_40M;
        case RIG_BAND_30M:    return FTX1_BAND_30M;
        case RIG_BAND_20M:    return FTX1_BAND_20M;
        case RIG_BAND_17M:    return FTX1_BAND_17M;
        case RIG_BAND_15M:    return FTX1_BAND_15M;
        case RIG_BAND_12M:    return FTX1_BAND_12M;
        case RIG_BAND_10M:    return FTX1_BAND_10M;
        case RIG_BAND_6M:     return FTX1_BAND_6M;
        case RIG_BAND_GEN:    return FTX1_BAND_GEN;
        case RIG_BAND_AIR:    return FTX1_BAND_AIR;
        case RIG_BAND_144MHZ: return FTX1_BAND_2M;
        case RIG_BAND_430MHZ: return FTX1_BAND_70CM;
        default:              return -1;
    }
}

/*
 * Convert FTX-1 band code to Hamlib hamlib_band_t enum
 * Returns -1 if band code invalid
 */
static hamlib_band_t ftx1_band_to_hamlib(int band_code)
{
    switch (band_code) {
        case FTX1_BAND_160M:  return RIG_BAND_160M;
        case FTX1_BAND_80M:   return RIG_BAND_80M;
        case FTX1_BAND_60M:   return RIG_BAND_60M;
        case FTX1_BAND_40M:   return RIG_BAND_40M;
        case FTX1_BAND_30M:   return RIG_BAND_30M;
        case FTX1_BAND_20M:   return RIG_BAND_20M;
        case FTX1_BAND_17M:   return RIG_BAND_17M;
        case FTX1_BAND_15M:   return RIG_BAND_15M;
        case FTX1_BAND_12M:   return RIG_BAND_12M;
        case FTX1_BAND_10M:   return RIG_BAND_10M;
        case FTX1_BAND_6M:    return RIG_BAND_6M;
        case FTX1_BAND_GEN:   return RIG_BAND_GEN;
        case FTX1_BAND_AIR:   return RIG_BAND_AIR;
        case FTX1_BAND_2M:    return RIG_BAND_144MHZ;
        case FTX1_BAND_70CM:  return RIG_BAND_430MHZ;
        default:              return -1;
    }
}

/* Set band select level (RIG_LEVEL_BAND_SELECT) */
int ftx1_set_band_select(RIG *rig, vfo_t vfo, int band)
{
    int ftx1_band;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s band=%d\n", __func__,
              rig_strvfo(vfo), band);

    ftx1_band = hamlib_band_to_ftx1((hamlib_band_t)band);
    if (ftx1_band < 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported band %d\n", __func__, band);
        return -RIG_EINVAL;
    }

    return ftx1_set_band(rig, vfo, ftx1_band);
}

/* Get band select level (RIG_LEVEL_BAND_SELECT) */
int ftx1_get_band_select(RIG *rig, vfo_t vfo, int *band)
{
    int ret, ftx1_band;
    hamlib_band_t hamlib_band;

    ret = ftx1_get_band(rig, vfo, &ftx1_band);
    if (ret != RIG_OK) return ret;

    hamlib_band = ftx1_band_to_hamlib(ftx1_band);
    if (hamlib_band < 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown FTX-1 band code %d\n", __func__, ftx1_band);
        return -RIG_EPROTO;
    }

    *band = (int)hamlib_band;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ftx1_band=%d hamlib_band=%d\n", __func__,
              ftx1_band, *band);

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

/* Forward declarations for EX0306 functions */
static int ftx1_get_ts_ex(RIG *rig, vfo_t vfo, shortfreq_t *ts);
static int ftx1_set_ts_ex(RIG *rig, vfo_t vfo, shortfreq_t ts);

/*
 * Set Tuning Step - uses mode-specific EX0306 menu
 *
 * Note: The TS command returns '?' on FTX-1. Instead, we use the EX0306
 * extended menu which provides mode-specific dial step settings:
 *   EX030601 = SSB/CW dial step (0=5Hz, 1=10Hz, 2=20Hz)
 *   EX030602 = RTTY/PSK dial step (0=5Hz, 1=10Hz, 2=20Hz)
 *   EX030603 = FM dial step (0=5kHz to 5=25kHz, 6=Auto)
 */
int ftx1_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s ts=%ld\n", __func__,
              rig_strvfo(vfo), (long)ts);
    return ftx1_set_ts_ex(rig, vfo, ts);
}

/*
 * Get Tuning Step - uses mode-specific EX0306 menu
 */
int ftx1_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));
    return ftx1_get_ts_ex(rig, vfo, ts);
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
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ZI%d;", p1);
    return newcat_set_cmd(rig);
}

/*
 * ===========================================================================
 * Mode-specific Tuning Step functions using EX0306
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
 * Queries the appropriate EX0306 setting based on current mode.
 */
static int ftx1_get_ts_ex(RIG *rig, vfo_t vfo, shortfreq_t *ts)
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

    /* Determine setting number based on mode */
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

        /* Convert step value to frequency based on mode */
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
 * Sets the appropriate EX0306 setting based on current mode.
 */
static int ftx1_set_ts_ex(RIG *rig, vfo_t vfo, shortfreq_t ts)
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

    /* Determine setting and convert ts to step value */
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
