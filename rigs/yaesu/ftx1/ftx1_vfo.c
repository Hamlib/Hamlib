/*
 * Hamlib Yaesu backend - FTX-1 VFO Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for VFO management and split operation.
 *
 * CAT Commands in this file:
 *   VS P1;    - VFO Select (0=MAIN, 1=SUB)
 *   FT P1;    - Function TX VFO (0=MAIN TX, 1=SUB TX)
 *   FR P1P2;  - Function RX (00=dual receive, 01=single receive)
 *   FA/FB     - VFO-A/B frequency (used for split_freq)
 *   MD P1 P2; - Mode (used for split_mode, P1=VFO)
 *
 * Note: ST (Split) command NOT used - see ftx1_set_split_vfo() for details.
 *       BS (Band Select) returns '?' in firmware - not implemented.
 *       AB, BA, SV handled via ftx1_vfo_op.
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "cache.h"
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
 * Set split mode using virtual split (no ST command to radio).
 *
 * The FTX-1 has a firmware issue where enabling hardware split (ST1)
 * forces the SUB VFO into TX/PTT state, locking its frequency. This
 * causes problems for sat. tracking apps like GPredict.
 *
 * Workaround: We track split state internally and only use the FT
 * command to select which VFO transmits. The radio never enters
 * hardware split mode, so SUB VFO remains controllable.
 *
 * VFO token mapping (FTX1 uses MAIN/SUB, not A/B):
 *   Main      → MAIN (TX on main VFO)
 *   Sub       → SUB  (TX on sub VFO)
 *   VFOA      → MAIN (A maps to MAIN)
 *   VFOB, "1" → SUB  (B and numeric "1" map to SUB for split)
 *
 * Note: GPredict sends "1" which rig_parse_vfo() maps to RIG_VFO_A.
 * Since "1" is intended to mean "TX on alternate VFO" for split,
 * we treat RIG_VFO_A as SUB when split is ON.
 *
 * Format: FT0; (MAIN TX) or FT1; (SUB TX)
 */
