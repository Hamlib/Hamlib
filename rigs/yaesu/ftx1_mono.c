/*
 * Hamlib Yaesu backend - FTX-1 (Monolithic version)
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Monolithic FTX-1 backend combining all source files.
 * This file is auto-generated from the multi-file implementation.
 * To regenerate: ./gen_ftx1_mono.sh
 *
 * FIRMWARE NOTES (v1.08+):
 * - RT (RIT on/off) and XT (XIT on/off) commands return '?' - not implemented
 * - CF (Clarifier) sets offset value only, does not enable/disable clarifier
 * - ZI (Zero In) only works in CW mode (MD03 or MD07)
 * - BS (Band Select) is set-only - no read/query capability
 * - MR/MT/MZ (Memory) use 5-digit format, not 4-digit as documented
 * - MC (Memory Channel) uses 6-digit format
 * - CH command only accepts CH0/CH1 (not CH;)
 * - VM mode codes: 00=VFO, 11=Memory (not 01 as documented)
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "sprintflst.h"
#include "yaesu.h"
#include "newcat.h"

/* ========== Shared constants from ftx1.h ========== */
#define NC_RIGID_FTX1               0x840   /* Default rig model ID */
#define FTX1_RADIO_ID               "0840"  /* Radio ID (same for all configs) */

/*
 * FTX-1 head type constants
 * Power ranges by head type:
 *   FIELD_BATTERY: 0.5W - 6W (0.5W steps)
 *   FIELD_12V:     0.5W - 10W (0.5W steps)
 *   SPA1:          5W - 100W (1W steps) - "Optima" when attached to Field
 */
#define FTX1_HEAD_UNKNOWN       0
#define FTX1_HEAD_FIELD_BATTERY 1   /* Field Head on battery (0.5-6W) */
#define FTX1_HEAD_FIELD_12V     2   /* Field Head on 12V (0.5-10W) */
#define FTX1_HEAD_SPA1          3   /* Optima/SPA-1 amplifier (5-100W) */

/* Legacy alias for backwards compatibility */
#define FTX1_HEAD_FIELD FTX1_HEAD_FIELD_12V

/*
 * FTX1_VFO_TO_P1 - Convert Hamlib VFO to FTX-1 P1 parameter
 * Returns: 0 for MAIN/VFO-A/CURR, 1 for SUB/VFO-B
 */
#define FTX1_VFO_TO_P1(vfo) \
    ((vfo == RIG_VFO_CURR || vfo == RIG_VFO_MAIN || vfo == RIG_VFO_A) ? 0 : 1)

/* FTX-1 CTCSS mode values for CT command */
#define FTX1_CTCSS_MODE_OFF  0   /* CT0: CTCSS/DCS off */
#define FTX1_CTCSS_MODE_ENC  1   /* CT1: CTCSS encode only (TX tone) */
#define FTX1_CTCSS_MODE_TSQ  2   /* CT2: Tone squelch (TX+RX tone) */
#define FTX1_CTCSS_MODE_DCS  3   /* CT3: DCS mode */

/* ========== Shared private data ========== */
static struct {
    int head_type;
    int spa1_detected;
    int detection_done;
} ftx1_priv = { FTX1_HEAD_UNKNOWN, 0, 0 };

static const struct newcat_priv_caps ftx1_priv_caps = { .roofing_filter_count = 0 };

/* SPA-1 detection functions */
static int ftx1_has_spa1(void) {
    return ftx1_priv.spa1_detected || ftx1_priv.head_type == FTX1_HEAD_SPA1;
}

static int ftx1_get_head_type(void) {
    return ftx1_priv.head_type;
}


/* ========== FROM ftx1_vfo.c ========== */


int ftx1_set_vfo(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        p1 = 0;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        p1 = 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS%d;", p1);

    return newcat_set_cmd(rig);
}

int ftx1_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS;");

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: VS0 or VS1
    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *vfo = (p1 == 0) ? RIG_VFO_MAIN : RIG_VFO_SUB;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(*vfo));

    return RIG_OK;
}

int ftx1_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: split=%d tx_vfo=%s\n", __func__,
              split, rig_strvfo(tx_vfo));

    // Set split on/off
    p1 = (split == RIG_SPLIT_ON) ? 1 : 0;
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ST%d;", p1);

    ret = newcat_set_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Set TX VFO if split is on
    if (split == RIG_SPLIT_ON)
    {
        p1 = (tx_vfo == RIG_VFO_SUB || tx_vfo == RIG_VFO_B) ? 1 : 0;
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FT%d;", p1);
        ret = newcat_set_cmd(rig);
    }

    return ret;
}

int ftx1_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    // Get split status
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ST;");

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse split '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *split = (p1 == 1) ? RIG_SPLIT_ON : RIG_SPLIT_OFF;

    // Get TX VFO
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FT;");

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse tx vfo '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *tx_vfo = (p1 == 1) ? RIG_VFO_SUB : RIG_VFO_MAIN;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: split=%d tx_vfo=%s\n", __func__,
              *split, rig_strvfo(*tx_vfo));

    return RIG_OK;
}

int ftx1_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: op=%d\n", __func__, op);

    switch (op)
    {
    case RIG_OP_CPY:
        /* AB: Copy VFO-A (MAIN) to VFO-B (SUB) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AB;");
        break;

    case RIG_OP_XCHG:
        /* SV: Swap VFO A and B */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SV;");
        break;

    case RIG_OP_BAND_UP:
        /* BU: Band Up (P1=0 for MAIN) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BU0;");
        break;

    case RIG_OP_BAND_DOWN:
        /* BD: Band Down (P1=0 for MAIN) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BD0;");
        break;

    case RIG_OP_FROM_VFO:
        /* BA: Copy VFO-B (SUB) to VFO-A (MAIN) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BA;");
        break;

    case RIG_OP_TUNE:
        /* AC110: Tuner start (turns on tuner and starts tune) */
        /* Format: AC P1 P2 P3; P1=1 (on), P2=1 (start tune), P3=0 (MAIN) */
        /* Note: This causes transmission! */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC110;");
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %d\n", __func__, op);
        return -RIG_EINVAL;
    }

    return newcat_set_cmd(rig);
}


/* Dual receive mode values */
#define FTX1_FR_DUAL_RECEIVE    0   /* FR00: Both VFOs can receive */
#define FTX1_FR_SINGLE_RECEIVE  1   /* FR01: Only selected VFO receives */

int ftx1_set_dual_receive(RIG *rig, int dual)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: dual=%d\n", __func__, dual);

    /* FR00 = dual receive, FR01 = single receive */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FR%02d;",
             dual ? FTX1_FR_DUAL_RECEIVE : FTX1_FR_SINGLE_RECEIVE);

    return newcat_set_cmd(rig);
}

int ftx1_get_dual_receive(RIG *rig, int *dual)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FR;");

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: FR00 or FR01 */
    if (sscanf(priv->ret_data + 2, "%02d", &p1p2) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* 00 = dual (return 1), 01 = single (return 0) */
    *dual = (p1p2 == FTX1_FR_DUAL_RECEIVE) ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: dual=%d\n", __func__, *dual);

    return RIG_OK;
}

/* ========== FROM ftx1_freq.c ========== */


// FTX-1 uses 9-digit Hz format for frequency
#define FTX1_FREQ_DIGITS 9

int ftx1_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char cmd;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    // Determine command based on VFO
    // MAIN/A/CURR -> FA, SUB/B -> FB
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_CURR:
        cmd = 'A';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        cmd = 'B';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "F%c%09.0f;", cmd, freq);

    return newcat_set_cmd(rig);
}

int ftx1_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char cmd;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    // Determine command based on VFO
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_CURR:
        cmd = 'A';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        cmd = 'B';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "F%c;", cmd);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: FA000014074; - parse 9 digits after "Fx"
    // priv->ret_data contains response without trailing ;
    if (strlen(priv->ret_data) < 2 + FTX1_FREQ_DIGITS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: short response '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *freq = atof(priv->ret_data + 2);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%.0f\n", __func__, *freq);

    return RIG_OK;
}


/* Repeater shift mode values */
#define FTX1_OS_SIMPLEX  0   /* Simplex (no shift) */
#define FTX1_OS_PLUS     1   /* Plus shift (+) */
#define FTX1_OS_MINUS    2   /* Minus shift (-) */
#define FTX1_OS_ARS      3   /* Automatic Repeater Shift */

int ftx1_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s shift=%d\n",
              __func__, rig_strvfo(vfo), shift);

    /* Determine VFO */
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_CURR:
        p1 = 0;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        p1 = 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    /* Convert Hamlib shift to FTX-1 code */
    switch (shift)
    {
    case RIG_RPT_SHIFT_NONE:
        p2 = FTX1_OS_SIMPLEX;
        break;

    case RIG_RPT_SHIFT_PLUS:
        p2 = FTX1_OS_PLUS;
        break;

    case RIG_RPT_SHIFT_MINUS:
        p2 = FTX1_OS_MINUS;
        break;

    default:
        /* Map unknown shifts to simplex */
        p2 = FTX1_OS_SIMPLEX;
        break;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "OS%d%d;", p1, p2);

    return newcat_set_cmd(rig);
}

int ftx1_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    /* Determine VFO */
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_CURR:
        p1 = 0;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        p1 = 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "OS%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: OS P1 P2 */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Convert FTX-1 code to Hamlib shift */
    switch (p2)
    {
    case FTX1_OS_SIMPLEX:
        *shift = RIG_RPT_SHIFT_NONE;
        break;

    case FTX1_OS_PLUS:
        *shift = RIG_RPT_SHIFT_PLUS;
        break;

    case FTX1_OS_MINUS:
        *shift = RIG_RPT_SHIFT_MINUS;
        break;

    case FTX1_OS_ARS:
        /* ARS maps to none for Hamlib */
        *shift = RIG_RPT_SHIFT_NONE;
        break;

    default:
        *shift = RIG_RPT_SHIFT_NONE;
        break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: shift=%d\n", __func__, *shift);

    return RIG_OK;
}


/* ========== FROM ftx1_preamp.c ========== */


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

/* ========== FROM ftx1_audio.c ========== */


/* Level ranges */
#define FTX1_AF_GAIN_MIN 0
#define FTX1_AF_GAIN_MAX 255
#define FTX1_RF_GAIN_MIN 0
#define FTX1_RF_GAIN_MAX 255
#define FTX1_MIC_GAIN_MIN 0
#define FTX1_MIC_GAIN_MAX 100

#define FTX1_POWER_MIN_FIELD 0.5f         /* Field head minimum (both modes) */
#define FTX1_POWER_MAX_FIELD_BATTERY 6.0f /* Field head on battery maximum */
#define FTX1_POWER_MAX_FIELD_12V 10.0f    /* Field head on 12V maximum */
#define FTX1_POWER_MIN_SPA1 5             /* Optima/SPA-1 minimum */
#define FTX1_POWER_MAX_SPA1 100           /* Optima/SPA-1 maximum */

