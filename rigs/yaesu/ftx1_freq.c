/*
 * Hamlib Yaesu backend - FTX-1 Frequency Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for frequency setting/getting.
 *
 * CAT Commands in this file:
 *   FA P1...P9;  - VFO-A Frequency (9-digit Hz format)
 *   FB P1...P9;  - VFO-B Frequency (9-digit Hz format)
 *   OS P1 P2;    - Offset (Repeater Shift) - simplex, +, -, ARS
 *
 * Note: CF (Clarifier) command returns '?' in firmware - not implemented.
 *       RIT/XIT handled via newcat functions using RI/XI commands.
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"

// FTX-1 uses 9-digit Hz format for frequency
#define FTX1_FREQ_DIGITS 9

/*
 * ftx1_set_freq
 *
 * Set frequency on specified VFO using FA (MAIN) or FB (SUB) command.
 * Format: FA000014074; or FB000014074; (9 digits, Hz)
 */
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

/*
 * ftx1_get_freq
 *
 * Get frequency from specified VFO using FA (MAIN) or FB (SUB) command.
 * Response format: FA000014074; or FB000014074; (9 digits, Hz)
 */
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

/*
 * =============================================================================
 * OS Command: Offset (Repeater Shift)
 * =============================================================================
 * CAT format: OS P1 P2;
 *   P1 = VFO (0=MAIN, 1=SUB)
 *   P2 = Shift mode:
 *        0 = Simplex (no shift)
 *        1 = Plus shift (+)
 *        2 = Minus shift (-)
 *        3 = Automatic Repeater Shift (ARS)
 *
 * Read response: OS0n where n is shift mode
 * Set command: OSnn;
 *
 * Note: This command only works in FM mode.
 */

/* Repeater shift mode values */
#define FTX1_OS_SIMPLEX  0   /* Simplex (no shift) */
#define FTX1_OS_PLUS     1   /* Plus shift (+) */
#define FTX1_OS_MINUS    2   /* Minus shift (-) */
#define FTX1_OS_ARS      3   /* Automatic Repeater Shift */

/*
 * ftx1_set_rptr_shift - Set repeater shift mode
 *
 * shift: 0=simplex, 1=plus, 2=minus, 3=ARS (see RIG_RPT_SHIFT_*)
 */
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

/*
 * ftx1_get_rptr_shift - Get repeater shift mode
 */
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

