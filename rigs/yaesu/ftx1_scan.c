/*
 * Hamlib Yaesu backend - FTX-1 Scan and Band Operations Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for scanning and band selection.
 *
 * CAT Commands in this file:
 *   SC P1;           - Scan Control (0=stop, 1=up, 2=down)
 *   BS P1P2;         - Band Select (P1P2=band code)
 *   UP;              - Frequency/Channel Up
 *   DN;              - Frequency/Channel Down
 *   TS P1P2;         - Tuning Step (00-14 code for step size)
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

/* Set Scan mode (SC P1;) */
int ftx1_set_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;

    (void)vfo;
    (void)ch;

    switch (scan) {
        case RIG_SCAN_STOP:
            p1 = 0;
            break;
        case RIG_SCAN_MEM:
        case RIG_SCAN_VFO:
            p1 = 1;  /* Scan up */
            break;
        default:
            p1 = 0;
            break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: scan=%d p1=%d\n", __func__, scan, p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC%1d;", p1);
    return newcat_set_cmd(rig);
}

/* Get Scan status */
int ftx1_get_scan(RIG *rig, vfo_t vfo, scan_t *scan, int *ch)
{
    int ret, p1;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *scan = (p1 == 0) ? RIG_SCAN_STOP : RIG_SCAN_VFO;
    *ch = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d scan=%d\n", __func__, p1, *scan);

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
            /* Not directly supported, could cycle bands */
            return -RIG_ENIMPL;
        case RIG_OP_BAND_DOWN:
            return -RIG_ENIMPL;
        default:
            return -RIG_EINVAL;
    }
}

/* Helper macro for VFO conversion */
#define FTX1_VFO_TO_P1(vfo) \
    ((vfo == RIG_VFO_CURR || vfo == RIG_VFO_MAIN || vfo == RIG_VFO_A) ? 0 : 1)

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