/* Set AF Gain (AG P1 P2P3P4;) */
int ftx1_set_af_gain(RIG *rig, vfo_t vfo, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int level = (int)(val * FTX1_AF_GAIN_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d val=%f\n", __func__,
              rig_strvfo(vfo), p1, val);

    if (level < FTX1_AF_GAIN_MIN) level = FTX1_AF_GAIN_MIN;
    if (level > FTX1_AF_GAIN_MAX) level = FTX1_AF_GAIN_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AG%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/* Get AF Gain */
int ftx1_get_af_gain(RIG *rig, vfo_t vfo, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AG%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / FTX1_AF_GAIN_MAX;
    return RIG_OK;
}

/* Set RF Gain (RG P1 P2P3P4;) */
int ftx1_set_rf_gain(RIG *rig, vfo_t vfo, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int level = (int)(val * FTX1_RF_GAIN_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d val=%f\n", __func__,
              rig_strvfo(vfo), p1, val);

    if (level < FTX1_RF_GAIN_MIN) level = FTX1_RF_GAIN_MIN;
    if (level > FTX1_RF_GAIN_MAX) level = FTX1_RF_GAIN_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RG%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/* Get RF Gain */
int ftx1_get_rf_gain(RIG *rig, vfo_t vfo, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RG%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / FTX1_RF_GAIN_MAX;
    return RIG_OK;
}

/* Set Mic Gain (MG P1P2P3;) */
int ftx1_set_mic_gain(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int level = (int)(val * FTX1_MIC_GAIN_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    if (level < FTX1_MIC_GAIN_MIN) level = FTX1_MIC_GAIN_MIN;
    if (level > FTX1_MIC_GAIN_MAX) level = FTX1_MIC_GAIN_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MG%03d;", level);
    return newcat_set_cmd(rig);
}

/* Get Mic Gain */
int ftx1_get_mic_gain(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MG;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%3d", &level) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / FTX1_MIC_GAIN_MAX;
    return RIG_OK;
}

/* Get S-Meter (SM P1;) */
int ftx1_get_smeter(RIG *rig, vfo_t vfo, int *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SM%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: SM P1 P2P3P4P5; (4 digits) */
    if (sscanf(priv->ret_data + 2, "%1d%4d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = level;
    return RIG_OK;
}

/* Get Meter (RM P1;) - various meters */
int ftx1_get_meter(RIG *rig, int meter_type, int *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: meter_type=%d\n", __func__, meter_type);

    /* meter_type: 1=COMP, 2=ALC, 3=PO, 4=SWR, 5=ID, 6=VDD */
    if (meter_type < 1 || meter_type > 6)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM%d;", meter_type);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: RM P1 P2P3P4; */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = level;
    return RIG_OK;
}

int ftx1_set_power(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int head_type;
    float watts;
    float max_power;
    int watts_int;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    /*
     * Use pre-detected head type from ftx1_open() for better performance
     * and to ensure consistency with SPA-1 guardrails.
     */
    head_type = ftx1_get_head_type();

    if (head_type == FTX1_HEAD_UNKNOWN)
    {
        /* Fallback: Query current PC to detect head type if not already known */
        rig_debug(RIG_DEBUG_VERBOSE, "%s: head type unknown, querying PC\n", __func__);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
        ret = newcat_get_cmd(rig);
        if (ret == RIG_OK && strlen(priv->ret_data) >= 3)
        {
            if (strncmp(priv->ret_data, "PC2", 3) == 0)
            {
                head_type = FTX1_HEAD_SPA1;
            }
            else
            {
                head_type = FTX1_HEAD_FIELD_12V;  /* Default to 12V if unknown */
            }
        }
        if (head_type == FTX1_HEAD_UNKNOWN)
        {
            head_type = FTX1_HEAD_FIELD_12V;  /* Default to field head 12V */
        }
        /* Small delay after query */
        hl_usleep(50 * 1000);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: using head_type=%d\n", __func__, head_type);

    if (head_type == FTX1_HEAD_SPA1)
    {
        /* Optima/SPA-1: 5-100W, always 3-digit zero-padded whole watts */
        watts_int = (int)(val * FTX1_POWER_MAX_SPA1);
        if (watts_int < FTX1_POWER_MIN_SPA1) watts_int = FTX1_POWER_MIN_SPA1;
        if (watts_int > FTX1_POWER_MAX_SPA1) watts_int = FTX1_POWER_MAX_SPA1;
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC2%03d;", watts_int);
    }
    else
    {
        /*
         * Field head power ranges:
         *   FIELD_BATTERY: 0.5-6W
         *   FIELD_12V:     0.5-10W
         */
        if (head_type == FTX1_HEAD_FIELD_BATTERY)
        {
            max_power = FTX1_POWER_MAX_FIELD_BATTERY;
        }
        else
        {
            max_power = FTX1_POWER_MAX_FIELD_12V;
        }

        watts = val * max_power;
        if (watts < FTX1_POWER_MIN_FIELD) watts = FTX1_POWER_MIN_FIELD;
        if (watts > max_power) watts = max_power;

        if (watts == (float)(int)watts)
        {
            /* Whole watts: use 3-digit zero-padded format */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC1%03d;", (int)watts);
        }
        else
        {
            /* Fractional watts: use decimal format */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC1%.1f;", watts);
        }
    }

    return newcat_set_cmd(rig);
}

int ftx1_get_power(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1;
    int head_type;
    float watts;
    float max_power;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: PC P1 P2... where P2 can be 3-digit or decimal */
    /* Try to parse P1 first (always 1 digit) */
    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse P1 from '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Now parse the power value - could be decimal (0.5, 1.5, 5.1) or integer (001, 005, 010) */
    if (sscanf(priv->ret_data + 3, "%f", &watts) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse watts from '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d watts=%.1f\n", __func__, p1, watts);

    /* Get detected head type for proper scaling */
    head_type = ftx1_get_head_type();

    /* Convert watts to 0.0-1.0 range based on head type */
    if (p1 == 2)
    {
        /* Optima/SPA-1: 5-100W maps to 0.05-1.0 */
        *val = watts / (float)FTX1_POWER_MAX_SPA1;
    }
    else if (p1 == 1)
    {
        /*
         * Field head: scale based on detected power source
         *   FIELD_BATTERY: 0.5-6W
         *   FIELD_12V:     0.5-10W
         */
        if (head_type == FTX1_HEAD_FIELD_BATTERY)
        {
            max_power = FTX1_POWER_MAX_FIELD_BATTERY;
        }
        else
        {
            max_power = FTX1_POWER_MAX_FIELD_12V;
        }
        *val = watts / max_power;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected P1=%d\n", __func__, p1);
        *val = watts / FTX1_POWER_MAX_FIELD_12V;  /* Default to field head 12V */
    }

    return RIG_OK;
}

/* Set VOX Gain (VG P1P2P3;) */
int ftx1_set_vox_gain(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int level = (int)(val * 100);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    if (level < 0) level = 0;
    if (level > 100) level = 100;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VG%03d;", level);
    return newcat_set_cmd(rig);
}

/* Get VOX Gain */
int ftx1_get_vox_gain(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VG;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%3d", &level) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / 100.0f;
    return RIG_OK;
}

int ftx1_set_vox_delay(RIG *rig, int tenths)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tenths=%d\n", __func__, tenths);

    if (tenths < 0) tenths = 0;
    if (tenths > 30) tenths = 30;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD%02d;", tenths);
    return newcat_set_cmd(rig);
}

/* Get VOX Delay */
int ftx1_get_vox_delay(RIG *rig, int *tenths)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int delay;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: VD00 to VD30 (2 digits) */
    if (sscanf(priv->ret_data + 2, "%2d", &delay) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *tenths = delay;
    return RIG_OK;
}

int ftx1_set_monitor_level(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int level = (int)(val * 100);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    if (level < 0) level = 0;
    if (level > 100) level = 100;

    /* FTX-1 format: ML P1 P2P3P4; where P1=VFO (0=MAIN) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML0%03d;", level);
    return newcat_set_cmd(rig);
}

int ftx1_get_monitor_level(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query MAIN VFO: ML0; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: ML0100 (P1=0, level=100) */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / 100.0f;
    return RIG_OK;
}

int ftx1_set_agc(RIG *rig, int val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%d\n", __func__, val);

    /* val is AGC setting code (0-4 for FTX-1) */
    /* Format: GT P1 P2; where P1=VFO (0=MAIN), P2=mode */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT0%d;", val);
    return newcat_set_cmd(rig);
}

/* Get AGC */
int ftx1_get_agc(RIG *rig, int *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, agc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query MAIN VFO AGC: GT0; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: GT0X where X is AGC mode (0-6, 4-6 are AUTO variants) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &agc) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = agc;
    return RIG_OK;
}

int ftx1_set_amc_output(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int level = (int)(val * 100);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    if (level < 0) level = 0;
    if (level > 100) level = 100;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AO%03d;", level);
    return newcat_set_cmd(rig);
}

/* Get AMC Output Level */
int ftx1_get_amc_output(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AO;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: AO P1P2P3; (3 digits 000-100) */
    if (sscanf(priv->ret_data + 2, "%3d", &level) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / 100.0f;
    return RIG_OK;
}

int ftx1_set_width(RIG *rig, vfo_t vfo, int width_code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s width_code=%d\n", __func__,
              rig_strvfo(vfo), width_code);

    if (width_code < 0) width_code = 0;
    if (width_code > 23) width_code = 23;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH%d0%02d;", p1, width_code);
    return newcat_set_cmd(rig);
}

/* Get Filter Width */
int ftx1_get_width(RIG *rig, vfo_t vfo, int *width_code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, width;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: SH P1 P2 P3P4; (P1=VFO, P2=0, P3P4=width code) */
    if (sscanf(priv->ret_data + 2, "%1d%1d%2d", &p1_resp, &p2, &width) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *width_code = width;
    return RIG_OK;
}

int ftx1_set_meter_switch(RIG *rig, int meter_type)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: meter_type=%d\n", __func__, meter_type);

    if (meter_type < 0) meter_type = 0;
    if (meter_type > 5) meter_type = 5;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MS0%d;", meter_type);
    return newcat_set_cmd(rig);
}

/* Get Meter Switch */
int ftx1_get_meter_switch(RIG *rig, int *meter_type)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, mt;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MS;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MS P1P2; */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &mt) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *meter_type = mt;
    return RIG_OK;
}


/* Voice message channel values */
#define FTX1_PB_STOP      0   /* Stop playback */
#define FTX1_PB_CHANNEL_1 1   /* Channel 1 */
#define FTX1_PB_CHANNEL_2 2   /* Channel 2 */
#define FTX1_PB_CHANNEL_3 3   /* Channel 3 */
#define FTX1_PB_CHANNEL_4 4   /* Channel 4 */
#define FTX1_PB_CHANNEL_5 5   /* Channel 5 */

int ftx1_play_voice_msg(RIG *rig, int channel)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: channel=%d\n", __func__, channel);

    if (channel < 0 || channel > 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid channel %d (must be 0-5)\n",
                  __func__, channel);
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PB0%d;", channel);

    return newcat_set_cmd(rig);
}

int ftx1_get_voice_msg_status(RIG *rig, int *channel)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PB0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: PB0n where n is channel (0-5) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *channel = p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: channel=%d\n", __func__, *channel);

    return RIG_OK;
}

int ftx1_stop_voice_msg(RIG *rig)
{
    return ftx1_play_voice_msg(rig, FTX1_PB_STOP);
}

/* ========== FROM ftx1_filter.c ========== */


int ftx1_set_anf_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BC%d%d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

int ftx1_get_anf_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BC%d;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: BC01 (VFO=0, status=1)
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1_resp, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = p2;

    return RIG_OK;
}

int ftx1_set_mn_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d0%03d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

int ftx1_get_mn_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d0;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: BP00001 (VFO=0, P2=0, P3=001 ON)
    if (sscanf(priv->ret_data + 2, "%1d%1d%3d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (p3 == 1) ? 1 : 0;

    return RIG_OK;
}

int ftx1_set_apf_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d2%04d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

int ftx1_get_apf_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d2;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: CO020001 (VFO=0, P2=2, P3=0001 ON)
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (p3 == 1) ? 1 : 0;

    return RIG_OK;
}

int ftx1_set_notchf_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d freq=%d\n", __func__,
              rig_strvfo(vfo), p1, val.i);

    // Convert Hz to command units (x10)
    p3 = val.i / 10;

    if (p3 < 1)
    {
        p3 = 1;
    }

    if (p3 > 320)
    {
        p3 = 320;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d1%03d;", p1, p3);

    return newcat_set_cmd(rig);
}

int ftx1_get_notchf_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d1;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: BP01320 (VFO=0, P2=1, P3=320)
    if (sscanf(priv->ret_data + 2, "%1d%1d%3d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->i = p3 * 10;  // Convert to Hz

    return RIG_OK;
}

int ftx1_set_apf_level_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d level=%f\n", __func__,
              rig_strvfo(vfo), p1, val.f);

    // val.f is 0.0-1.0, map to 0-50
    p3 = (int)(val.f * 50);

    if (p3 < 0)
    {
        p3 = 0;
    }

    if (p3 > 50)
    {
        p3 = 50;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d3%04d;", p1, p3);

    return newcat_set_cmd(rig);
}

int ftx1_get_apf_level_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d3;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: CO030025 (VFO=0, P2=3, P3=0025)
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->f = (float)p3 / 50.0f;  // Convert 0-50 to 0.0-1.0

    return RIG_OK;
}

int ftx1_set_filter_number(RIG *rig, vfo_t vfo, int filter)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d filter=%d\n", __func__,
              rig_strvfo(vfo), p1, filter);

    if (filter < 1 || filter > 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid filter number %d\n", __func__, filter);
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FN%d%d;", p1, filter);

    return newcat_set_cmd(rig);
}

