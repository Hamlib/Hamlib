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

    /*
     * Clear the cache fix flag if set - we're querying the radio directly
     * so the cache will be updated with the correct value by the caller.
     */
    if (priv->ftx1_cache_fix_needed)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: clearing cache fix flag (querying radio)\n", __func__);
        priv->ftx1_cache_fix_needed = 0;
    }

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

    /* Use strtod with validation instead of atof */
    char *endptr;
    *freq = strtod(priv->ret_data + 2, &endptr);
    if (endptr == priv->ret_data + 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse frequency from '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

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

/*
 * =============================================================================
 * Repeater Offset Frequency Functions
 * =============================================================================
 * FTX-1 stores repeater offset frequencies in EX menu items per band:
 *   EX010316 (TOK_FM_RPT_SHIFT_28)  - 28 MHz band (0-1000 kHz)
 *   EX010317 (TOK_FM_RPT_SHIFT_50)  - 50 MHz band (0-4000 kHz)
 *   EX010318 (TOK_FM_RPT_SHIFT_144) - 144 MHz band (0-100 x 10 kHz = 0-1000 kHz)
 *   EX010319 (TOK_FM_RPT_SHIFT_430) - 430 MHz band (0-100 x 100 kHz = 0-10000 kHz)
 *
 * The menu values are in different units per band.
 */

#include "ftx1.h"
#include "ftx1_menu.h"

/*
 * ftx1_get_band_offset_token - Get menu token for current band's repeater offset
 *
 * Returns the token for the appropriate band, or 0 if not applicable.
 * Also returns the multiplier to convert menu value to Hz.
 */
static hamlib_token_t ftx1_get_band_offset_token(freq_t freq, int *multiplier)
{
    if (freq >= MHz(420) && freq <= MHz(470))
    {
        /* 430 MHz band - menu value is in 100 kHz units */
        *multiplier = 100000;
        return TOK_FM_RPT_SHIFT_430;
    }
    else if (freq >= MHz(144) && freq <= MHz(148))
    {
        /* 144 MHz band - menu value is in 10 kHz units */
        *multiplier = 10000;
        return TOK_FM_RPT_SHIFT_144;
    }
    else if (freq >= MHz(50) && freq <= MHz(54))
    {
        /* 50 MHz band - menu value is in kHz */
        *multiplier = 1000;
        return TOK_FM_RPT_SHIFT_50;
    }
    else if (freq >= MHz(28) && freq <= MHz(30))
    {
        /* 28 MHz band - menu value is in kHz */
        *multiplier = 1000;
        return TOK_FM_RPT_SHIFT_28;
    }

    /* Band doesn't support repeater offset */
    *multiplier = 0;
    return 0;
}

/*
 * ftx1_set_rptr_offs - Set repeater offset frequency
 *
 * Sets the offset for the current band via EX menu.
 * Requires reading current frequency to determine which band's offset to set.
 */
int ftx1_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    int ret;
    freq_t freq;
    hamlib_token_t token;
    int multiplier;
    value_t val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s offs=%ld\n",
              __func__, rig_strvfo(vfo), offs);

    /* Get current frequency to determine band */
    ret = ftx1_get_freq(rig, vfo, &freq);
    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to get frequency\n", __func__);
        return ret;
    }

    /* Get appropriate token for this band */
    token = ftx1_get_band_offset_token(freq, &multiplier);
    if (token == 0 || multiplier == 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: band at %.0f Hz doesn't support repeater offset\n",
                  __func__, freq);
        return -RIG_ENAVAIL;
    }

    /* Convert Hz offset to menu value (use val.f for RIG_CONF_NUMERIC) */
    val.f = (float)(offs / multiplier);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%.0f token=0x%lx val=%.0f (offs=%ld, mult=%d)\n",
              __func__, freq, (unsigned long)token, val.f, offs, multiplier);

    /* Set via menu system */
    return ftx1_menu_set_token(rig, token, val);
}

/*
 * ftx1_get_rptr_offs - Get repeater offset frequency
 *
 * Gets the offset for the current band via EX menu.
 * Requires reading current frequency to determine which band's offset to read.
 */
int ftx1_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    int ret;
    freq_t freq;
    hamlib_token_t token;
    int multiplier;
    value_t val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    /* Get current frequency to determine band */
    ret = ftx1_get_freq(rig, vfo, &freq);
    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to get frequency\n", __func__);
        return ret;
    }

    /* Get appropriate token for this band */
    token = ftx1_get_band_offset_token(freq, &multiplier);
    if (token == 0 || multiplier == 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: band at %.0f Hz doesn't support repeater offset\n",
                  __func__, freq);
        *offs = 0;
        return RIG_OK;  /* Return 0 offset for bands without repeater */
    }

    /* Get via menu system */
    ret = ftx1_menu_get_token(rig, token, &val);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Convert menu value to Hz offset (val.f for RIG_CONF_NUMERIC) */
    *offs = (shortfreq_t)val.f * multiplier;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%.0f token=0x%lx val=%.0f offs=%ld\n",
              __func__, freq, (unsigned long)token, val.f, *offs);

    return RIG_OK;
}

