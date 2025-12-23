/*
 * Hamlib Yaesu backend - FTX-1 VFO Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for VFO management and split operation.
 *
 * CAT Commands in this file:
 *   VS P1;    - VFO Select (0=MAIN, 1=SUB)
 *   ST P1;    - Split on/off (0=off, 1=on)
 *   FT P1;    - Function TX VFO (0=MAIN TX, 1=SUB TX)
 *   FR P1P2;  - Function RX (00=dual receive, 01=single receive)
 *   FA/FB     - VFO-A/B frequency (used for split_freq)
 *   MD P1 P2; - Mode (used for split_mode, P1=VFO)
 *
 * Note: BS (Band Select) returns '?' in firmware - not implemented.
 *       AB, BA, SV handled via ftx1_vfo_op.
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/*
 * ftx1_set_vfo
 *
 * Set active VFO using VS command.
 * Format: VS0; (MAIN) or VS1; (SUB)
 */
int ftx1_set_vfo(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv;
    int p1;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

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

/*
 * ftx1_get_vfo
 *
 * Get active VFO using VS command.
 * Response: VS0; (MAIN) or VS1; (SUB)
 */
int ftx1_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct newcat_priv_data *priv;
    int ret;
    int p1;

    if (!rig || !vfo)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS;");

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: VS0 or VS1 */
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

/*
 * ftx1_set_split_vfo
 *
 * Set split mode using ST command, and TX VFO using FT command.
 * Format: ST0; (off) or ST1; (on), FT0; (MAIN TX) or FT1; (SUB TX)
 */
int ftx1_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct newcat_priv_data *priv;
    int ret;
    int p1;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: split=%d tx_vfo=%s\n", __func__,
              split, rig_strvfo(tx_vfo));

    /* Set split on/off */
    p1 = (split == RIG_SPLIT_ON) ? 1 : 0;
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ST%d;", p1);

    ret = newcat_set_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Set TX VFO if split is on */
    if (split == RIG_SPLIT_ON)
    {
        p1 = (tx_vfo == RIG_VFO_SUB || tx_vfo == RIG_VFO_B) ? 1 : 0;
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FT%d;", p1);
        ret = newcat_set_cmd(rig);
    }

    return ret;
}

/*
 * ftx1_get_split_vfo
 *
 * Get split mode using ST command, and TX VFO using FT command.
 * Response: ST0/ST1 for split, FT0/FT1 for TX VFO
 */
int ftx1_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct newcat_priv_data *priv;
    int ret;
    int p1;

    if (!rig || !split || !tx_vfo)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Get split status */
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

    /* Get TX VFO */
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

/*
 * ftx1_vfo_op
 *
 * VFO operations: AB (copy A->B), BA (copy B->A), XCHG (swap), BAND_UP, BAND_DOWN
 * Commands: AB; BA; SV; BU0; BD0;
 */
int ftx1_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct newcat_priv_data *priv;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

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
        /* BU: Band Up (P1=VFO: 0=MAIN, 1=SUB) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BU%d;", FTX1_VFO_TO_P1(vfo));
        break;

    case RIG_OP_BAND_DOWN:
        /* BD: Band Down (P1=VFO: 0=MAIN, 1=SUB) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BD%d;", FTX1_VFO_TO_P1(vfo));
        break;

    case RIG_OP_FROM_VFO:
        /* Store VFO to current memory channel (VFO→MEM) */
        /* AM for VFO-A, BM for VFO-B */
        if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BM;");
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AM;");
        }
        break;

    case RIG_OP_TUNE:
        /* AC110: Tuner start (turns on tuner and starts tune) */
        /* Format: AC P1 P2 P3; P1=1 (on), P2=1 (start tune), P3=0 (MAIN) */
        /* Note: This causes transmission! */
        /* Tuner is only available on SPA-1/Optima head */
        if (!ftx1_has_spa1(rig))
        {
            rig_debug(RIG_DEBUG_WARN, "%s: TUNE not available on Field Head\n",
                      __func__);
            return -RIG_ENAVAIL;
        }
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC110;");
        break;

    case RIG_OP_TO_VFO:
        /* Copy current memory channel to VFO (MEM→VFO) */
        /* MA for VFO-A, MB for VFO-B */
        if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MB;");
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MA;");
        }
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %d\n", __func__, op);
        return -RIG_EINVAL;
    }

    return newcat_set_cmd(rig);
}

/*
 * =============================================================================
 * FR Command: Function RX (Dual/Single Receive)
 * =============================================================================
 * CAT format: FR P1 P2;
 *   P1P2 = 00: Dual receive (both VFOs active)
 *   P1P2 = 01: Single receive (only selected VFO active)
 *
 * Read response: FR00 or FR01
 * Set command: FR00; or FR01;
 */

/* Dual receive mode values */
#define FTX1_FR_DUAL_RECEIVE    0   /* FR00: Both VFOs can receive */
#define FTX1_FR_SINGLE_RECEIVE  1   /* FR01: Only selected VFO receives */

/*
 * ftx1_set_dual_receive - Set dual/single receive mode
 *
 * Format: FR00; (dual) or FR01; (single)
 */