int ftx1_get_filter_number(RIG *rig, vfo_t vfo, int *filter)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FN%d;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: FN01 (VFO=0, filter=1) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1_resp, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *filter = p2;

    return RIG_OK;
}

int ftx1_set_contour(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d0%04d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

int ftx1_get_contour(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d0;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: CO000001 (VFO=0, P2=0, P3=0001 ON) */
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (p3 == 1) ? 1 : 0;

    return RIG_OK;
}

int ftx1_set_contour_freq(RIG *rig, vfo_t vfo, int freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d freq=%d\n", __func__,
              rig_strvfo(vfo), p1, freq);

    if (freq < 10) freq = 10;
    if (freq > 3200) freq = 3200;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d1%04d;", p1, freq);

    return newcat_set_cmd(rig);
}

int ftx1_get_contour_freq(RIG *rig, vfo_t vfo, int *freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d1;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: CO011000 (VFO=0, P2=1, P3=1000 Hz) */
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *freq = p3;

    return RIG_OK;
}

/* ========== FROM ftx1_noise.c ========== */


/* Level ranges */
#define FTX1_NB_LEVEL_MIN 0
#define FTX1_NB_LEVEL_MAX 10   /* 0=OFF, 1-10=level */
#define FTX1_NR_LEVEL_MIN 0
#define FTX1_NR_LEVEL_MAX 15   /* 0=OFF, 1-15=level */

int ftx1_set_nb_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int level = status ? 5 : 0;  /* Default to mid-level when turning on */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

int ftx1_get_nb_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: NL P1 P2P3P4; (VFO + 3-digit level 000-010) */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (level > 0) ? 1 : 0;
    return RIG_OK;
}

int ftx1_set_nr_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int level = status ? 8 : 0;  /* Default to mid-level when turning on */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d%02d;", p1, level);
    return newcat_set_cmd(rig);
}

int ftx1_get_nr_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: RL P1 P2P3; (VFO + 2-digit level 00-15) */
    if (sscanf(priv->ret_data + 2, "%1d%2d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (level > 0) ? 1 : 0;
    return RIG_OK;
}

int ftx1_set_nb_level_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int level = (int)(val.f * FTX1_NB_LEVEL_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d level=%d\n", __func__,
              rig_strvfo(vfo), p1, level);

    if (level < FTX1_NB_LEVEL_MIN) level = FTX1_NB_LEVEL_MIN;
    if (level > FTX1_NB_LEVEL_MAX) level = FTX1_NB_LEVEL_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

int ftx1_get_nb_level_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: NL P1 P2P3P4; (3-digit level 000-010) */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->f = (float)level / FTX1_NB_LEVEL_MAX;
    return RIG_OK;
}

int ftx1_set_nr_level_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int level = (int)(val.f * FTX1_NR_LEVEL_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d level=%d\n", __func__,
              rig_strvfo(vfo), p1, level);

    if (level < FTX1_NR_LEVEL_MIN) level = FTX1_NR_LEVEL_MIN;
    if (level > FTX1_NR_LEVEL_MAX) level = FTX1_NR_LEVEL_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d%02d;", p1, level);
    return newcat_set_cmd(rig);
}

int ftx1_get_nr_level_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: RL P1 P2P3; (2-digit level 00-15) */
    if (sscanf(priv->ret_data + 2, "%1d%2d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->f = (float)level / FTX1_NR_LEVEL_MAX;
    return RIG_OK;
}

int ftx1_set_na_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p2 = status ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NA%d%d;", p1, p2);
    return newcat_set_cmd(rig);
}

int ftx1_get_na_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = FTX1_VFO_TO_P1(vfo);
    int p1_resp, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NA%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: NA P1 P2; (P1=VFO, P2=on/off) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1_resp, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = p2;
    return RIG_OK;
}

/* ========== FROM ftx1_tx.c ========== */


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


int ftx1_set_tx_watch(RIG *rig, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status=%d\n", __func__, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TS%d;", status ? 1 : 0);

    return newcat_set_cmd(rig);
}

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

/* ========== FROM ftx1_cw.c ========== */


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

int ftx1_load_message(RIG *rig, int start)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = start ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: start=%d\n", __func__, start);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "LM%02d;", p1);
    return newcat_set_cmd(rig);
}

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
        msg[0] = '\0';  /* Empty slot (response is just "KM") */
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: msg='%s'\n", __func__, msg);

    return RIG_OK;
}

int ftx1_set_keyer_memory(RIG *rig, int slot, const char *msg)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char safe_msg[51];  /* Max 50 chars per spec */
    size_t len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: slot=%d msg='%s'\n", __func__, slot, msg);

    if (slot < 1 || slot > 5)
    {
        return -RIG_EINVAL;
    }

    if (msg == NULL)
    {
        msg = "";  /* Allow clearing slot with empty message */
    }

    /* Copy and truncate message to 50 chars max */
    strncpy(safe_msg, msg, 50);
    safe_msg[50] = '\0';

    /* Convert to uppercase (CW convention) */
    len = strlen(safe_msg);
    for (size_t i = 0; i < len; i++)
    {
        if (safe_msg[i] >= 'a' && safe_msg[i] <= 'z')
        {
            safe_msg[i] = safe_msg[i] - 'a' + 'A';
        }
    }

    /* KM P1 msg; - slot number followed by message */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KM%d%s;", slot, safe_msg);
    return newcat_set_cmd(rig);
}

/* ========== FROM ftx1_ctcss.c ========== */


#define FTX1_CTCSS_MIN 1
#define FTX1_CTCSS_MAX 50

/* CTCSS tone frequencies in 0.1 Hz (multiply by 10 for actual) */
static const unsigned int ftx1_ctcss_tones[] = {
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,   /* 01-10 */
    948,  974,  1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,  /* 11-20 */
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,  /* 21-30 */
    1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995, 2035, 2065,  /* 31-40 */
    2257, 2291, 2336, 2418, 2503, 2541, 0, 0, 0, 0               /* 41-50 */
};

/* Convert CTCSS frequency (in 0.1 Hz) to tone number */
static int ftx1_freq_to_tone_num(unsigned int freq)
{
    int i;

    for (i = 0; i < FTX1_CTCSS_MAX; i++)
    {
        if (ftx1_ctcss_tones[i] == freq)
        {
            return i + 1;  /* Tone numbers are 1-based */
        }
    }

    return -1;  /* Not found */
}

/* Convert tone number to frequency (in 0.1 Hz) */
static unsigned int ftx1_tone_num_to_freq(int num)
{
    if (num < FTX1_CTCSS_MIN || num > FTX1_CTCSS_MAX)
    {
        return 0;
    }

    return ftx1_ctcss_tones[num - 1];
}

int ftx1_set_ctcss_mode(RIG *rig, tone_t mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;

    /* Validate and use FTX-1 CT command values directly */
    if (mode > FTX1_CTCSS_MODE_DCS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid mode %u (must be 0-3)\n",
                  __func__, mode);
        return -RIG_EINVAL;
    }

    p1 = (int)mode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%u p1=%d\n", __func__, mode, p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT%d;", p1);
    return newcat_set_cmd(rig);
}

int ftx1_get_ctcss_mode(RIG *rig, tone_t *mode)
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

    /* Return FTX-1 mode value directly (0-3) */
    *mode = (tone_t)p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d mode=%u\n", __func__, p1, *mode);

    return RIG_OK;
}

/* Set CTCSS Tone (CN P1 P2P3;) */
int ftx1_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int tone_num;

    (void)vfo;  /* Unused */

    /* tone is in 0.1 Hz, find matching tone number */
    tone_num = ftx1_freq_to_tone_num(tone);

    if (tone_num < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: tone %u not found in table\n", __func__,
                  tone);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone=%u tone_num=%d\n", __func__, tone,
              tone_num);

    /* P1=0 for TX tone, P2P3P4 is 3-digit tone number */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN00%03d;", tone_num);
    return newcat_set_cmd(rig);
}

/* Get CTCSS Tone */
int ftx1_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1, tone_num;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* P1=0 for TX tone, need CN00; to query */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN00;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN P1 P2P3P4 (e.g., CN00012 for TX tone 12) */
    if (sscanf(priv->ret_data + 2, "%2d%3d", &p1, &tone_num) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *tone = ftx1_tone_num_to_freq(tone_num);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone_num=%d tone=%u\n", __func__,
              tone_num, *tone);

    return RIG_OK;
}

/* Set CTCSS Squelch Tone (CN P1 P2P3P4; with P1=1 for RX) */
int ftx1_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int tone_num;

    (void)vfo;  /* Unused */

    tone_num = ftx1_freq_to_tone_num(tone);

    if (tone_num < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: tone %u not found in table\n", __func__,
                  tone);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone=%u tone_num=%d\n", __func__, tone,
              tone_num);

    /* P1=1 for RX tone, P2P3P4 is 3-digit tone number */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN10%03d;", tone_num);
    return newcat_set_cmd(rig);
}

/* Get CTCSS Squelch Tone */
int ftx1_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1, tone_num;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* P1=1 for RX tone, need CN10; to query */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN10;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN P1 P2P3P4 (e.g., CN10012 for RX tone 12) */
    if (sscanf(priv->ret_data + 2, "%2d%3d", &p1, &tone_num) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *tone = ftx1_tone_num_to_freq(tone_num);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone_num=%d tone=%u\n", __func__,
              tone_num, *tone);

    return RIG_OK;
}

/* Set DCS Code (DC P1 P2P3P4;) */
int ftx1_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, code);

    /* P1=0 for TX DCS code */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC0%03u;", code);
    return newcat_set_cmd(rig);
}

/* Get DCS Code */
int ftx1_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;
    unsigned int dcs;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3u", &p1, &dcs) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *code = dcs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, *code);

    return RIG_OK;
}

/* Set DCS Squelch Code */
int ftx1_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, code);

    /* P1=1 for RX DCS code */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC1%03u;", code);
    return newcat_set_cmd(rig);
}

/* Get DCS Squelch Code */
int ftx1_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;
    unsigned int dcs;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC1;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3u", &p1, &dcs) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *code = dcs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, *code);

    return RIG_OK;
}

/* ========== FROM ftx1_mem.c ========== */


#define FTX1_MEM_MIN 1
#define FTX1_MEM_MAX 117
#define FTX1_MEM_REGULAR_MAX 99

/* Forward declaration - needed for ftx1_set_vfo_mem_mode */
static int ftx1_get_vfo_mem_mode_internal(RIG *rig, vfo_t *vfo);

int ftx1_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* VFO not used in MC command */

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range %d-%d\n",
                  __func__, ch, FTX1_MEM_MIN, FTX1_MEM_MAX);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    /* Format: MCNNNNNN (6-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%06d;", ch);
    return newcat_set_cmd(rig);
}

int ftx1_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int ret, channel;
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int vfo_num = 0;

    /* Determine VFO number: 0=MAIN, 1=SUB */
    if (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B) {
        vfo_num = 1;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%d\n", __func__, vfo_num);

    /* Format: MCx where x=VFO (0 or 1) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%d;", vfo_num);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MCNNNNNN (MC + 6-digit channel) */
    /* Skip "MC" (2 chars) and parse 6-digit channel */
    if (strlen(priv->ret_data) < 8) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    if (sscanf(priv->ret_data + 2, "%6d", &channel) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *ch = channel;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, *ch);

    return RIG_OK;
}

