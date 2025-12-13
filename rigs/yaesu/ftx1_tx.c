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
 *   BI P1;           - Break-In (QSK) (0=off, 1=semi, 2=full)
 *   PS P1;           - Power Switch (0=off, 1=on) - USE WITH CAUTION
 *   SQ P1 P2P3P4;    - Squelch Level (P1=VFO, P2-P4=000-100)
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

/* Set Antenna Tuner (AC P1 P2 P3;) - SPA-1 REQUIRED */
int ftx1_set_tuner(RIG *rig, int mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, mode);

    /*
     * GUARDRAIL: Internal tuner requires SPA-1 amplifier
     * Without SPA-1, the AC command will fail or produce undefined behavior.
     */
    if (!ftx1_has_spa1())
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: internal tuner requires SPA-1 amplifier (not detected)\n",
                  __func__);
        return -RIG_ENAVAIL;  /* Feature not available */
    }

    /* mode: 0=off, 1=on, 2=tune */
    switch (mode)
    {
    case 0:  /* Tuner off */
        p1 = 0;
        p2 = 0;
        break;

    case 1:  /* Tuner on (no tune) */
        p1 = 1;
        p2 = 0;
        break;

    case 2:  /* Start tune (turns on tuner and starts tuning) */
        p1 = 1;
        p2 = 1;
        break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC%d%d0;", p1, p2);
    return newcat_set_cmd(rig);
}

/* Get Antenna Tuner status - SPA-1 REQUIRED */
int ftx1_get_tuner(RIG *rig, int *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /*
     * GUARDRAIL: Internal tuner requires SPA-1 amplifier
     * Without SPA-1, report tuner as off rather than querying.
     */
    if (!ftx1_has_spa1())
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: no SPA-1 detected, tuner not available\n", __func__);
        *mode = 0;  /* Report tuner as off */
        return RIG_OK;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse first digit (tuner on/off) from AC P1 P2 P3; */
    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Return 0=off, 1=on */
    *mode = p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, *mode);

    return RIG_OK;
}

/* Set Break-In mode (BI P1;) */
int ftx1_set_breakin(RIG *rig, int mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    /* mode: 0=off, 1=semi, 2=full */
    if (mode < 0 || mode > 2)
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
    int p1 = (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B || vfo == RIG_VFO_CURR) ? 0 : 0;
    int level = (int)(val * 100);

    /* Handle VFO_CURR as MAIN */
    if (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B)
    {
        p1 = 1;
    }

    if (level < 0) level = 0;
    if (level > 100) level = 100;

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

    p1 = (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B) ? 1 : 0;

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

    *val = (float)level / 100.0f;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%d val=%f\n", __func__, level, *val);

    return RIG_OK;
}

/* Set Speech Processor (PR P1 P2;) */
int ftx1_set_processor(RIG *rig, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p2 = status ? 1 : 0;  /* 0=off, 1=on, 2=on2 */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, status);

    /* PR P1 P2; where P1=VFO (0=Main), P2=mode */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR0%d;", p2);
    return newcat_set_cmd(rig);
}

/* Get Speech Processor status */
int ftx1_get_processor(RIG *rig, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse PR P1 P2; response */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Return 1 if processor is on (p2 = 1 or 2) */
    *status = (p2 > 0) ? 1 : 0;

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
