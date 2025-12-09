/*
 * Hamlib Yaesu backend - FTX-1 Frequency Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for frequency setting/getting.
 *
 * CAT Commands in this file:
 *   FA P1...P9;  - VFO-A Frequency (9-digit Hz format)
 *   FB P1...P9;  - VFO-B Frequency (9-digit Hz format)
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
 * ftx1_get_rit_info - Get RIT Information (RI P1;)
 * CAT command: RI P1; Response: RI P1 P2P3P4P5P6P7P8;
 *   P1 = VFO (0=MAIN, 1=SUB)
 *   P2 = RIT on/off (0/1)
 *   P3 = XIT on/off (0/1)
 *   P4 = Direction (+/-)
 *   P5-P8 = Offset frequency (4 digits, 0000-9999 Hz)
 *
 * Example: RI00100123 = MAIN, RIT=off, XIT=on, +, 0123 Hz
 */
int ftx1_get_rit_info(RIG *rig, vfo_t vfo, int *rit_on, int *xit_on, int *offset_hz)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, rit, xit;
    char dir;
    int freq;

    /* Determine P1 based on VFO */
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
        p1 = 0;
        break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RI%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: RI0 P2 P3 P4 P5P6P7P8; (7 chars after RI0) */
    /* Example: RI00100123 */
    if (sscanf(priv->ret_data + 2, "%1d%1d%1d%c%4d", &p1, &rit, &xit, &dir, &freq) != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    if (rit_on) *rit_on = rit;
    if (xit_on) *xit_on = xit;
    if (offset_hz)
    {
        *offset_hz = (dir == '-') ? -freq : freq;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: rit=%d xit=%d offset=%d\n", __func__,
              rit, xit, (dir == '-') ? -freq : freq);

    return RIG_OK;
}