int ftx1_set_dual_receive(RIG *rig, int dual)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: dual=%d\n", __func__, dual);

    /* FR00 = dual receive, FR01 = single receive */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FR%02d;",
             dual ? FTX1_FR_DUAL_RECEIVE : FTX1_FR_SINGLE_RECEIVE);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_dual_receive - Get dual/single receive mode
 *
 * Response: FR00 (dual) or FR01 (single)
 */
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

/*
 * =============================================================================
 * Split Frequency and Mode Functions
 * =============================================================================
 * These functions set/get the TX frequency and mode during split operation.
 * The FTX-1 uses VFO-B (SUB) as the TX VFO when split is enabled.
 */

/*
 * ftx1_set_split_freq - Set TX frequency for split operation
 *
 * Sets the frequency on VFO-B (TX VFO during split).
 * Format: FB000014074; (9 digits, Hz)
 */
int ftx1_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct newcat_priv_data *priv;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tx_freq=%.0f\n", __func__, tx_freq);

    /* Set VFO-B (TX VFO) frequency */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FB%09.0f;", tx_freq);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_split_freq - Get TX frequency for split operation
 *
 * Gets the frequency from VFO-B (TX VFO during split).
 * Response: FB000014074; (9 digits, Hz)
 */
int ftx1_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct newcat_priv_data *priv;
    int ret;

    if (!rig || !tx_freq)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query VFO-B frequency */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FB;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: FB000014074 (FB + 9 digits) */
    if (sscanf(priv->ret_data + 2, "%lf", tx_freq) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tx_freq=%.0f\n", __func__, *tx_freq);

    return RIG_OK;
}

/*
 * ftx1_set_split_mode - Set TX mode for split operation
 *
 * Sets the mode on VFO-B (TX VFO during split).
 * Format: MD1 P2; where P2 is mode code
 */
int ftx1_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
    struct newcat_priv_data *priv;
    char mode_char;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tx_mode=%s\n", __func__,
              rig_strrmode(tx_mode));

    /* Map Hamlib mode to FTX-1 mode code */
    switch (tx_mode)
    {
    case RIG_MODE_LSB:      mode_char = '1'; break;
    case RIG_MODE_USB:      mode_char = '2'; break;
    case RIG_MODE_CW:       mode_char = '3'; break;
    case RIG_MODE_FM:       mode_char = '4'; break;
    case RIG_MODE_AM:       mode_char = '5'; break;
    case RIG_MODE_RTTY:     mode_char = '6'; break;
    case RIG_MODE_CWR:      mode_char = '7'; break;
    case RIG_MODE_PKTLSB:   mode_char = '8'; break;
    case RIG_MODE_RTTYR:    mode_char = '9'; break;
    case RIG_MODE_PKTFM:    mode_char = 'A'; break;
    case RIG_MODE_FMN:      mode_char = 'B'; break;
    case RIG_MODE_PKTUSB:   mode_char = 'C'; break;
    case RIG_MODE_AMN:      mode_char = 'D'; break;
    case RIG_MODE_PSK:      mode_char = 'E'; break;
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(tx_mode));
        return -RIG_EINVAL;
    }

    /* Set mode on VFO-B (SUB = 1) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MD1%c;", mode_char);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_split_mode - Get TX mode for split operation
 *
 * Gets the mode from VFO-B (TX VFO during split).
 * Response: MD1P2; where P2 is mode code
 */
int ftx1_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
    struct newcat_priv_data *priv;
    int ret;
    char mode_char;

    if (!rig || !tx_mode || !tx_width)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query mode on VFO-B (SUB = 1) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MD1;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: MD1X where X is mode code */
    if (strlen(priv->ret_data) < 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    mode_char = priv->ret_data[3];

    /* Map FTX-1 mode code to Hamlib mode */
    switch (mode_char)
    {
    case '1': *tx_mode = RIG_MODE_LSB;    break;
    case '2': *tx_mode = RIG_MODE_USB;    break;
    case '3': *tx_mode = RIG_MODE_CW;     break;
    case '4': *tx_mode = RIG_MODE_FM;     break;
    case '5': *tx_mode = RIG_MODE_AM;     break;
    case '6': *tx_mode = RIG_MODE_RTTY;   break;
    case '7': *tx_mode = RIG_MODE_CWR;    break;
    case '8': *tx_mode = RIG_MODE_PKTLSB; break;
    case '9': *tx_mode = RIG_MODE_RTTYR;  break;
    case 'A': *tx_mode = RIG_MODE_PKTFM;  break;
    case 'B': *tx_mode = RIG_MODE_FMN;    break;
    case 'C': *tx_mode = RIG_MODE_PKTUSB; break;
    case 'D': *tx_mode = RIG_MODE_AMN;    break;
    case 'E': *tx_mode = RIG_MODE_PSK;    break;
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown mode code '%c'\n", __func__,
                  mode_char);
        return -RIG_EPROTO;
    }

    /* Default width - actual width would need SH command query */
    *tx_width = RIG_PASSBAND_NORMAL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tx_mode=%s\n", __func__,
              rig_strrmode(*tx_mode));

    return RIG_OK;
}