int ftx1_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s: split=%d tx_vfo=%s\n", __func__,
              split, rig_strvfo(tx_vfo));

    /*
     * FTX1 split configuration:
     *   - Split ON:  RX on MAIN, TX on SUB (the only supported config)
     *   - Split OFF: TX on MAIN (same as RX)
     *
     * CRITICAL: The Hamlib core's vfo_fixup() converts "Main" to "VFOA".
     * Since VFOA and MAIN share the same cache slot (freqMainA), we MUST
     * force tx_vfo to SUB when split is ON. Otherwise:
     *   1. rig_set_split_freq caches TX freq for VFOA
     *   2. This overwrites the MAIN cache with the TX frequency
     *   3. rig_get_freq returns TX freq instead of RX freq
     *
     * By forcing tx_vfo=SUB here, and updating STATE(rig)->tx_vfo, we ensure
     * that subsequent rig_set_split_freq calls cache for SUB (not VFOA/MAIN).
     *
     * Note: The core will overwrite rs->tx_vfo after this function returns,
     * but we also fix it in ftx1_set_split_freq as a backup.
     */
    if (split == RIG_SPLIT_ON)
    {
        tx_vfo = RIG_VFO_SUB;
    }
    else
    {
        tx_vfo = RIG_VFO_MAIN;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: normalized tx_vfo=%s\n", __func__,
              rig_strvfo(tx_vfo));

    /* Fix rs->tx_vfo - the core may overwrite this, but try anyway */
    STATE(rig)->tx_vfo = tx_vfo;

    /* Store virtual split state - do NOT send ST command to radio */
    priv->ftx1_virtual_split = (split == RIG_SPLIT_ON) ? 1 : 0;
    priv->ftx1_tx_vfo = tx_vfo;

    /* Set TX VFO using FT command: 0=MAIN TX, 1=SUB TX */
    p1 = (tx_vfo == RIG_VFO_SUB) ? 1 : 0;
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FT%d;", p1);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_split_vfo
 *
 * Get split mode from virtual split state, TX VFO from FT command.
 *
 * Returns the internally tracked virtual split state (not hardware ST).
 * TX VFO is queried from radio via FT command.
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

    /* Return virtual split state */
    *split = priv->ftx1_virtual_split ? RIG_SPLIT_ON : RIG_SPLIT_OFF;

    /* Get actual TX VFO from radio */
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
        /*
         * VFO Copy: Direction depends on source VFO parameter
         *   AB; copies VFO-A (MAIN) to VFO-B (SUB)
         *   BA; copies VFO-B (SUB) to VFO-A (MAIN)
         */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                 ftx1_vfo_to_p1(rig, vfo) ? "BA;" : "AB;");
        break;

    case RIG_OP_XCHG:
        /* SV: Swap VFO A and B */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SV;");
        break;

    case RIG_OP_BAND_UP:
        /* BU: Band Up (P1=VFO: 0=MAIN, 1=SUB) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BU%d;", ftx1_vfo_to_p1(rig, vfo));
        break;

    case RIG_OP_BAND_DOWN:
        /* BD: Band Down (P1=VFO: 0=MAIN, 1=SUB) */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BD%d;", ftx1_vfo_to_p1(rig, vfo));
        break;

    case RIG_OP_FROM_VFO:
        /* Store VFO to current memory channel (VFO→MEM) */
        /* AM for VFO-A, BM for VFO-B */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                 ftx1_vfo_to_p1(rig, vfo) ? "BM;" : "AM;");
        break;

    case RIG_OP_TUNE:
        /*
         * AC command format: AC P1 P2 P3;
         *   P1=1 (tuner on), P2=1 (start tune), P3=0 (antenna)
         *
         * Note: Current FTX-1 firmware returns '?' for AC set commands.
         * We try anyway in case future firmware updates fix this.
         * Tuner is only available on SPA-1/Optima configuration.
         */
        if (!ftx1_has_spa1(rig))
        {
            rig_debug(RIG_DEBUG_WARN, "%s: TUNE requires SPA-1 amplifier\n",
                      __func__);
            return -RIG_ENAVAIL;
        }
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC110;");
        break;

    case RIG_OP_TO_VFO:
        /* Copy current memory channel to VFO (MEM→VFO) */
        /* MA for VFO-A, MB for VFO-B */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                 ftx1_vfo_to_p1(rig, vfo) ? "MB;" : "MA;");
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
    struct rig_cache *cachep;
    freq_t saved_main_freq;
    int ret;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    cachep = CACHE(rig);
    if (!priv || !cachep)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tx_freq=%.0f\n", __func__, tx_freq);

    /*
     * Save the Main freq before any cache corruption.
     * The Hamlib core will cache tx_freq for VFOA after we return,
     * which corrupts the Main cache (they share freqMainA slot).
     * We save it here so we can restore it below.
     */
    saved_main_freq = cachep->freqMainA;

    /* Set VFO-B (TX VFO) frequency */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FB%09.0f;", tx_freq);
    ret = newcat_set_cmd(rig);

    if (ret == RIG_OK)
    {
        /*
         * Fix rs->tx_vfo for future operations.
         * The core set it to VFOA in rig_set_split_vfo, but FTX1 uses SUB.
         */
        STATE(rig)->tx_vfo = RIG_VFO_SUB;

        /*
         * Cache the TX freq for SUB (where we actually set it).
         * This ensures rig_get_split_freq returns the correct value.
         */
        rig_set_cache_freq(rig, RIG_VFO_SUB, tx_freq);

        /*
         * WORKAROUND for Hamlib core cache issue:
         * The core will cache tx_freq for VFOA AFTER we return, which
         * corrupts the Main cache (VFOA and MAIN share freqMainA slot).
         *
         * We save the correct Main freq here. It will be restored in
         * ftx1_get_split_freq, which GPredict calls before get_freq.
         */
        priv->ftx1_cache_fix_freq = saved_main_freq;
        priv->ftx1_cache_fix_needed = 1;
    }

    return ret;
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
    struct rig_cache *cachep;
    int ret;

    if (!rig || !tx_freq)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    cachep = CACHE(rig);
    if (!priv || !cachep)
    {
        return -RIG_EINTERNAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /*
     * WORKAROUND for Hamlib core cache issue:
     * If ftx1_set_split_freq saved the Main freq, restore it now.
     * This happens before rig_get_freq is called, so the cache will
     * return the correct RX frequency.
     */
    if (priv->ftx1_cache_fix_needed)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: restoring Main cache to %.0f Hz\n",
                  __func__, priv->ftx1_cache_fix_freq);
        cachep->freqMainA = priv->ftx1_cache_fix_freq;
        elapsed_ms(&cachep->time_freqMainA, HAMLIB_ELAPSED_SET);
        priv->ftx1_cache_fix_needed = 0;
    }

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
