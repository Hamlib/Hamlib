/*
 * Hamlib Yaesu backend - FTX-1 Transmit Control Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for transmit control, PTT, and TX settings.
 *
 * CAT Commands in this file:
 *   TX P1;           - Transmit (P1: 0=RX, 1=TX CAT, 2=TX Data)
 *   PC P1P2P3;       - Power Control (005-100 watts) [also in ftx1_audio.c]
 *   MX P1;           - Monitor TX Audio (0=off, 1=on)
 *   VX P1;           - VOX on/off (0=off, 1=on)
 *   AC P1P2P3;       - Antenna Tuner Control (P1: 0=off, 1=on, 2=tune)
 *   BI P1;           - Break-In on/off (0=off, 1=on) - type via EX020115
 *   PS P1;           - Power Switch (0=off, 1=on) - USE WITH CAUTION
 *   SQ P1 P2P3P4;    - Squelch Level (P1=VFO, P2-P4=000-255)
 *   TS P1;           - TXW (TX Watch) (0=off, 1=on)
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/* Set PTT (TX P1;) */
int ftx1_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;

    (void)vfo;  /* Not used for TX command */

    switch (ptt)
    {
    case RIG_PTT_OFF:
        p1 = 0;
        break;

    case RIG_PTT_ON:
    case RIG_PTT_ON_MIC:
        p1 = 1;  /* TX via CAT */
        break;

    case RIG_PTT_ON_DATA:
        p1 = 2;  /* TX via Data */
        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d p1=%d\n", __func__, ptt, p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TX%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get PTT status */
int ftx1_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TX;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    switch (p1)
    {
    case 0:
        *ptt = RIG_PTT_OFF;
        break;

    case 1:
        *ptt = RIG_PTT_ON;
        break;

    case 2:
        *ptt = RIG_PTT_ON_DATA;
        break;

    default:
        *ptt = RIG_PTT_OFF;
        break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d ptt=%d\n", __func__, p1, *ptt);

    return RIG_OK;
}

/* Set VOX on/off (VX P1;) */
int ftx1_set_vox(RIG *rig, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = status ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VX%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get VOX status (VX P1;) */
int ftx1_get_vox(RIG *rig, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VX;");

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

/* Set TX Monitor (MX P1;) */
int ftx1_set_monitor(RIG *rig, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = status ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MX%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get TX Monitor status (MX P1;) */
int ftx1_get_monitor(RIG *rig, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MX;");

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
 * FTX-1 AC command format: AC P1 P2 P3;
 *   P1 = Tuner on/off (0=off, 1=on)
 *   P2 = Tune start (0=no action, 1=start tune) - write only
 *   P3 = Unknown
 *
 * Response format: AC P1 P2 P3; (3 digits)
 * Set format for tuner on/off: AC P1 0 0; (turn on/off without starting tune)
 * Set format for tune: AC 1 1 0; (turn on and start tune)
 *
 * IMPORTANT: The internal antenna tuner is ONLY available when the SPA-1
 * amplifier is connected (FTX-1 Optima configuration). When using the field
 * head only, this command will return an error from the radio.
 */

/*
 * Set Antenna Tuner - SPA-1 REQUIRED
 *
 * The AC command is STATUS-ONLY on FTX-1 (set returns '?').
 * Use GEN_TUNER_SELECT (EX030104) to control tuner:
 *   0 = OFF (bypass), 1 = INT, 2 = EXT, 3 = ATAS
 *
 * mode: 0=off (bypass), 1=on (internal tuner)
 * Note: mode=2 (start tune) is not supported via CAT.
 */
int ftx1_set_tuner(RIG *rig, int mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int tuner_select;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, mode);

    /*
     * GUARDRAIL: Internal tuner requires SPA-1 amplifier
     */
    if (!ftx1_has_spa1(rig))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: internal tuner requires SPA-1 amplifier (not detected)\n",
                  __func__);
        return -RIG_ENAVAIL;
    }

    /* mode: 0=off, 1=on, 2=tune (not supported) */
    switch (mode)
    {
    case 0:  /* Tuner off (bypass) */
        tuner_select = 0;  /* EX030104 = 0 (OFF) */
        break;

    case 1:  /* Tuner on (internal) */
        tuner_select = 1;  /* EX030104 = 1 (INT) */
        break;

    case 2:  /* Start tune - not supported via CAT */
        rig_debug(RIG_DEBUG_WARN,
                  "%s: start tune (mode=2) not supported via CAT\n", __func__);
        return -RIG_ENIMPL;

    default:
        return -RIG_EINVAL;
    }

    /* Use EX030104 (GEN_TUNER_SELECT) instead of AC command */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030104%d;", tuner_select);
    return newcat_set_cmd(rig);
}

/*
 * Get Antenna Tuner status - SPA-1 REQUIRED
 *
 * Uses GEN_TUNER_SELECT (EX030104) to determine if tuner is enabled.
 * Returns: 0=off/bypass, 1=on (INT or other tuner selected)
 */
int ftx1_get_tuner(RIG *rig, int *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, tuner_select;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /*
     * GUARDRAIL: Internal tuner requires SPA-1 amplifier
     */
    if (!ftx1_has_spa1(rig))
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: no SPA-1 detected, tuner not available\n", __func__);
        *mode = 0;
        return RIG_OK;
    }

    /* Query GEN_TUNER_SELECT (EX030104) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030104;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse value from EX030104N; (N = 0-3) */
    if (sscanf(priv->ret_data + 8, "%1d", &tuner_select) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* 0=OFF(bypass), 1=INT, 2=EXT, 3=ATAS - report 0 as off, others as on */
    *mode = (tuner_select > 0) ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d (tuner_select=%d)\n",
              __func__, *mode, tuner_select);

    return RIG_OK;
}

/* Set Break-In on/off (BI P1;) - type (semi/full) is EX020115 */
int ftx1_set_breakin(RIG *rig, int mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    /* Spec: BI P1 only allows 0=off, 1=on; type is EX020115 */
    if (mode < 0 || mode > 1)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, mode);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BI%d;", mode);
    return newcat_set_cmd(rig);
}

