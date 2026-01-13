/*
 * Hamlib Yaesu backend - FTX-1 Preamp and Attenuator Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for RF preamp (PA) and attenuator (RA).
 *
 * CAT Commands in this file:
 *   PA P1 P2;        - Preamp (P1=band type, P2=preamp code)
 *                      P1: 0=HF/50MHz, 1=VHF (144MHz), 2=UHF (430MHz)
 *                      P2 (HF/50): 0=IPO (bypass), 1=AMP1, 2=AMP2
 *                      P2 (VHF/UHF): 0=OFF, 1=ON
 *   RA P1 P2;        - Attenuator (P1=0 fixed, P2=0 off/1 on)
 *                      Provides 12dB fixed attenuation when enabled
 */

#include <stdlib.h>
#include <stdio.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"

/* Preamp codes from CAT manual page 21 */
/* PA P1 P2; P1=band type, P2=preamp setting */
/* P1: 0=HF/50MHz, 1=VHF (144MHz), 2=UHF (430MHz) */
/* P2 when P1=0: 0=IPO, 1=AMP1, 2=AMP2 */
/* P2 when P1=1 or 2: 0=OFF, 1=ON */
#define FTX1_BAND_HF50   0
#define FTX1_BAND_VHF    1
#define FTX1_BAND_UHF    2

#define FTX1_PREAMP_IPO  0
#define FTX1_PREAMP_AMP1 1
#define FTX1_PREAMP_AMP2 2

/* RA P1 P2; P1=0 (fixed), P2=0 OFF, 1 ON */
#define FTX1_ATT_OFF 0
#define FTX1_ATT_ON 1

/*
 * ftx1_get_band_type - Helper to determine band type from frequency
 * Returns: 0=HF/50MHz, 1=VHF (144MHz), 2=UHF (430MHz)
 */
static int ftx1_get_band_type(RIG *rig, vfo_t vfo)
{
    freq_t freq = 0;
    int ret;

    /* Try to get current frequency to determine band type */
    if (rig->caps->get_freq)
    {
        ret = rig->caps->get_freq(rig, vfo, &freq);
        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: get_freq failed (%s), defaulting to HF/50MHz band\n",
                      __func__, rigerror(ret));
            return FTX1_BAND_HF50;
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: no get_freq capability, defaulting to HF/50MHz band\n",
                  __func__);
    }

    if (freq >= 420000000) return FTX1_BAND_UHF;   /* 430 MHz band */
    if (freq >= 144000000) return FTX1_BAND_VHF;   /* 144 MHz band */
    return FTX1_BAND_HF50;  /* HF and 50 MHz */
}

/*
 * ftx1_set_preamp_helper - Set Preamp
 * CAT command: PA P1 P2; (P1=band type, P2=preamp code)
 *
 * For HF/50: P2: 0=IPO, 1=AMP1, 2=AMP2
 * For VHF/UHF: P2: 0=OFF, 1=ON
 */
int ftx1_set_preamp_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int band_type = ftx1_get_band_type(rig, vfo);
    int preamp_code;

    if (band_type == FTX1_BAND_HF50)
    {
        /* HF/50: val.i: 0=IPO, 10=AMP1, 20=AMP2 */
        preamp_code = val.i / 10;
        if (preamp_code > FTX1_PREAMP_AMP2) preamp_code = FTX1_PREAMP_AMP2;
    }
    else
    {
        /* VHF/UHF: val.i: 0=OFF, anything else=ON */
        preamp_code = (val.i > 0) ? 1 : 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: band_type=%d preamp_code=%d val.i=%d\n",
              __func__, band_type, preamp_code, val.i);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PA%d%d;", band_type, preamp_code);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_preamp_helper - Get Preamp
 * CAT command: PA P1; Response: PA P1 P2;
 */
int ftx1_get_preamp_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int band_type = ftx1_get_band_type(rig, vfo);
    int p1, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: band_type=%d\n", __func__, band_type);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PA%d;", band_type);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse response: PA P1 P2; (P1=band type, P2=preamp code) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    if (band_type == FTX1_BAND_HF50)
    {
        /* HF/50: P2 0=IPO, 1=AMP1, 2=AMP2 -> val.i 0, 10, 20 */
        val->i = p2 * 10;
    }
    else
    {
        /* VHF/UHF: P2 0=OFF, 1=ON -> val.i 0, 10 */
        val->i = p2 * 10;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d p2=%d val->i=%d\n", __func__,
              p1, p2, val->i);

    return RIG_OK;
}

/*
 * ftx1_set_att_helper - Set Attenuator
 * CAT command: RA P1 P2; (P1=0 fixed, P2=0 OFF/1 ON)
 */
int ftx1_set_att_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int att_on = (val.i > 0) ? FTX1_ATT_ON : FTX1_ATT_OFF;

    (void)vfo;  /* Unused - FTX-1 RA doesn't use VFO parameter */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val.i=%d att_on=%d\n", __func__,
              val.i, att_on);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RA0%d;", att_on);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_att_helper - Get Attenuator
 * CAT command: RA0; Response: RA0 P2; (P2=0 OFF/1 ON)
 */
int ftx1_get_att_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2;

    (void)vfo;  /* Unused - FTX-1 RA doesn't use VFO parameter */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RA0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse response: RA0 P2; (P1=0, P2=att state) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->i = (p2 == FTX1_ATT_ON) ? 12 : 0;  /* 12dB attenuator */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d p2=%d val->i=%d\n", __func__,
              p1, p2, val->i);

    return RIG_OK;
}