int ftx1_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char mode_char;
    char clar_dir;
    int clar_offset;
    int p7_vfo_mem;
    int p8_ctcss;
    int p10_shift;

    (void)vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d freq=%.0f mode=%d\n", __func__,
              chan->channel_num, chan->freq, (int)chan->mode);

    /* Validate channel number */
    if (chan->channel_num < FTX1_MEM_MIN || chan->channel_num > FTX1_MEM_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range %d-%d\n",
                  __func__, chan->channel_num, FTX1_MEM_MIN, FTX1_MEM_MAX);
        return -RIG_EINVAL;
    }

    /* Convert Hamlib mode to FTX-1 mode code */
    switch (chan->mode) {
    case RIG_MODE_LSB:      mode_char = '1'; break;
    case RIG_MODE_USB:      mode_char = '2'; break;
    case RIG_MODE_CW:       mode_char = '3'; break;  /* CW-U */
    case RIG_MODE_FM:       mode_char = '4'; break;
    case RIG_MODE_AM:       mode_char = '5'; break;
    case RIG_MODE_RTTYR:    mode_char = '6'; break;  /* RTTY-L */
    case RIG_MODE_CWR:      mode_char = '7'; break;  /* CW-L */
    case RIG_MODE_PKTLSB:   mode_char = '8'; break;  /* DATA-L */
    case RIG_MODE_RTTY:     mode_char = '9'; break;  /* RTTY-U */
    case RIG_MODE_PKTFM:    mode_char = 'A'; break;  /* DATA-FM */
    case RIG_MODE_FMN:      mode_char = 'B'; break;  /* FM-N */
    case RIG_MODE_PKTUSB:   mode_char = 'C'; break;  /* DATA-U */
    case RIG_MODE_AMN:      mode_char = 'D'; break;  /* AM-N */
    case RIG_MODE_PSK:      mode_char = 'E'; break;  /* PSK */
    default:                mode_char = '2'; break;  /* Default to USB */
    }

    /* Clarifier direction and offset */
    if (chan->rit != 0) {
        clar_dir = (chan->rit >= 0) ? '+' : '-';
        clar_offset = labs(chan->rit);
        if (clar_offset > 9990) clar_offset = 9990;
    } else if (chan->xit != 0) {
        clar_dir = (chan->xit >= 0) ? '+' : '-';
        clar_offset = labs(chan->xit);
        if (clar_offset > 9990) clar_offset = 9990;
    } else {
        clar_dir = '+';
        clar_offset = 0;
    }

    /* P7: VFO/Memory mode - default to Memory (1) when writing to channel */
    p7_vfo_mem = 1;

    /* P8: CTCSS mode from channel flags */
    p8_ctcss = 0;  /* Default OFF */
    if (chan->flags & RIG_CHFLAG_SKIP) {
        /* Use flags for tone squelch indication if available */
    }
    /* Check tone settings */
    if (chan->ctcss_tone != 0 && chan->ctcss_sql != 0) {
        p8_ctcss = 1;  /* CTCSS ENC/DEC */
    } else if (chan->ctcss_tone != 0) {
        p8_ctcss = 2;  /* CTCSS ENC only */
    } else if (chan->dcs_code != 0) {
        p8_ctcss = 3;  /* DCS */
    }

    /* P10: Repeater shift */
    switch (chan->rptr_shift) {
    case RIG_RPT_SHIFT_PLUS:  p10_shift = 1; break;
    case RIG_RPT_SHIFT_MINUS: p10_shift = 2; break;
    default:                  p10_shift = 0; break;  /* Simplex */
    }

    /* Build MW command string:
     * MW P1P1P1P1P1 P2P2P2P2P2P2P2P2P2 P3P3P3P3P3 P4 P5 P6 P7 P8 P9P9 P10;
     * Total: 2 + 5 + 9 + 5 + 1 + 1 + 1 + 1 + 1 + 2 + 1 + 1 = 30 chars
     */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
             "MW%05d%09.0f%c%04d%d%d%c%d%d00%d;",
             chan->channel_num,          /* P1: 5-digit channel */
             chan->freq,                 /* P2: 9-digit frequency in Hz */
             clar_dir,                   /* P3: clarifier direction */
             clar_offset,                /* P3: clarifier offset */
             (chan->rit != 0) ? 1 : 0,   /* P4: RX CLAR on/off */
             (chan->xit != 0) ? 1 : 0,   /* P5: TX CLAR on/off */
             mode_char,                  /* P6: mode code */
             p7_vfo_mem,                 /* P7: VFO/Memory mode */
             p8_ctcss,                   /* P8: CTCSS mode */
                                         /* P9: "00" (fixed) */
             p10_shift);                 /* P10: shift */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd='%s'\n", __func__, priv->cmd_str);

    return newcat_set_cmd(rig);
}

int ftx1_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int ret;
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ch;

    (void)vfo;
    (void)read_only;

    ch = chan->channel_num;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range %d-%d\n",
                  __func__, ch, FTX1_MEM_MIN, FTX1_MEM_MAX);
        return -RIG_EINVAL;
    }

    /* Format: MR P1P2P2P2P2 (bank 0 + 4-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MR%05d;", ch);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MR00001FFFFFFFFF+OOOOOPPMMxxxx */
    /* Skip "MR" + 5-digit channel (7 chars), then 9-digit frequency */
    if (strlen(priv->ret_data) < 16) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Parse frequency (9 digits starting at position 7) */
    freq_t freq;
    if (sscanf(priv->ret_data + 7, "%9lf", &freq) == 1) {
        chan->freq = freq;
    }

    /* TODO: Parse mode, offset, and other parameters from response */

    return RIG_OK;
}

/* Memory to VFO-A (MA;) */
int ftx1_mem_to_vfo(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MA;");
    return newcat_set_cmd(rig);
}

int ftx1_get_mem_name(RIG *rig, int ch, char *name, size_t name_len)
{
    int ret;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Format: MT P1P2P2P2P2 (bank 0 + 4-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MT%05d;", ch);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MT00001[12-char name] */
    /* Skip "MT" + 5-digit channel (7 chars), then 12-char name */
    if (strlen(priv->ret_data) < 7) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Extract name (12 chars starting at position 7) */
    size_t copy_len = (name_len - 1 < 12) ? name_len - 1 : 12;
    strncpy(name, priv->ret_data + 7, copy_len);
    name[copy_len] = '\0';

    /* Trim trailing spaces */
    size_t len = strlen(name);
    while (len > 0 && name[len - 1] == ' ') {
        name[--len] = '\0';
    }

    return RIG_OK;
}

int ftx1_set_mem_name(RIG *rig, int ch, const char *name)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char padded_name[13];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d name='%s'\n", __func__, ch, name);

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Pad name to 12 chars with spaces */
    memset(padded_name, ' ', 12);
    padded_name[12] = '\0';
    size_t name_len = strlen(name);
    if (name_len > 12) name_len = 12;
    memcpy(padded_name, name, name_len);

    /* Format: MT P1P2P2P2P2 NAME (bank 0 + 4-digit channel + 12-char name) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MT%05d%s;", ch, padded_name);

    return newcat_set_cmd(rig);
}

int ftx1_quick_mem_store(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s (NOTE: QI is non-functional in firmware)\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QI;");
    return newcat_set_cmd(rig);
}

int ftx1_quick_mem_recall(RIG *rig, int slot)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: slot=%d (NOTE: QR is non-functional in firmware)\n", __func__, slot);

    /* CAT manual shows QR; with no parameters, but we accept slot for API compatibility */
    (void)slot;  /* unused - firmware ignores parameters anyway */

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QR;");
    return newcat_set_cmd(rig);
}

int ftx1_set_vfo_mem_mode(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    if (vfo == RIG_VFO_MEM) {
        /*
         * Memory mode requested - use SV toggle
         * First check current mode, then toggle if needed
         */
        int ret;
        vfo_t current_vfo;

        ret = ftx1_get_vfo_mem_mode_internal(rig, &current_vfo);
        if (ret != RIG_OK) return ret;

        if (current_vfo != RIG_VFO_MEM) {
            /* Toggle to memory mode using SV */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SV;");
            return newcat_set_cmd(rig);
        }
        return RIG_OK;  /* Already in memory mode */
    } else {
        /* VFO mode requested - VM000 works */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM000;");
        return newcat_set_cmd(rig);
    }
}

static int ftx1_get_vfo_mem_mode_internal(RIG *rig, vfo_t *vfo)
{
    int ret, mode;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query MAIN VFO mode: VM0 returns VM0PP */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: VM0PP where PP is 2-digit mode code */
    if (strlen(priv->ret_data) < 5) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Parse 2-digit mode code at position 3 */
    if (sscanf(priv->ret_data + 3, "%2d", &mode) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Mode codes: 00=VFO, 11=Memory (firmware differs from spec) */
    *vfo = (mode == 11) ? RIG_VFO_MEM : RIG_VFO_CURR;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d vfo=%s\n", __func__, mode,
              rig_strvfo(*vfo));

    return RIG_OK;
}

/* Public wrapper for ftx1_get_vfo_mem_mode */
int ftx1_get_vfo_mem_mode(RIG *rig, vfo_t *vfo)
{
    return ftx1_get_vfo_mem_mode_internal(rig, vfo);
}

int ftx1_vfo_a_to_mem(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AM;");
    return newcat_set_cmd(rig);
}

int ftx1_vfo_b_to_mem(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BM;");
    return newcat_set_cmd(rig);
}

int ftx1_mem_to_vfo_b(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MB;");
    return newcat_set_cmd(rig);
}

int ftx1_set_mem_zone(RIG *rig, int channel, const char *zone_data)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char padded_data[11];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d data='%s'\n", __func__,
              channel, zone_data);

    if (channel < FTX1_MEM_MIN || channel > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Pad/truncate zone data to 10 chars */
    memset(padded_data, '0', 10);
    padded_data[10] = '\0';
    size_t data_len = strlen(zone_data);
    if (data_len > 10) data_len = 10;
    memcpy(padded_data, zone_data, data_len);

    /* Format: MZ P1P2P2P2P2 DATA (bank 0 + 4-digit channel + 10-digit data) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MZ%05d%s;",
             channel, padded_data);

    return newcat_set_cmd(rig);
}

int ftx1_get_mem_zone(RIG *rig, int channel, char *zone_data, size_t data_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, channel);

    if (channel < FTX1_MEM_MIN || channel > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Format: MZ P1P2P2P2P2 (bank 0 + 4-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MZ%05d;", channel);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MZ00001[10-digit zone data] */
    /* Skip "MZ" + 5-digit channel (7 chars), then 10-digit data */
    if (strlen(priv->ret_data) < 17) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Extract zone data (10 chars starting at position 7) */
    size_t copy_len = (data_len - 1 < 10) ? data_len - 1 : 10;
    strncpy(zone_data, priv->ret_data + 7, copy_len);
    zone_data[copy_len] = '\0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s: zone_data='%s'\n", __func__, zone_data);

    return RIG_OK;
}

int ftx1_mem_ch_up(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CH0;");
    return newcat_set_cmd(rig);
}

int ftx1_mem_ch_down(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CH1;");
    return newcat_set_cmd(rig);
}

int ftx1_get_mem_group(RIG *rig, int *group)
{
    int ret, ch;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Use MC0 to get current memory channel */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MCGGNNNN where GG=group (00-05), NNNN=channel */
    if (strlen(priv->ret_data) < 8) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Parse group (2 digits at position 2) and channel (4 digits at position 4) */
    if (sscanf(priv->ret_data + 2, "%2d%4d", group, &ch) != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: group=%d channel=%d\n", __func__, *group, ch);

    return RIG_OK;
}

/* ========== FROM ftx1_scan.c ========== */


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

int ftx1_zero_in(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = FTX1_VFO_TO_P1(vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ZI%d;", p1);
    return newcat_set_cmd(rig);
}

/* ========== FROM ftx1_info.c ========== */


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

int ftx1_set_date(RIG *rig, int year, int month, int day)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: year=%d month=%d day=%d\n", __func__,
              year, month, day);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT0%04d%02d%02d;",
             year, month, day);

    return newcat_set_cmd(rig);
}

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

int ftx1_set_time(RIG *rig, int hour, int min, int sec)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: hour=%d min=%d sec=%d\n", __func__,
              hour, min, sec);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT1%02d%02d%02d;",
             hour, min, sec);

    return newcat_set_cmd(rig);
}

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


int ftx1_set_gp_out(RIG *rig, int out_a, int out_b, int out_c, int out_d)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: A=%d B=%d C=%d D=%d\n",
              __func__, out_a, out_b, out_c, out_d);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GP%d%d%d%d;",
             out_a ? 1 : 0, out_b ? 1 : 0, out_c ? 1 : 0, out_d ? 1 : 0);

    return newcat_set_cmd(rig);
}

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

/* ========== FROM ftx1_ext.c ========== */



/* TUNER SELECT: EX030104 - values 0 (INT) and 1 (INT FAST) require SPA-1 */
#define FTX1_EX_TUNER_SELECT_GROUP    3
#define FTX1_EX_TUNER_SELECT_SECTION  1
#define FTX1_EX_TUNER_SELECT_ITEM     4

/* TX GENERAL power limits (field head): EX0305xx */
#define FTX1_EX_TX_GENERAL_GROUP      3
#define FTX1_EX_TX_GENERAL_SECTION    5