/* Get Break-In mode */
int ftx1_get_breakin(RIG *rig, int *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BI;");

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

/* Set Squelch Level (SQ P1 P2P3P4;) */
int ftx1_set_squelch(RIG *rig, vfo_t vfo, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = (int)(val * 255);  /* Spec: SQ P2 range is 000-255 */

    if (level < 0) level = 0;
    if (level > 255) level = 255;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d level=%d\n", __func__,
              rig_strvfo(vfo), p1, level);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SQ%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/* Get Squelch Level */
int ftx1_get_squelch(RIG *rig, vfo_t vfo, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1, level;

    p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SQ%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / 255.0f;  /* Spec: SQ P2 range is 000-255 */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%d val=%f\n", __func__, level, *val);

    return RIG_OK;
}

/*
 * Set Speech Processor (PR P1 P2;)
 * Spec: P1=VFO (0=MAIN, 1=SUB), P2=mode (1=OFF, 2=ON)
 */
int ftx1_set_processor(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p2 = status ? 2 : 1;  /* Spec: P2 1=OFF, 2=ON */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s status=%d\n", __func__,
              rig_strvfo(vfo), status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR%d%d;", p1, p2);
    return newcat_set_cmd(rig);
}

/*
 * Get Speech Processor status (PR P1;)
 * Spec: P1=VFO (0=MAIN, 1=SUB), response P2=mode (1=OFF, 2=ON)
 */
int ftx1_get_processor(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1_resp, p2;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse PR P1 P2; response */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1_resp, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Spec: P2 1=OFF, 2=ON - return 1 if p2==2 */
    *status = (p2 == 2) ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p2=%d status=%d\n", __func__, p2, *status);

    return RIG_OK;
}

/* Power Switch (PS P1;) - USE WITH CAUTION */
int ftx1_set_powerstat(RIG *rig, powerstat_t status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = (status == RIG_POWER_ON) ? 1 : 0;

    rig_debug(RIG_DEBUG_WARN, "%s: Setting power to %d\n", __func__, p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PS%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get Power status */
int ftx1_get_powerstat(RIG *rig, powerstat_t *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PS;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (p1 == 1) ? RIG_POWER_ON : RIG_POWER_OFF;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d status=%d\n", __func__, p1, *status);

    return RIG_OK;
}

/*
 * =============================================================================
 * TS Command: TXW (TX Watch)
 * =============================================================================
 * CAT format: TS P1;
 *   P1 = 0: TXW off
 *   P1 = 1: TXW on (monitor SUB band during TX)
 *
 * Read response: TS0 or TS1
 * Set command: TS0; or TS1;
 *
 * When TXW is enabled, the radio continues to monitor the SUB band
 * while transmitting on the MAIN band. Useful for monitoring a
 * frequency while calling CQ.
 */

/*
 * ftx1_set_tx_watch - Set TXW (TX Watch) mode
 *
 * status: 0=off, 1=on
 */
int ftx1_set_tx_watch(RIG *rig, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TS%d;", status ? 1 : 0);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_tx_watch - Get TXW (TX Watch) status
 *
 * Returns: 0=off, 1=on
 */
int ftx1_get_tx_watch(RIG *rig, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TS;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: TS0 or TS1 */
    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, *status);

    return RIG_OK;
}

/*
 * =============================================================================
 * RI Command: Read Radio Information (for DCD/Squelch status)
 * =============================================================================
 * CAT format: RI;
 * Response: RI P1 P2 P3 P4 P5 P6 P7 P8 P9 P10 P11 P12 P13;
 *
 * P8 = SQL (Squelch) status:
 *   0 = SQL Closed (no signal / below threshold)
 *   1 = SQL Open (BUSY - signal present / above threshold)
 *
 * This is used for DCD (Data Carrier Detect) functionality.
 */

/*
 * ftx1_get_dcd - Get DCD (squelch) status via RI command P8
 *
 * Returns RIG_DCD_ON when squelch is open (signal present),
 * RIG_DCD_OFF when squelch is closed (no signal).
 */
int ftx1_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2, p3, p4, p5, p6, p7, p8;
    int vfo_num;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    /* Determine VFO number for RI command */
    switch (vfo)
    {
    case RIG_VFO_B:
    case RIG_VFO_SUB:
        vfo_num = 1;
        break;

    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_CURR:
    default:
        vfo_num = 0;
        break;
    }

    /* RI command requires VFO parameter: RI0; or RI1; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RI%d;", vfo_num);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /*
     * Response format: RIP1P2P3P4P5P6P7P8;
     * VFO is not echoed in response. Parse 8 status digits starting at position 2.
     * P8 (8th digit) is squelch status: 0=closed, 1=open/BUSY.
     */
    if (sscanf(priv->ret_data + 2, "%1d%1d%1d%1d%1d%1d%1d%1d",
               &p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8) != 8)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* P8: 0=SQL Closed (no signal), 1=SQL Open (BUSY/signal present) */
    *dcd = (p8 == 1) ? RIG_DCD_ON : RIG_DCD_OFF;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: P8(SQL)=%d dcd=%s\n",
              __func__, p8, (*dcd == RIG_DCD_ON) ? "ON" : "OFF");

    return RIG_OK;
}
