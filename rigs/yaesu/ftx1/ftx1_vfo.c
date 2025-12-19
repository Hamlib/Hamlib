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
 *
 * Note: BS (Band Select) returns '?' in firmware - not implemented.
 *       AB, BA, SV handled via newcat_vfo_op.
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "../ftx1.h"

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
        /* MV: Memory to VFO - copy current memory channel to VFO */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MV;");
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