/* OPTION power limits (SPA-1): EX0307xx */
#define FTX1_EX_OPTION_GROUP          3
#define FTX1_EX_OPTION_SECTION        7

static int ftx1_check_ex_spa1_guardrail(int group, int section, int item, int value)
{
    /*
     * TUNER SELECT (EX030104): Values 0 (INT) and 1 (INT FAST) require SPA-1
     * Internal tuner is only available in SPA-1 amplifier
     */
    if (group == FTX1_EX_TUNER_SELECT_GROUP &&
        section == FTX1_EX_TUNER_SELECT_SECTION &&
        item == FTX1_EX_TUNER_SELECT_ITEM)
    {
        if ((value == 0 || value == 1) && !ftx1_has_spa1())
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: TUNER SELECT INT/INT(FAST) requires SPA-1 amplifier\n",
                      __func__);
            return -RIG_ENAVAIL;
        }
    }

    /*
     * OPTION section (EX0307xx): SPA-1 max power settings
     * These settings only apply when SPA-1 is connected
     */
    if (group == FTX1_EX_OPTION_GROUP && section == FTX1_EX_OPTION_SECTION)
    {
        if (!ftx1_has_spa1())
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: OPTION power settings (EX0307%02d) require SPA-1\n",
                      __func__, item);
            return -RIG_ENAVAIL;
        }
    }

    return RIG_OK;
}

/* Set Extended Menu parameter (EX MMNN PPPP;) */
int ftx1_set_ext_parm(RIG *rig, int menu, int submenu, int value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: menu=%d submenu=%d value=%d\n", __func__,
              menu, submenu, value);

    /* Check SPA-1 guardrails for specific menu items */
    ret = ftx1_check_ex_spa1_guardrail(menu / 100, (menu % 100), submenu, value);
    if (ret != RIG_OK)
    {
        return ret;
    }

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


int ftx1_set_ex_menu(RIG *rig, int group, int section, int item, int value, int digits)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    char fmt[32];

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: group=%d section=%d item=%d value=%d digits=%d\n",
              __func__, group, section, item, value, digits);

    /* Check SPA-1 guardrails */
    ret = ftx1_check_ex_spa1_guardrail(group, section, item, value);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Build format string based on value digits */
    SNPRINTF(fmt, sizeof(fmt), "EX%%02d%%02d%%02d%%0%dd;", digits);
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), fmt, group, section, item, value);

    return newcat_set_cmd(rig);
}

int ftx1_get_ex_menu(RIG *rig, int group, int section, int item, int *value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: group=%d section=%d item=%d\n",
              __func__, group, section, item);

    /* Query format: EX P1 P2 P3; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX%02d%02d%02d;",
             group, section, item);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: EX P1 P2 P3 P4; - parse value after 8 chars (EX + 6 digits) */
    if (strlen(priv->ret_data) > 8)
    {
        char *valstr = priv->ret_data + 8;
        /* Remove trailing semicolon */
        char *semi = strchr(valstr, ';');
        if (semi) *semi = '\0';

        *value = atoi(valstr);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: value=%d\n", __func__, *value);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int ftx1_set_tuner_select(RIG *rig, int tuner_type)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: tuner_type=%d\n", __func__, tuner_type);

    if (tuner_type < 0 || tuner_type > 3)
    {
        return -RIG_EINVAL;
    }

    /*
     * GUARDRAIL: INT (0) and INT(FAST) (1) tuner types require SPA-1
     */
    if ((tuner_type == 0 || tuner_type == 1) && !ftx1_has_spa1())
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: internal tuner (INT/INT FAST) requires SPA-1 amplifier\n",
                  __func__);
        return -RIG_ENAVAIL;
    }

    /* EX030104 = group 3, section 1, item 4, 1 digit */
    return ftx1_set_ex_menu(rig, 3, 1, 4, tuner_type, 1);
}

int ftx1_get_tuner_select(RIG *rig, int *tuner_type)
{
    return ftx1_get_ex_menu(rig, 3, 1, 4, tuner_type);
}

int ftx1_set_max_power(RIG *rig, int band_item, int watts)
{
    int head_type = ftx1_get_head_type();

    rig_debug(RIG_DEBUG_VERBOSE, "%s: band_item=%d watts=%d head_type=%d\n",
              __func__, band_item, watts, head_type);

    if (head_type == FTX1_HEAD_SPA1)
    {
        /* Use OPTION section (EX0307xx) for SPA-1 */
        return ftx1_set_ex_menu(rig, 3, 7, band_item, watts, 3);
    }
    else
    {
        /* Use TX GENERAL section (EX0305xx) for field head */
        return ftx1_set_ex_menu(rig, 3, 5, band_item, watts, 3);
    }
}

int ftx1_get_max_power(RIG *rig, int band_item, int *watts)
{
    int head_type = ftx1_get_head_type();

    if (head_type == FTX1_HEAD_SPA1)
    {
        return ftx1_get_ex_menu(rig, 3, 7, band_item, watts);
    }
    else
    {
        return ftx1_get_ex_menu(rig, 3, 5, band_item, watts);
    }
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

int ftx1_quick_info(RIG *rig, char *info, size_t info_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s (NOTE: QI is non-functional in firmware)\n", __func__);

    /* QI; is QMB Store - a set-only command that returns empty, not data */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QI;");

    /* Since QI is non-functional and returns empty, just return empty info */
    if (info && info_len > 0) {
        info[0] = '\0';
    }

    return RIG_OK;
}


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


/* Spectrum scope parameter types */
#define FTX1_SS_SPEED   0
#define FTX1_SS_PEAK    1
#define FTX1_SS_MARKER  2
#define FTX1_SS_COLOR   3
#define FTX1_SS_LEVEL   4
#define FTX1_SS_SPAN    5
#define FTX1_SS_MODE    6
#define FTX1_SS_AFFFT   7

int ftx1_get_spectrum_scope(RIG *rig, int param, char *value, size_t value_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: param=%d\n", __func__, param);

    if (param < 0 || param > 7)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid param %d (must be 0-7)\n",
                  __func__, param);
        return -RIG_EINVAL;
    }

    /* Query format: SS0X; where X is param type */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SS0%d;", param);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /*
     * Response format: SS0Xvalue;
     * Value starts at position 3 (after SS0X prefix)
     * Examples:
     *   SS00 -> SS0020000 (SPEED)
     *   SS01 -> SS0100000 (PEAK)
     *   SS04 -> SS04-05.0 (LEVEL, note signed decimal)
     */
    if (strlen(priv->ret_data) > 3)
    {
        const char *val_start = priv->ret_data + 3;
        size_t val_len = strlen(val_start);

        /* Remove trailing semicolon */
        if (val_len > 0 && val_start[val_len - 1] == ';')
        {
            val_len--;
        }

        if (val_len >= value_len)
        {
            val_len = value_len - 1;
        }

        strncpy(value, val_start, val_len);
        value[val_len] = '\0';

        rig_debug(RIG_DEBUG_VERBOSE, "%s: param=%d value='%s'\n",
                  __func__, param, value);
    }
    else
    {
        value[0] = '\0';
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int ftx1_set_spectrum_scope(RIG *rig, int param, const char *value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: param=%d value='%s'\n",
              __func__, param, value);

    if (param < 0 || param > 7)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid param %d (must be 0-7)\n",
                  __func__, param);
        return -RIG_EINVAL;
    }

    /* Set format: SS0Xvalue; where X is param type */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SS0%d%s;", param, value);

    return newcat_set_cmd(rig);
}

int ftx1_get_scope_speed(RIG *rig, int *speed)
{
    char value[16];
    int ret;

    ret = ftx1_get_spectrum_scope(rig, FTX1_SS_SPEED, value, sizeof(value));
    if (ret == RIG_OK)
    {
        *speed = atoi(value);
    }
    return ret;
}

int ftx1_get_scope_level(RIG *rig, float *level)
{
    char value[16];
    int ret;

    ret = ftx1_get_spectrum_scope(rig, FTX1_SS_LEVEL, value, sizeof(value));
    if (ret == RIG_OK)
    {
        *level = atof(value);
    }
    return ret;
}

int ftx1_get_scope_span(RIG *rig, int *span)
{
    char value[16];
    int ret;

    ret = ftx1_get_spectrum_scope(rig, FTX1_SS_SPAN, value, sizeof(value));
    if (ret == RIG_OK)
    {
        *span = atoi(value);
    }
    return ret;
}


/* Encoder offset VFO selection */
#define FTX1_EO_VFO_MAIN    0
#define FTX1_EO_VFO_SUB     1

/* Encoder offset encoder selection */
#define FTX1_EO_ENC_MAIN    0   /* Main dial (frequency) */
#define FTX1_EO_ENC_FUNC    1   /* Function knob */

/* Encoder offset unit selection */
#define FTX1_EO_UNIT_HZ     0
#define FTX1_EO_UNIT_KHZ    1
#define FTX1_EO_UNIT_MHZ    2

int ftx1_set_encoder_offset(RIG *rig, int vfo_select, int encoder,
                            int direction, int unit, int value)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char dir_char;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: vfo=%d encoder=%d dir=%d unit=%d value=%d\n",
              __func__, vfo_select, encoder, direction, unit, value);

    /* Validate parameters */
    if (vfo_select < 0 || vfo_select > 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid vfo_select %d\n",
                  __func__, vfo_select);
        return -RIG_EINVAL;
    }

    if (encoder < 0 || encoder > 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid encoder %d\n",
                  __func__, encoder);
        return -RIG_EINVAL;
    }

    if (unit < 0 || unit > 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid unit %d\n", __func__, unit);
        return -RIG_EINVAL;
    }

    if (value < 0 || value > 999)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid value %d (must be 0-999)\n",
                  __func__, value);
        return -RIG_EINVAL;
    }

    dir_char = (direction >= 0) ? '+' : '-';

    /* Format: EO P1 P2 P3 P4 P5P5P5; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EO%d%d%c%d%03d;",
             vfo_select, encoder, dir_char, unit, value);

    return newcat_set_cmd(rig);
}

int ftx1_step_frequency(RIG *rig, vfo_t vfo, int hz)
{
    int vfo_select;
    int direction;
    int unit;
    int value;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s hz=%d\n",
              __func__, rig_strvfo(vfo), hz);

    /* Determine VFO */
    if (vfo == RIG_VFO_SUB || vfo == RIG_VFO_B)
    {
        vfo_select = FTX1_EO_VFO_SUB;
    }
    else
    {
        vfo_select = FTX1_EO_VFO_MAIN;
    }

    /* Handle direction */
    direction = (hz >= 0) ? 1 : -1;
    if (hz < 0) hz = -hz;

    /* Determine best unit for the step */
    if (hz >= 1000000 && (hz % 1000000) == 0)
    {
        unit = FTX1_EO_UNIT_MHZ;
        value = hz / 1000000;
    }
    else if (hz >= 1000 && (hz % 1000) == 0)
    {
        unit = FTX1_EO_UNIT_KHZ;
        value = hz / 1000;
    }
    else
    {
        unit = FTX1_EO_UNIT_HZ;
        value = hz;
    }

    /* Clamp to max 999 */
    if (value > 999)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: value %d exceeds max 999, clamping\n",
                  __func__, value);
        value = 999;
    }

    return ftx1_set_encoder_offset(rig, vfo_select, FTX1_EO_ENC_MAIN,
                                   direction, unit, value);
}

/* ========== FROM ftx1_func.c ========== */


/* Extern helpers from ftx1_filter.c */
extern int ftx1_set_anf_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_anf_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_mn_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_mn_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_apf_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_apf_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_notchf_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_notchf_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_apf_level_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_apf_level_helper(RIG *rig, vfo_t vfo, value_t *val);

/* Extern helpers from ftx1_noise.c */
extern int ftx1_set_nb_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_nb_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_nr_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_nr_helper(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_nb_level_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_nb_level_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_nr_level_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_nr_level_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_na_helper(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_na_helper(RIG *rig, vfo_t vfo, int *status);

/* Extern helpers from ftx1_preamp.c */
extern int ftx1_set_preamp_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_preamp_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_att_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_att_helper(RIG *rig, vfo_t vfo, value_t *val);

/* Extern helpers from ftx1_audio.c */
extern int ftx1_set_af_gain(RIG *rig, vfo_t vfo, float val);
extern int ftx1_get_af_gain(RIG *rig, vfo_t vfo, float *val);
extern int ftx1_set_rf_gain(RIG *rig, vfo_t vfo, float val);
extern int ftx1_get_rf_gain(RIG *rig, vfo_t vfo, float *val);
extern int ftx1_set_mic_gain(RIG *rig, float val);
extern int ftx1_get_mic_gain(RIG *rig, float *val);
extern int ftx1_set_power(RIG *rig, float val);
extern int ftx1_get_power(RIG *rig, float *val);
extern int ftx1_set_vox_gain(RIG *rig, float val);
extern int ftx1_get_vox_gain(RIG *rig, float *val);
extern int ftx1_set_vox_delay(RIG *rig, int ms);
extern int ftx1_get_vox_delay(RIG *rig, int *ms);
extern int ftx1_set_agc(RIG *rig, int val);
extern int ftx1_get_agc(RIG *rig, int *val);
extern int ftx1_get_smeter(RIG *rig, vfo_t vfo, int *val);
extern int ftx1_get_meter(RIG *rig, int meter_type, int *val);
extern int ftx1_set_monitor_level(RIG *rig, float val);
extern int ftx1_get_monitor_level(RIG *rig, float *val);
extern int ftx1_set_amc_output(RIG *rig, float val);
extern int ftx1_get_amc_output(RIG *rig, float *val);
extern int ftx1_set_width(RIG *rig, vfo_t vfo, int width_code);
extern int ftx1_get_width(RIG *rig, vfo_t vfo, int *width_code);
extern int ftx1_set_meter_switch(RIG *rig, int meter_type);
extern int ftx1_get_meter_switch(RIG *rig, int *meter_type);

/* Extern helpers from ftx1_filter.c */
extern int ftx1_set_filter_number(RIG *rig, vfo_t vfo, int filter);
extern int ftx1_get_filter_number(RIG *rig, vfo_t vfo, int *filter);
extern int ftx1_set_contour(RIG *rig, vfo_t vfo, int status);
extern int ftx1_get_contour(RIG *rig, vfo_t vfo, int *status);
extern int ftx1_set_contour_freq(RIG *rig, vfo_t vfo, int freq);
extern int ftx1_get_contour_freq(RIG *rig, vfo_t vfo, int *freq);

/* Extern helpers from ftx1_tx.c */
extern int ftx1_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int ftx1_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
extern int ftx1_set_vox(RIG *rig, int status);
extern int ftx1_get_vox(RIG *rig, int *status);
extern int ftx1_set_monitor(RIG *rig, int status);
extern int ftx1_get_monitor(RIG *rig, int *status);
extern int ftx1_set_tuner(RIG *rig, int mode);
extern int ftx1_get_tuner(RIG *rig, int *mode);
extern int ftx1_set_squelch(RIG *rig, vfo_t vfo, float val);
extern int ftx1_get_squelch(RIG *rig, vfo_t vfo, float *val);
extern int ftx1_set_powerstat(RIG *rig, powerstat_t status);
extern int ftx1_get_powerstat(RIG *rig, powerstat_t *status);

/* Extern helpers from ftx1_cw.c */
extern int ftx1_set_keyer_speed(RIG *rig, int wpm);
extern int ftx1_get_keyer_speed(RIG *rig, int *wpm);
extern int ftx1_set_keyer_paddle(RIG *rig, int ratio);
extern int ftx1_get_keyer_paddle(RIG *rig, int *ratio);
extern int ftx1_set_cw_delay(RIG *rig, int ms);
extern int ftx1_get_cw_delay(RIG *rig, int *ms);
extern int ftx1_send_morse(RIG *rig, vfo_t vfo, const char *msg);
extern int ftx1_stop_morse(RIG *rig, vfo_t vfo);

/* Extern helpers from ftx1_tx.c */
extern int ftx1_set_breakin(RIG *rig, int mode);
extern int ftx1_get_breakin(RIG *rig, int *mode);
extern int ftx1_set_processor(RIG *rig, int status);
extern int ftx1_get_processor(RIG *rig, int *status);

/* Extern helpers from ftx1_ctcss.c */
extern int ftx1_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code);
extern int ftx1_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code);

/* Extern helpers from ftx1_info.c */
extern int ftx1_set_trn(RIG *rig, int trn);
extern int ftx1_get_trn(RIG *rig, int *trn);
extern int ftx1_get_info(RIG *rig, char *info, size_t info_len);
extern int ftx1_set_lock(RIG *rig, int lock);
extern int ftx1_get_lock(RIG *rig, int *lock);
extern int ftx1_set_if_shift(RIG *rig, vfo_t vfo, int on, int shift_hz);
extern int ftx1_get_if_shift(RIG *rig, vfo_t vfo, int *on, int *shift_hz);

/* Main override for set_func */
int ftx1_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: func=%s status=%d\n", __func__,
              rig_strfunc(func), status);

    switch (func) {
        case RIG_FUNC_ANF:
            return ftx1_set_anf_helper(rig, vfo, status);
        case RIG_FUNC_MN:
            return ftx1_set_mn_helper(rig, vfo, status);
        case RIG_FUNC_APF:
            return ftx1_set_apf_helper(rig, vfo, status);
        case RIG_FUNC_NB:
            return ftx1_set_nb_helper(rig, vfo, status);
        case RIG_FUNC_NR:
            return ftx1_set_nr_helper(rig, vfo, status);
        case RIG_FUNC_VOX:
            return ftx1_set_vox(rig, status);
        case RIG_FUNC_MON:
            return ftx1_set_monitor(rig, status);
        case RIG_FUNC_TUNER:
            return ftx1_set_tuner(rig, status ? 1 : 0);
        case RIG_FUNC_LOCK:
            return ftx1_set_lock(rig, status);
        case RIG_FUNC_COMP:
            return ftx1_set_processor(rig, status);
        case RIG_FUNC_SBKIN:
            return ftx1_set_breakin(rig, status ? 1 : 0);
        case RIG_FUNC_FBKIN:
            return ftx1_set_breakin(rig, status ? 2 : 0);
        /* Note: Contour (CO command) not exposed as RIG_FUNC_CONTOUR doesn't exist in Hamlib */
        default:
            return newcat_set_func(rig, vfo, func, status);
    }
}

/* Main override for get_func */
int ftx1_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int ret, mode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: func=%s\n", __func__, rig_strfunc(func));

    switch (func) {
        case RIG_FUNC_ANF:
            return ftx1_get_anf_helper(rig, vfo, status);
        case RIG_FUNC_MN:
            return ftx1_get_mn_helper(rig, vfo, status);
        case RIG_FUNC_APF:
            return ftx1_get_apf_helper(rig, vfo, status);
        case RIG_FUNC_NB:
            return ftx1_get_nb_helper(rig, vfo, status);
        case RIG_FUNC_NR:
            return ftx1_get_nr_helper(rig, vfo, status);
        case RIG_FUNC_VOX:
            return ftx1_get_vox(rig, status);
        case RIG_FUNC_MON:
            return ftx1_get_monitor(rig, status);
        case RIG_FUNC_TUNER:
            ret = ftx1_get_tuner(rig, &mode);
            if (ret == RIG_OK) *status = (mode > 0) ? 1 : 0;
            return ret;
        case RIG_FUNC_LOCK:
            return ftx1_get_lock(rig, status);
        case RIG_FUNC_COMP:
            return ftx1_get_processor(rig, status);
        case RIG_FUNC_SBKIN:
            ret = ftx1_get_breakin(rig, &mode);
            if (ret == RIG_OK) *status = (mode == 1) ? 1 : 0;
            return ret;
        case RIG_FUNC_FBKIN:
            ret = ftx1_get_breakin(rig, &mode);
            if (ret == RIG_OK) *status = (mode == 2) ? 1 : 0;
            return ret;
        /* Note: Contour (CO command) not exposed as RIG_FUNC_CONTOUR doesn't exist in Hamlib */
        default:
            return newcat_get_func(rig, vfo, func, status);
    }
}

/* Main override for set_level */
int ftx1_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%s\n", __func__, rig_strlevel(level));

    switch (level) {
        case RIG_LEVEL_AF:
            return ftx1_set_af_gain(rig, vfo, val.f);
        case RIG_LEVEL_RF:
            return ftx1_set_rf_gain(rig, vfo, val.f);
        case RIG_LEVEL_SQL:
            return ftx1_set_squelch(rig, vfo, val.f);
        case RIG_LEVEL_MICGAIN:
            return ftx1_set_mic_gain(rig, val.f);
        case RIG_LEVEL_RFPOWER:
            return ftx1_set_power(rig, val.f);
        case RIG_LEVEL_VOXGAIN:
            return ftx1_set_vox_gain(rig, val.f);
        case RIG_LEVEL_VOXDELAY:
            /* VOXDELAY is val.i in tenths of seconds, FTX-1 uses 00-30 (same units) */
            return ftx1_set_vox_delay(rig, val.i);
        case RIG_LEVEL_AGC:
            return ftx1_set_agc(rig, val.i);
        case RIG_LEVEL_KEYSPD:
            return ftx1_set_keyer_speed(rig, val.i);
        case RIG_LEVEL_BKINDL:
            return ftx1_set_cw_delay(rig, val.i);
        case RIG_LEVEL_CWPITCH:
            /* FTX-1 uses KP for paddle ratio, not pitch. No CAT command for pitch. */
            return -RIG_ENAVAIL;
        case RIG_LEVEL_NOTCHF:
            return ftx1_set_notchf_helper(rig, vfo, val);
        case RIG_LEVEL_APF:
            return ftx1_set_apf_level_helper(rig, vfo, val);
        case RIG_LEVEL_NB:
            return ftx1_set_nb_level_helper(rig, vfo, val);
        case RIG_LEVEL_NR:
            return ftx1_set_nr_level_helper(rig, vfo, val);
        case RIG_LEVEL_PREAMP:
            return ftx1_set_preamp_helper(rig, vfo, val);
        case RIG_LEVEL_ATT:
            return ftx1_set_att_helper(rig, vfo, val);
        case RIG_LEVEL_IF:
            /* FTX-1 IS command: val.i is shift in Hz, turn on if non-zero */
            return ftx1_set_if_shift(rig, vfo, (val.i != 0) ? 1 : 0, val.i);
        case RIG_LEVEL_MONITOR_GAIN:
            return ftx1_set_monitor_level(rig, val.f);
        case RIG_LEVEL_METER:
            return ftx1_set_meter_switch(rig, val.i);
        case RIG_LEVEL_PBT_IN:
        case RIG_LEVEL_PBT_OUT:
            /* SH command: val.f is 0.0-1.0, convert to width code (0-23) */
            return ftx1_set_width(rig, vfo, (int)(val.f * 23));
        case RIG_LEVEL_ANTIVOX:
            return ftx1_set_amc_output(rig, val.f);
        default:
            return newcat_set_level(rig, vfo, level, val);
    }
}

/* Main override for get_level */
int ftx1_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int ret, ival;
    float fval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: level=%s\n", __func__, rig_strlevel(level));

    switch (level) {
        case RIG_LEVEL_AF:
            ret = ftx1_get_af_gain(rig, vfo, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_RF:
            ret = ftx1_get_rf_gain(rig, vfo, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_SQL:
            ret = ftx1_get_squelch(rig, vfo, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_MICGAIN:
            ret = ftx1_get_mic_gain(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_RFPOWER:
            ret = ftx1_get_power(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_VOXGAIN:
            ret = ftx1_get_vox_gain(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_VOXDELAY:
            /* FTX-1 returns 00-30 (tenths of seconds), same as Hamlib VOXDELAY */
            ret = ftx1_get_vox_delay(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_AGC:
            ret = ftx1_get_agc(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_KEYSPD:
            ret = ftx1_get_keyer_speed(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_BKINDL:
            ret = ftx1_get_cw_delay(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_CWPITCH:
            /* FTX-1 uses KP for paddle ratio, not pitch. No CAT command for pitch. */
            return -RIG_ENAVAIL;
        case RIG_LEVEL_NOTCHF:
            return ftx1_get_notchf_helper(rig, vfo, val);
        case RIG_LEVEL_APF:
            return ftx1_get_apf_level_helper(rig, vfo, val);
        case RIG_LEVEL_NB:
            return ftx1_get_nb_level_helper(rig, vfo, val);
        case RIG_LEVEL_NR:
            return ftx1_get_nr_level_helper(rig, vfo, val);
        case RIG_LEVEL_PREAMP:
            return ftx1_get_preamp_helper(rig, vfo, val);
        case RIG_LEVEL_ATT:
            return ftx1_get_att_helper(rig, vfo, val);
        case RIG_LEVEL_STRENGTH:
        case RIG_LEVEL_RAWSTR:
            ret = ftx1_get_smeter(rig, vfo, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_SWR:
            ret = ftx1_get_meter(rig, 4, &ival);  /* 4=SWR */
            if (ret == RIG_OK) val->f = (float)ival / 100.0f;
            return ret;
        case RIG_LEVEL_ALC:
            ret = ftx1_get_meter(rig, 2, &ival);  /* 2=ALC */
            if (ret == RIG_OK) val->f = (float)ival / 100.0f;
            return ret;
        case RIG_LEVEL_COMP:
            ret = ftx1_get_meter(rig, 1, &ival);  /* 1=COMP */
            if (ret == RIG_OK) val->f = (float)ival / 100.0f;
            return ret;
        case RIG_LEVEL_IF:
            {
                int on, shift_hz;
                ret = ftx1_get_if_shift(rig, vfo, &on, &shift_hz);
                if (ret == RIG_OK) val->i = shift_hz;
                return ret;
            }
        case RIG_LEVEL_MONITOR_GAIN:
            ret = ftx1_get_monitor_level(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        case RIG_LEVEL_METER:
            ret = ftx1_get_meter_switch(rig, &ival);
            if (ret == RIG_OK) val->i = ival;
            return ret;
        case RIG_LEVEL_PBT_IN:
        case RIG_LEVEL_PBT_OUT:
            /* SH command: returns width code (0-23), convert to 0.0-1.0 */
            ret = ftx1_get_width(rig, vfo, &ival);
            if (ret == RIG_OK) val->f = (float)ival / 23.0f;
            return ret;
        case RIG_LEVEL_ANTIVOX:
            ret = ftx1_get_amc_output(rig, &fval);
            if (ret == RIG_OK) val->f = fval;
            return ret;
        default:
            return newcat_get_level(rig, vfo, level, val);
    }
}

/* PTT wrapper */
int ftx1_set_ptt_func(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    return ftx1_set_ptt(rig, vfo, ptt);
}

int ftx1_get_ptt_func(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    return ftx1_get_ptt(rig, vfo, ptt);
}

/* Power stat wrapper */
int ftx1_set_powerstat_func(RIG *rig, powerstat_t status)
{
    return ftx1_set_powerstat(rig, status);
}

int ftx1_get_powerstat_func(RIG *rig, powerstat_t *status)
{
    return ftx1_get_powerstat(rig, status);
}

/* CTCSS tone wrappers */
int ftx1_set_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t tone)
{
    return ftx1_set_ctcss_tone(rig, vfo, tone);
}

int ftx1_get_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t *tone)
{
    return ftx1_get_ctcss_tone(rig, vfo, tone);
}

int ftx1_set_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t tone)
{
    return ftx1_set_ctcss_sql(rig, vfo, tone);
}

int ftx1_get_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t *tone)
{
    return ftx1_get_ctcss_sql(rig, vfo, tone);
}

/* DCS code wrappers */
int ftx1_set_dcs_code_func(RIG *rig, vfo_t vfo, tone_t code)
{
    return ftx1_set_dcs_code(rig, vfo, code);
}

int ftx1_get_dcs_code_func(RIG *rig, vfo_t vfo, tone_t *code)
{
    return ftx1_get_dcs_code(rig, vfo, code);
}

int ftx1_set_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t code)
{
    return ftx1_set_dcs_sql(rig, vfo, code);
}

int ftx1_get_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t *code)
{
    return ftx1_get_dcs_sql(rig, vfo, code);
}

/* Morse/CW wrappers */
int ftx1_send_morse_func(RIG *rig, vfo_t vfo, const char *msg)
{
    return ftx1_send_morse(rig, vfo, msg);
}

int ftx1_stop_morse_func(RIG *rig, vfo_t vfo)
{
    return ftx1_stop_morse(rig, vfo);
}

/* Transceive (AI) mode wrapper */
int ftx1_set_trn_func(RIG *rig, int trn)
{
    return ftx1_set_trn(rig, trn);
}

int ftx1_get_trn_func(RIG *rig, int *trn)
{
    return ftx1_get_trn(rig, trn);
}

/* ========== FROM ftx1.c ========== */
static struct
{
    int head_type;          /* FTX1_HEAD_FIELD or FTX1_HEAD_SPA1 */
    int spa1_detected;      /* 1 if SPA-1 confirmed via VE4 command */
    int detection_done;     /* 1 if auto-detection has been performed */
} ftx1_priv = {
    .head_type = FTX1_HEAD_UNKNOWN,
    .spa1_detected = 0,
    .detection_done = 0,
};

// Private caps for newcat framework
static const struct newcat_priv_caps ftx1_priv_caps = {
    .roofing_filter_count = 0,
};

// Extern declarations for group-specific functions (add more as groups are implemented)
extern int ftx1_set_vfo(RIG *rig, vfo_t vfo);
extern int ftx1_get_vfo(RIG *rig, vfo_t *vfo);
extern int ftx1_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
extern int ftx1_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
// Additional externs for group 4
extern int ftx1_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
extern int ftx1_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
extern int ftx1_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
extern int ftx1_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
// Additional externs for group 6
extern int ftx1_set_preamp_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_preamp_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_att_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_att_helper(RIG *rig, vfo_t vfo, value_t *val);

// Wrappers from ftx1_func.c for rig caps
extern int ftx1_set_ptt_func(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int ftx1_get_ptt_func(RIG *rig, vfo_t vfo, ptt_t *ptt);
extern int ftx1_set_powerstat_func(RIG *rig, powerstat_t status);
extern int ftx1_get_powerstat_func(RIG *rig, powerstat_t *status);
extern int ftx1_set_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_dcs_code_func(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_code_func(RIG *rig, vfo_t vfo, tone_t *code);
extern int ftx1_set_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t *code);
extern int ftx1_send_morse_func(RIG *rig, vfo_t vfo, const char *msg);
extern int ftx1_stop_morse_func(RIG *rig, vfo_t vfo);
extern int ftx1_set_trn_func(RIG *rig, int trn);
extern int ftx1_get_trn_func(RIG *rig, int *trn);

// Externs from ftx1_mem.c
extern int ftx1_set_mem(RIG *rig, vfo_t vfo, int ch);
extern int ftx1_get_mem(RIG *rig, vfo_t vfo, int *ch);
extern int ftx1_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
extern int ftx1_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);

// Externs from ftx1_scan.c
extern int ftx1_set_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
extern int ftx1_get_scan(RIG *rig, vfo_t vfo, scan_t *scan, int *ch);
extern int ftx1_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
extern int ftx1_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);

// Externs from ftx1_freq.c
extern int ftx1_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
extern int ftx1_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

// Externs from ftx1_vfo.c
extern int ftx1_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

/*
 * ftx1_detect_spa1 - Detect SPA-1 amplifier via VE4 command
 *
 * The VE4; command returns the SPA-1 firmware version if connected.
 * If no SPA-1 is present, the command typically returns '?' or an error.
 *
 * Returns: 1 if SPA-1 detected, 0 otherwise
 */
static int ftx1_detect_spa1(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: checking for SPA-1\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VE4;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: VE4 command failed (no SPA-1)\n", __func__);
        return 0;
    }

    /* VE4 returns version string like "VE4xxx;" if SPA-1 is present */
    if (strlen(priv->ret_data) >= 4 && priv->ret_data[0] == 'V' &&
        priv->ret_data[1] == 'E' && priv->ret_data[2] == '4')
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: SPA-1 detected, firmware: %s\n",
                  __func__, priv->ret_data);
        return 1;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: no SPA-1 detected\n", __func__);
    return 0;
}

/*
 * ftx1_probe_field_head_power - Probe Field Head power source (battery vs 12V)
 *
 * The radio enforces hardware power limits based on actual power source.
 * On battery, the radio will not accept power settings above 6W.
 * This probe attempts to set 8W and checks if the radio accepts it.
 *
 * Returns: FTX1_HEAD_FIELD_BATTERY or FTX1_HEAD_FIELD_12V
 */
static int ftx1_probe_field_head_power(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    char original_power[16];
    int power_accepted;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: probing Field Head power source\n", __func__);

    /* Save current power setting */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK || strlen(priv->ret_data) < 4)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: could not read current power\n", __func__);
        return FTX1_HEAD_FIELD_BATTERY;  /* Default to battery (safer) */
    }

    strncpy(original_power, priv->ret_data, sizeof(original_power) - 1);
    original_power[sizeof(original_power) - 1] = '\0';
    rig_debug(RIG_DEBUG_VERBOSE, "%s: original power: %s\n", __func__, original_power);

    /* Try to set 8W (above battery max of 6W) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC1008;");
    ret = newcat_set_cmd(rig);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: could not set test power\n", __func__);
        return FTX1_HEAD_FIELD_BATTERY;
    }

    /* Read back to see if it was accepted */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK || strlen(priv->ret_data) < 5)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: could not read back power\n", __func__);
        power_accepted = 0;
    }
    else
    {
        /* Check if power is 8W or above (PC1008 or higher) */
        int power_value = atoi(priv->ret_data + 3);
        power_accepted = (power_value >= 8);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: power readback: %d, accepted=%d\n",
                  __func__, power_value, power_accepted);
    }

    /* Restore original power setting */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", original_power);
    newcat_set_cmd(rig);

    if (power_accepted)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Field Head on 12V detected (accepted 8W)\n", __func__);
        return FTX1_HEAD_FIELD_12V;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Field Head on battery detected (rejected 8W)\n", __func__);
        return FTX1_HEAD_FIELD_BATTERY;
    }
}

/*
 * ftx1_detect_head_type - Detect head type using PC command format and power probe
 *
 * Stage 1: PC command format identifies Field Head vs SPA-1
 *   PC1xxx = Field Head (battery or 12V)
 *   PC2xxx = Optima/SPA-1 (5-100W)
 *
 * Stage 2: For Field Head, power probe distinguishes battery vs 12V
 *   - Attempt to set 8W
 *   - If radio accepts 8W → 12V power (0.5-10W)
 *   - If radio rejects 8W → Battery power (0.5-6W)
 *
 * Returns: FTX1_HEAD_FIELD_BATTERY, FTX1_HEAD_FIELD_12V, FTX1_HEAD_SPA1, or FTX1_HEAD_UNKNOWN
 */
static int ftx1_detect_head_type(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: detecting head type via PC command\n", __func__);

    /* Stage 1: Read PC command to determine Field vs SPA-1 */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: PC query failed\n", __func__);
        return FTX1_HEAD_UNKNOWN;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: PC response: %s\n", __func__, priv->ret_data);

    /* Response format: PC1xxx (Field) or PC2xxx (SPA-1) */
    if (strlen(priv->ret_data) < 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: PC response too short: %s\n",
                  __func__, priv->ret_data);
        return FTX1_HEAD_UNKNOWN;
    }

    /* Check head type indicator (position 2) */
    char head_code = priv->ret_data[2];

    if (head_code == '1')
    {
        /* Field Head - probe to determine battery vs 12V */
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Field Head detected, probing power source\n", __func__);
        return ftx1_probe_field_head_power(rig);
    }
    else if (head_code == '2')
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Optima/SPA-1 (5-100W) detected\n", __func__);
        return FTX1_HEAD_SPA1;
    }

    rig_debug(RIG_DEBUG_WARN, "%s: unknown head type code: %c\n", __func__, head_code);
    return FTX1_HEAD_UNKNOWN;
}

/*
 * ftx1_open - FTX-1 specific rig open with head type detection
 *
 * Calls newcat_open, then auto-detects the head configuration:
 *   Stage 1: PC command format (PC1xxx=Field, PC2xxx=SPA-1)
 *   Stage 2: Power probe for Field Head (battery vs 12V)
 *   - Also queries VE4 to confirm SPA-1 presence
 *   - Stores results for use by tuner and power control functions
 */
static int ftx1_open(RIG *rig)
{
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Call the standard newcat open first */
    ret = newcat_open(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Auto-detect head type and SPA-1 presence */
    ftx1_priv.head_type = ftx1_detect_head_type(rig);
    ftx1_priv.spa1_detected = ftx1_detect_spa1(rig);
    ftx1_priv.detection_done = 1;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: detection complete - head_type=%d spa1_detected=%d\n",
              __func__, ftx1_priv.head_type, ftx1_priv.spa1_detected);

    /* Cross-check: if head_type is SPA-1, spa1_detected should also be true */
    if (ftx1_priv.head_type == FTX1_HEAD_SPA1 && !ftx1_priv.spa1_detected)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: PC reports SPA-1 but VE4 detection failed\n", __func__);
    }

    return RIG_OK;
}

/*
 * ftx1_has_spa1 - Check if Optima/SPA-1 amplifier is present
 *
 * Returns 1 if Optima/SPA-1 detected, 0 otherwise.
 * Used by tuner and power control functions for guardrails.
 */
int ftx1_has_spa1(void)
{
    return ftx1_priv.spa1_detected ||
           ftx1_priv.head_type == FTX1_HEAD_SPA1;
}

/*
 * ftx1_get_head_type - Get detected head type
 *
 * Returns FTX1_HEAD_FIELD, FTX1_HEAD_SPA1, or FTX1_HEAD_UNKNOWN
 */
int ftx1_get_head_type(void)
{
    return ftx1_priv.head_type;
}

// Rig caps structure
struct rig_caps ftx1_caps = {
    .rig_model = RIG_MODEL_FTX1,
    .model_name = "FTX-1",
    .mfg_name = "Yaesu",
    .version = "20251209.0",  // Use date-based version for dev
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,  // Update to stable once complete
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,  /* 8N1 per FTX-1 default */
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 50,  // Delay after write for FTX-1 response time
    .timeout = 2000,
    .retry = 3,
    /* Note: ARO, FAGC, DUAL_WATCH, DIVERSITY return '?' from FTX-1 firmware - not supported */
    /* Contour (CO command) is FTX-1 specific - handle via ext_level */
    .has_get_func = RIG_FUNC_COMP | RIG_FUNC_VOX | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SBKIN | RIG_FUNC_FBKIN | RIG_FUNC_NB | RIG_FUNC_NR | RIG_FUNC_MN | RIG_FUNC_LOCK | RIG_FUNC_MON | RIG_FUNC_TUNER | RIG_FUNC_RIT | RIG_FUNC_XIT | RIG_FUNC_APF | RIG_FUNC_ANF,
    .has_set_func = RIG_FUNC_COMP | RIG_FUNC_VOX | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SBKIN | RIG_FUNC_FBKIN | RIG_FUNC_NB | RIG_FUNC_NR | RIG_FUNC_MN | RIG_FUNC_LOCK | RIG_FUNC_MON | RIG_FUNC_TUNER | RIG_FUNC_RIT | RIG_FUNC_XIT | RIG_FUNC_APF | RIG_FUNC_ANF,
    .has_get_level = RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_SQL | RIG_LEVEL_IF | RIG_LEVEL_APF | RIG_LEVEL_NB | RIG_LEVEL_NR | RIG_LEVEL_PBT_IN | RIG_LEVEL_PBT_OUT | RIG_LEVEL_RFPOWER | RIG_LEVEL_MICGAIN | RIG_LEVEL_KEYSPD | RIG_LEVEL_NOTCHF | RIG_LEVEL_COMP | RIG_LEVEL_AGC | RIG_LEVEL_BKINDL | RIG_LEVEL_BALANCE | RIG_LEVEL_METER | RIG_LEVEL_VOXGAIN | RIG_LEVEL_VOXDELAY | RIG_LEVEL_ANTIVOX | RIG_LEVEL_RAWSTR | RIG_LEVEL_SWR | RIG_LEVEL_ALC | RIG_LEVEL_STRENGTH | RIG_LEVEL_ATT | RIG_LEVEL_PREAMP | RIG_LEVEL_MONITOR_GAIN,
    .has_set_level = RIG_LEVEL_SET(RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_SQL | RIG_LEVEL_IF | RIG_LEVEL_APF | RIG_LEVEL_NB | RIG_LEVEL_NR | RIG_LEVEL_PBT_IN | RIG_LEVEL_PBT_OUT | RIG_LEVEL_RFPOWER | RIG_LEVEL_MICGAIN | RIG_LEVEL_KEYSPD | RIG_LEVEL_NOTCHF | RIG_LEVEL_COMP | RIG_LEVEL_AGC | RIG_LEVEL_BKINDL | RIG_LEVEL_BALANCE | RIG_LEVEL_METER | RIG_LEVEL_VOXGAIN | RIG_LEVEL_VOXDELAY | RIG_LEVEL_ANTIVOX | RIG_LEVEL_RAWSTR | RIG_LEVEL_SWR | RIG_LEVEL_ALC | RIG_LEVEL_STRENGTH | RIG_LEVEL_ATT | RIG_LEVEL_PREAMP | RIG_LEVEL_MONITOR_GAIN),
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran = {
        /* FTX-1 specific level granularity */
        /* Include common Yaesu defaults, then override as needed */
#include "level_gran_yaesu.h"
        /* FTX-1 overrides for levels with 0-100 range instead of 0-255 */
        [LVL_MICGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_VOXGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_RFPOWER] = { .min = { .f = 0.05 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_MONITOR_GAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_SQL] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        /* CW pitch: FTX-1 does NOT support CWPITCH via CAT (KP is paddle ratio) */
        /* Key speed: FTX-1 uses 4-60 WPM */
        [LVL_KEYSPD] = { .min = { .i = 4 }, .max = { .i = 60 }, .step = { .i = 1 } },
        /* Break-in delay: FTX-1 SD command uses 00-30 (tenths of seconds, 0-3000ms) */
        [LVL_BKINDL] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        /* VOX delay: FTX-1 VD command uses 00-30 (tenths of seconds, 0-3000ms) */
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        /* Notch frequency: FTX-1 uses 1-3200 Hz */
        [LVL_NOTCHF] = { .min = { .i = 1 }, .max = { .i = 3200 }, .step = { .i = 10 } },
    },
    .ctcss_list = common_ctcss_list,  // Reused common list
    .dcs_list = common_dcs_list,
    .preamp = {10, 20, RIG_DBLST_END},  // AMP1=10dB, AMP2=20dB (0=IPO)
    .attenuator = {12, RIG_DBLST_END},
    .max_rit = Hz(9999),
    .max_xit = Hz(9999),
    .max_ifshift = Hz(1200),
    .vfo_ops = RIG_OP_CPY | RIG_OP_XCHG | RIG_OP_FROM_VFO | RIG_OP_TO_VFO | RIG_OP_MCL | RIG_OP_TUNE | RIG_OP_BAND_UP | RIG_OP_BAND_DOWN,
    .targetable_vfo = RIG_TARGETABLE_ALL,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 12,  // Tag size from manual
    .chan_list = {
        {1, 99, RIG_MTYPE_MEM, NEWCAT_MEM_CAP},
        {100, 117, RIG_MTYPE_EDGE, NEWCAT_MEM_CAP},  // PMS
        {500, 503, RIG_MTYPE_MEM, NEWCAT_MEM_CAP},  // 60m
        RIG_CHAN_END,
    },
    .rx_range_list1 = {
        {kHz(30), MHz(56), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(118), MHz(164), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(430), MHz(470), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},  // Updated to 430-470 for UHF
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {kHz(1800), MHz(2) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(3.5), MHz(4) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(5.1675), MHz(5.4065), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},  // 60m
        {MHz(7), MHz(7.3), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(10.1), MHz(10.15), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(14), MHz(14.35), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(18.068), MHz(18.168), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(21), MHz(21.45), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(24.89), MHz(24.99), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(28), MHz(29.7), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(50), MHz(54), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(144), MHz(148), RIG_MODE_ALL, 5000, 50000, RIG_VFO_ALL, RIG_ANT_1},  // Approx 50W
        {MHz(430), MHz(450), RIG_MODE_ALL, 5000, 20000, RIG_VFO_ALL, RIG_ANT_1},  // Approx 20W
        RIG_FRNG_END,
    },
    .rx_range_list2 = {
        {kHz(30), MHz(56), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(118), MHz(164), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(430), MHz(470), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},  // Updated to 430-470 for UHF
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        {kHz(1800), MHz(2) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(3.5), MHz(4) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(5.1675), MHz(5.4065), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},  // 60m
        {MHz(7), MHz(7.3), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(10.1), MHz(10.15), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(14), MHz(14.35), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(18.068), MHz(18.168), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(21), MHz(21.45), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(24.89), MHz(24.99), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(28), MHz(29.7), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(50), MHz(54), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(144), MHz(148), RIG_MODE_ALL, 5000, 50000, RIG_VFO_ALL, RIG_ANT_1},  // Approx 50W
        {MHz(430), MHz(450), RIG_MODE_ALL, 5000, 20000, RIG_VFO_ALL, RIG_ANT_1},  // Approx 20W
        RIG_FRNG_END,
    },
    .tuning_steps = {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, 10},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_PKTFM, 100},
        {RIG_MODE_WFM, kHz(25)},
        RIG_TS_END,
    },
    .filters = {},  // To be populated in filter group
    .priv = (void *)&ftx1_priv_caps,  // Reuse newcat priv
    .rig_init = newcat_init,
    .rig_cleanup = newcat_cleanup,
    .rig_open = ftx1_open,  // FTX-1 specific open with SPA-1 detection
    .rig_close = newcat_close,
    // Pointers to group-specific overrides or newcat defaults
    .set_freq = ftx1_set_freq,  // Override from ftx1_freq.c
    .get_freq = ftx1_get_freq,
    .set_mode = newcat_set_mode,
    .get_mode = newcat_get_mode,
    .set_vfo = ftx1_set_vfo,  // Override from ftx1_vfo.c
    .get_vfo = ftx1_get_vfo,
    .set_ptt = ftx1_set_ptt_func,  // Override from ftx1_tx.c via ftx1_func.c
    .get_ptt = ftx1_get_ptt_func,
    .get_dcd = NULL,
    .set_powerstat = ftx1_set_powerstat_func,  // Override from ftx1_tx.c via ftx1_func.c
    .get_powerstat = ftx1_get_powerstat_func,
    .set_func = ftx1_set_func,
    .get_func = ftx1_get_func,
    .set_level = ftx1_set_level,
    .get_level = ftx1_get_level,
    .set_ctcss_tone = ftx1_set_ctcss_tone_func,  // Override from ftx1_ctcss.c via ftx1_func.c
    .get_ctcss_tone = ftx1_get_ctcss_tone_func,
    .set_ctcss_sql = ftx1_set_ctcss_sql_func,
    .get_ctcss_sql = ftx1_get_ctcss_sql_func,
    .set_dcs_code = ftx1_set_dcs_code_func,
    .get_dcs_code = ftx1_get_dcs_code_func,
    .set_dcs_sql = ftx1_set_dcs_sql_func,
    .get_dcs_sql = ftx1_get_dcs_sql_func,
    .send_morse = ftx1_send_morse_func,  // Override from ftx1_cw.c via ftx1_func.c
    .stop_morse = ftx1_stop_morse_func,
    .set_trn = ftx1_set_trn_func,  // Override from ftx1_info.c via ftx1_func.c
    .get_trn = ftx1_get_trn_func,
    .set_mem = ftx1_set_mem,  // Override from ftx1_mem.c
    .get_mem = ftx1_get_mem,
    .vfo_op = ftx1_vfo_op,
    .scan = ftx1_set_scan,  // Override from ftx1_scan.c
    .set_channel = ftx1_set_channel,  // Override from ftx1_mem.c
    .get_channel = ftx1_get_channel,
    .set_ts = ftx1_set_ts,  // Override from ftx1_scan.c
    .get_ts = ftx1_get_ts,
    .set_ext_level = newcat_set_ext_level,
    .get_ext_level = newcat_get_ext_level,
    .set_conf = newcat_set_conf,
    .get_conf = newcat_get_conf,
    .set_rit = newcat_set_rit,
    .get_rit = newcat_get_rit,
    .set_xit = newcat_set_xit,
    .get_xit = newcat_get_xit,
    .set_split_vfo = ftx1_set_split_vfo,  // Override from ftx1_vfo.c
    .get_split_vfo = ftx1_get_split_vfo,
    .set_split_freq = NULL,
    .get_split_freq = NULL,
    .set_split_mode = NULL,
    .get_split_mode = NULL,
    .get_info = newcat_get_info,
    .power2mW = newcat_power2mW,
    .mW2power = newcat_mW2power,
};
