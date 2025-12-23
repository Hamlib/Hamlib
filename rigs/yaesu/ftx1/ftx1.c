/*
 * Hamlib Yaesu backend - FTX-1
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
 * ===========================================================================
 * ACKNOWLEDGMENTS
 * ===========================================================================
 * Special thanks to Jeremy Miller (KO4SSD) for his invaluable contributions
 * to FTX-1 support in Hamlib (PR #1826). Jeremy discovered critical workarounds
 * for FTX-1 firmware limitations:
 *
 *   - RIT/XIT: The standard RT/XT commands return '?' on FTX-1. Jeremy figured
 *     out that the RC (Receiver Clarifier) and TC (Transmit Clarifier) commands
 *     work correctly, and that the IF response can be parsed for clarifier state.
 *
 *   - Tuning Steps: Jeremy implemented mode-specific dial steps via the EX0306
 *     extended menu command, providing finer control than the basic TS command.
 *
 * This implementation incorporates Jeremy's discoveries with gratitude.
 * His persistence in testing with actual hardware and willingness to share
 * findings with the community made complete FTX-1 support possible.
 *
 * Jeremy's original implementation: https://github.com/Hamlib/Hamlib/pull/1826
 * ===========================================================================
 *
 * FIRMWARE NOTES (v1.08+):
 * - RT (RIT on/off) and XT (XIT on/off) commands return '?' - use RC/TC instead
 *   (discovered by Jeremy Miller KO4SSD)
 * - CF (Clarifier) sets offset value only, does not enable/disable clarifier
 *   Format: CF P1 P2 P3 [+/-] PPPP where P3 must be 1
 *   Example: CF001+0500 sets clarifier offset to +500Hz
 * - ZI (Zero In) only works in CW mode (MD03 or MD07)
 * - BS (Band Select) is set-only - no read/query capability
 * - GP (GP OUT) requires menu: [OPERATION SETTING] → [GENERAL] → [TUN/LIN PORT SELECT] = "GPO"
 *   Format: GP P1 P2 P3 P4 where each controls GP OUT A/B/C/D (0=LOW, 1=HIGH)
 * - MR/MT/MZ (Memory) use 5-digit format, not 4-digit as documented
 *   Example: MR00001 (not MR0001) for channel 1
 * - MT (Memory Tag) is read/write: MT00001NAME sets 12-char name
 * - MZ (Memory Zone) is read/write: MZ00001DATA sets 10-digit zone data
 * - MC (Memory Channel) uses different format than documented:
 *   Read: MC0 (MAIN) or MC1 (SUB) returns MCNNNNNN (6-digit channel)
 *   Set: MCNNNNNN (6-digit channel, no VFO prefix)
 *   Returns '?' if channel doesn't exist (not programmed)
 * - CH (Memory Channel Up/Down) works with CH0/CH1 only:
 *   CH0 = next channel, CH1 = previous channel (cycles through all groups)
 *   Cycles: PMG (00xxxx) → QMB (05xxxx) → PMG (wraps around)
 *   CH; CH00; CH01; etc. return '?' - only CH0 and CH1 work
 * - VM mode codes differ from spec: 00=VFO, 11=Memory (not 01 as documented)
 *   Only VM000 set works; use SV command to toggle to memory mode
 */

#include <stdlib.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "sprintflst.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"
#include "ftx1_menu.h"

/* Private caps for newcat framework */
static const struct newcat_priv_caps ftx1_priv_caps = {
    .roofing_filter_count = 0,
};

/* Extern declarations for group-specific functions (add more as groups are implemented) */
extern int ftx1_set_vfo(RIG *rig, vfo_t vfo);
extern int ftx1_get_vfo(RIG *rig, vfo_t *vfo);
extern int ftx1_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
extern int ftx1_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
/* Additional externs for group 4 */
extern int ftx1_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
extern int ftx1_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
extern int ftx1_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
extern int ftx1_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
/* Additional externs for group 6 */
extern int ftx1_set_preamp_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_preamp_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_att_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_att_helper(RIG *rig, vfo_t vfo, value_t *val);

/* Wrappers from ftx1_func.c for rig caps */
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
extern int ftx1_wait_morse_func(RIG *rig, vfo_t vfo);
extern int ftx1_set_trn_func(RIG *rig, int trn);
extern int ftx1_get_trn_func(RIG *rig, int *trn);
extern int ftx1_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);

/* Externs from ftx1_mem.c */
extern int ftx1_set_mem(RIG *rig, vfo_t vfo, int ch);
extern int ftx1_get_mem(RIG *rig, vfo_t vfo, int *ch);
extern int ftx1_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
extern int ftx1_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);

/* Externs from ftx1_scan.c */
extern int ftx1_set_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
extern int ftx1_get_scan(RIG *rig, vfo_t vfo, scan_t *scan, int *ch);
extern int ftx1_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
extern int ftx1_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);

/*
 * Externs from ftx1_clarifier.c
 * RIT/XIT implementation by Jeremy Miller (KO4SSD) - uses RC/TC commands
 * instead of RT/XT which return '?' on FTX-1
 */
extern int ftx1_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
extern int ftx1_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
extern int ftx1_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);
extern int ftx1_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);

/* Externs from ftx1_freq.c */
extern int ftx1_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
extern int ftx1_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
extern int ftx1_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
extern int ftx1_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift);
extern int ftx1_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
extern int ftx1_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs);

/* Externs from ftx1_vfo.c */
extern int ftx1_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
extern int ftx1_set_dual_receive(RIG *rig, int dual);
extern int ftx1_get_dual_receive(RIG *rig, int *dual);
extern int ftx1_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
extern int ftx1_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
extern int ftx1_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width);
extern int ftx1_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width);

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
        /* Use strtof() for proper error handling (atof returns 0 on error) */
        char *endptr;
        float power_value = strtof(priv->ret_data + 3, &endptr);

        /* Validate that we parsed at least one character */
        if (endptr == priv->ret_data + 3)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: failed to parse power value from '%s'\n",
                      __func__, priv->ret_data);
            power_accepted = 0;
        }
        else
        {
            power_accepted = (power_value >= 8.0f);
            rig_debug(RIG_DEBUG_VERBOSE, "%s: power readback: %.1f, accepted=%d\n",
                      __func__, power_value, power_accepted);
        }
    }

    /* Restore original power setting - retry on failure */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", original_power);
    ret = newcat_set_cmd(rig);

    if (ret != RIG_OK)
    {
        /* First attempt failed, wait and retry */
        rig_debug(RIG_DEBUG_WARN, "%s: first restore attempt failed, retrying\n",
                  __func__);
        hl_usleep(100000);  /* 100ms delay */
        ret = newcat_set_cmd(rig);

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: CRITICAL - failed to restore original power setting '%s'\n",
                      __func__, original_power);
            /* Radio may be left at 8W - log prominently but continue */
        }
    }

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
    struct newcat_priv_data *priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Call the standard newcat open first */
    ret = newcat_open(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: newcat priv data not initialized\n", __func__);
        return -RIG_EINTERNAL;
    }

    /* Auto-detect head type and SPA-1 presence - store in per-rig instance */
    priv->ftx1_head_type = ftx1_detect_head_type(rig);
    priv->ftx1_spa1_detected = ftx1_detect_spa1(rig);
    priv->ftx1_detection_done = 1;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: detection complete - head_type=%d spa1_detected=%d\n",
              __func__, priv->ftx1_head_type, priv->ftx1_spa1_detected);

    /* Cross-check: if head_type is SPA-1, spa1_detected should also be true */
    if (priv->ftx1_head_type == FTX1_HEAD_SPA1 && !priv->ftx1_spa1_detected)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: PC reports SPA-1 but VE4 detection failed\n", __func__);
    }

    return RIG_OK;
}

/*
 * ftx1_close - FTX-1 specific rig close
 *
 * Resets detection state so reopening the rig will re-detect head configuration.
 * This handles scenarios where the head is swapped while the rig is powered off.
 */
static int ftx1_close(RIG *rig)
{
    struct newcat_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = STATE(rig)->priv;
    if (priv)
    {
        /* Reset detection state for next open */
        priv->ftx1_head_type = FTX1_HEAD_UNKNOWN;
        priv->ftx1_spa1_detected = 0;
        priv->ftx1_detection_done = 0;
    }

    return newcat_close(rig);
}

/*
 * ftx1_has_spa1 - Check if Optima/SPA-1 amplifier is present
 *
 * Returns 1 if Optima/SPA-1 detected, 0 otherwise.
 * Used by tuner and power control functions for guardrails.
 */
int ftx1_has_spa1(RIG *rig)
{
    struct newcat_priv_data *priv;

    if (!rig)
    {
        return 0;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return 0;
    }

    return priv->ftx1_spa1_detected ||
           priv->ftx1_head_type == FTX1_HEAD_SPA1;
}

/*
 * ftx1_get_head_type - Get detected head type
 *
 * Returns FTX1_HEAD_FIELD_BATTERY, FTX1_HEAD_FIELD_12V, FTX1_HEAD_SPA1,
 * or FTX1_HEAD_UNKNOWN
 */
int ftx1_get_head_type(RIG *rig)
{
    struct newcat_priv_data *priv;

    if (!rig)
    {
        return FTX1_HEAD_UNKNOWN;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return FTX1_HEAD_UNKNOWN;
    }

    return priv->ftx1_head_type;
}

/*
 * ftx1_set_ext_parm - Set FTX-1 extended menu parameter
 *
 * Uses the menu system to set any EX command by token.
 * Token encodes the EX command address (group/section/item).
 */
static int ftx1_set_ext_parm(RIG *rig, hamlib_token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: token=0x%lx\n", __func__,
              (unsigned long)token);

    return ftx1_menu_set_token(rig, token, val);
}

/*
 * ftx1_get_ext_parm - Get FTX-1 extended menu parameter
 *
 * Uses the menu system to get any EX command by token.
 * Token encodes the EX command address (group/section/item).
 */
static int ftx1_get_ext_parm(RIG *rig, hamlib_token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: token=0x%lx\n", __func__,
              (unsigned long)token);

    return ftx1_menu_get_token(rig, token, val);
}

/* Rig caps structure */
struct rig_caps ftx1_caps = {
    .rig_model = RIG_MODEL_FTX1,
    .model_name = "FTX-1",
    .mfg_name = "Yaesu",
    .version = "20251219.0",  /* Date-based version - added full EX menu support */
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,  /* Update to stable once complete */
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,  /* FTX-1 RI command P8 provides squelch status */
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,  /* 8N1 per FTX-1 default */
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 50,  /* Delay after write for FTX-1 response time */
    .timeout = 2000,
    .retry = 3,
    /* Note: ARO, FAGC, DIVERSITY return '?' from FTX-1 firmware - not supported */
    /* DUAL_WATCH uses FR command for dual/single receive mode */
    /* Contour (CO command) is FTX-1 specific - handle via ext_level */
    /* Note: NA command (narrow filter) available via ftx1_set_na_helper/ftx1_get_na_helper but no RIG_FUNC_NAR exists in Hamlib */
    .has_get_func = RIG_FUNC_COMP | RIG_FUNC_VOX | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SBKIN | RIG_FUNC_FBKIN | RIG_FUNC_NB | RIG_FUNC_NR | RIG_FUNC_MN | RIG_FUNC_LOCK | RIG_FUNC_MON | RIG_FUNC_TUNER | RIG_FUNC_RIT | RIG_FUNC_XIT | RIG_FUNC_APF | RIG_FUNC_ANF | RIG_FUNC_DUAL_WATCH,
    .has_set_func = RIG_FUNC_COMP | RIG_FUNC_VOX | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SBKIN | RIG_FUNC_FBKIN | RIG_FUNC_NB | RIG_FUNC_NR | RIG_FUNC_MN | RIG_FUNC_LOCK | RIG_FUNC_MON | RIG_FUNC_TUNER | RIG_FUNC_RIT | RIG_FUNC_XIT | RIG_FUNC_APF | RIG_FUNC_ANF | RIG_FUNC_DUAL_WATCH,
    .has_get_level = RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_SQL | RIG_LEVEL_IF | RIG_LEVEL_APF | RIG_LEVEL_NB | RIG_LEVEL_NR | RIG_LEVEL_PBT_IN | RIG_LEVEL_PBT_OUT | RIG_LEVEL_RFPOWER | RIG_LEVEL_MICGAIN | RIG_LEVEL_KEYSPD | RIG_LEVEL_NOTCHF | RIG_LEVEL_COMP | RIG_LEVEL_AGC | RIG_LEVEL_BKINDL | RIG_LEVEL_BALANCE | RIG_LEVEL_METER | RIG_LEVEL_VOXGAIN | RIG_LEVEL_VOXDELAY | RIG_LEVEL_ANTIVOX | RIG_LEVEL_RAWSTR | RIG_LEVEL_SWR | RIG_LEVEL_ALC | RIG_LEVEL_STRENGTH | RIG_LEVEL_ATT | RIG_LEVEL_PREAMP | RIG_LEVEL_MONITOR_GAIN | RIG_LEVEL_CWPITCH,
    .has_set_level = RIG_LEVEL_SET(RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_SQL | RIG_LEVEL_IF | RIG_LEVEL_APF | RIG_LEVEL_NB | RIG_LEVEL_NR | RIG_LEVEL_PBT_IN | RIG_LEVEL_PBT_OUT | RIG_LEVEL_RFPOWER | RIG_LEVEL_MICGAIN | RIG_LEVEL_KEYSPD | RIG_LEVEL_NOTCHF | RIG_LEVEL_COMP | RIG_LEVEL_AGC | RIG_LEVEL_BKINDL | RIG_LEVEL_BALANCE | RIG_LEVEL_METER | RIG_LEVEL_VOXGAIN | RIG_LEVEL_VOXDELAY | RIG_LEVEL_ANTIVOX | RIG_LEVEL_RAWSTR | RIG_LEVEL_SWR | RIG_LEVEL_ALC | RIG_LEVEL_STRENGTH | RIG_LEVEL_ATT | RIG_LEVEL_PREAMP | RIG_LEVEL_MONITOR_GAIN | RIG_LEVEL_CWPITCH),
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran = {
        /* FTX-1 specific level granularity */
        /* Suppress entries we override, then include common Yaesu defaults */
#define NO_LVL_MICGAIN
#define NO_LVL_VOXGAIN
#define NO_LVL_RFPOWER
#define NO_LVL_MONITOR_GAIN
#define NO_LVL_SQL
#define NO_LVL_CWPITCH
#define NO_LVL_NOTCHF
#define NO_LVL_BKINDL
#define NO_LVL_VOXDELAY
#include "level_gran_yaesu.h"
        /* FTX-1 overrides for levels with 0-100 range instead of 0-255 */
        [LVL_MICGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_VOXGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        /*
         * RFPOWER level_gran: FTX-1 power ranges vary by head configuration:
         *   Field Battery: 0.5-6W   (min=0.083 normalized)
         *   Field 12V:     0.5-10W  (min=0.05 normalized)
         *   Optima/SPA-1:  5-100W   (min=0.05 normalized)
         *
         * Static level_gran cannot represent all three configs. Using SPA-1/12V
         * minimum (0.05) as default. Actual power limits are enforced at runtime
         * in ftx1_set_power() and ftx1_get_power() based on detected head type.
         * Field Battery users may see 0.05 minimum in apps but hardware will
         * clamp to 0.5W (0.083 normalized).
         */
        [LVL_RFPOWER] = { .min = { .f = 0.05 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_MONITOR_GAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_SQL] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        /* CW pitch: FTX-1 KP command sets pitch 300-1050 Hz in 10Hz steps */
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 1050 }, .step = { .i = 10 } },
        /* Key speed: FTX-1 uses 4-60 WPM */
        [LVL_KEYSPD] = { .min = { .i = 4 }, .max = { .i = 60 }, .step = { .i = 1 } },
        /* Break-in delay: FTX-1 SD command uses 00-33 (non-linear, see CAT manual) */
        [LVL_BKINDL] = { .min = { .i = 0 }, .max = { .i = 33 }, .step = { .i = 1 } },
        /* VOX delay: FTX-1 VD command uses 00-33 (non-linear, see CAT manual) */
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 33 }, .step = { .i = 1 } },
        /* Notch frequency: FTX-1 uses 1-3200 Hz */
        [LVL_NOTCHF] = { .min = { .i = 1 }, .max = { .i = 3200 }, .step = { .i = 10 } },
    },
    .ctcss_list = common_ctcss_list,
    .dcs_list = common_dcs_list,
    .str_cal = FTX1_STR_CAL,
    .preamp = {10, 20, RIG_DBLST_END},  /* AMP1=10dB, AMP2=20dB (0=IPO) */
    .attenuator = {12, RIG_DBLST_END},
    .max_rit = Hz(9999),
    .max_xit = Hz(9999),
    .max_ifshift = Hz(1200),
    .agc_level_count = 5,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW, RIG_AGC_AUTO },
    .vfo_ops = RIG_OP_CPY | RIG_OP_XCHG | RIG_OP_FROM_VFO | RIG_OP_TO_VFO | RIG_OP_MCL | RIG_OP_TUNE | RIG_OP_BAND_UP | RIG_OP_BAND_DOWN,
    .targetable_vfo = RIG_TARGETABLE_ALL,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 12,  /* Tag size from manual */
    .chan_list = {
        {1, 99, RIG_MTYPE_MEM, NEWCAT_MEM_CAP},
        {100, 117, RIG_MTYPE_EDGE, NEWCAT_MEM_CAP},  /* PMS */
        {500, 503, RIG_MTYPE_MEM, NEWCAT_MEM_CAP},  /* 60m */
        RIG_CHAN_END,
    },
    .rx_range_list1 = {
        {kHz(30), MHz(56), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(118), MHz(164), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(430), MHz(470), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {kHz(1800), MHz(2) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(3.5), MHz(4) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(5.1675), MHz(5.4065), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},  /* 60m */
        {MHz(7), MHz(7.3), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(10.1), MHz(10.15), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(14), MHz(14.35), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(18.068), MHz(18.168), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(21), MHz(21.45), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(24.89), MHz(24.99), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(28), MHz(29.7), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(50), MHz(54), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(144), MHz(148), RIG_MODE_ALL, 5000, 50000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(430), MHz(450), RIG_MODE_ALL, 5000, 20000, RIG_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .rx_range_list2 = {
        {kHz(30), MHz(56), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(118), MHz(164), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(430), MHz(470), RIG_MODE_ALL, -1, -1, RIG_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        {kHz(1800), MHz(2) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(3.5), MHz(4) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(5.1675), MHz(5.4065), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},  /* 60m */
        {MHz(7), MHz(7.3), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(10.1), MHz(10.15), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(14), MHz(14.35), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(18.068), MHz(18.168), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(21), MHz(21.45), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(24.89), MHz(24.99), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(28), MHz(29.7), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(50), MHz(54), RIG_MODE_ALL, 5000, 100000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(144), MHz(148), RIG_MODE_ALL, 5000, 50000, RIG_VFO_ALL, RIG_ANT_1},
        {MHz(430), MHz(450), RIG_MODE_ALL, 5000, 20000, RIG_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .tuning_steps = {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, 10},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_PKTFM, 100},
        {RIG_MODE_WFM, kHz(25)},
        RIG_TS_END,
    },
    .filters = {
        /* FTX-1 filter widths based on SH command codes (Hz):
         *   00=200, 01=250, 02=300, 03=350, 04=400, 05=450, 06=500
         *   07=600, 08=700, 09=800, 10=900, 11=1000, 12=1200, 13=1400
         *   14=1600, 15=1800, 16=2000, 17=2200, 18=2400, 19=2600
         *   20=2800, 21=3000, 22=3200, 23=3400 */
        /* CW modes: 200Hz to 3000Hz */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},     /* Normal CW */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(200)},     /* Narrow CW */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(2400)},    /* Wide CW */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(1800)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(1200)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(800)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(400)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        /* SSB modes: 200Hz to 3000Hz */
        {RIG_MODE_SSB, Hz(2400)},    /* Normal SSB */
        {RIG_MODE_SSB, Hz(1800)},    /* Narrow SSB */
        {RIG_MODE_SSB, Hz(3000)},    /* Wide SSB */
        {RIG_MODE_SSB, Hz(2800)},
        {RIG_MODE_SSB, Hz(2600)},
        {RIG_MODE_SSB, Hz(2200)},
        {RIG_MODE_SSB, Hz(2000)},
        {RIG_MODE_SSB, Hz(1600)},
        {RIG_MODE_SSB, Hz(1400)},
        {RIG_MODE_SSB, Hz(1200)},
        /* RTTY/DATA modes: 200Hz to 2400Hz */
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(250)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(2400)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(1800)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(1200)},
        /* AM modes */
        {RIG_MODE_AM, Hz(6000)},     /* Normal AM */
        {RIG_MODE_AM, Hz(3000)},     /* Narrow AM */
        {RIG_MODE_AM, Hz(9000)},     /* Wide AM */
        /* FM modes */
        {RIG_MODE_FM | RIG_MODE_PKTFM, Hz(12000)},  /* Normal FM */
        {RIG_MODE_FM | RIG_MODE_PKTFM, Hz(9000)},   /* Narrow FM */
        /* Any mode, any filter */
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, RIG_FLT_ANY},
        RIG_FLT_END,
    },
    .priv = (void *)&ftx1_priv_caps,
    .rig_init = newcat_init,
    .rig_cleanup = newcat_cleanup,
    .rig_open = ftx1_open,
    .rig_close = ftx1_close,  /* FTX-1 specific close - resets detection state */
    .set_freq = ftx1_set_freq,
    .get_freq = ftx1_get_freq,
    .set_mode = newcat_set_mode,
    .get_mode = newcat_get_mode,
    .set_vfo = ftx1_set_vfo,
    .get_vfo = ftx1_get_vfo,
    .set_ptt = ftx1_set_ptt_func,
    .get_ptt = ftx1_get_ptt_func,
    .get_dcd = ftx1_get_dcd,  /* RI command P8 provides squelch/DCD status */
    .set_powerstat = ftx1_set_powerstat_func,
    .get_powerstat = ftx1_get_powerstat_func,
    .set_func = ftx1_set_func,
    .get_func = ftx1_get_func,
    .set_level = ftx1_set_level,
    .get_level = ftx1_get_level,
    .set_ctcss_tone = ftx1_set_ctcss_tone_func,
    .get_ctcss_tone = ftx1_get_ctcss_tone_func,
    .set_ctcss_sql = ftx1_set_ctcss_sql_func,
    .get_ctcss_sql = ftx1_get_ctcss_sql_func,
    .set_dcs_code = ftx1_set_dcs_code_func,
    .get_dcs_code = ftx1_get_dcs_code_func,
    .set_dcs_sql = ftx1_set_dcs_sql_func,
    .get_dcs_sql = ftx1_get_dcs_sql_func,
    .send_morse = ftx1_send_morse_func,
    .stop_morse = ftx1_stop_morse_func,
    .wait_morse = ftx1_wait_morse_func,
    .set_trn = ftx1_set_trn_func,
    .get_trn = ftx1_get_trn_func,
    .set_mem = ftx1_set_mem,
    .get_mem = ftx1_get_mem,
    .vfo_op = ftx1_vfo_op,
    .scan = ftx1_set_scan,
    .set_channel = ftx1_set_channel,
    .get_channel = ftx1_get_channel,
    .set_ts = ftx1_set_ts,
    .get_ts = ftx1_get_ts,
    .set_ext_level = newcat_set_ext_level,
    .get_ext_level = newcat_get_ext_level,
    .set_ext_parm = ftx1_set_ext_parm,
    .get_ext_parm = ftx1_get_ext_parm,
    .set_conf = newcat_set_conf,
    .get_conf2 = newcat_get_conf2,
    /*
     * RIT/XIT: Uses RC/TC commands per Jeremy Miller (KO4SSD) PR #1826
     * The standard RT/XT commands return '?' on FTX-1
     */
    .set_rit = ftx1_set_rit,
    .get_rit = ftx1_get_rit,
    .set_xit = ftx1_set_xit,
    .get_xit = ftx1_get_xit,
    .set_rptr_shift = ftx1_set_rptr_shift,
    .get_rptr_shift = ftx1_get_rptr_shift,
    .set_rptr_offs = ftx1_set_rptr_offs,
    .get_rptr_offs = ftx1_get_rptr_offs,
    .set_split_vfo = ftx1_set_split_vfo,
    .get_split_vfo = ftx1_get_split_vfo,
    .set_split_freq = ftx1_set_split_freq,
    .get_split_freq = ftx1_get_split_freq,
    .set_split_mode = ftx1_set_split_mode,
    .get_split_mode = ftx1_get_split_mode,
    .get_info = newcat_get_info,
    .power2mW = newcat_power2mW,
    .mW2power = newcat_mW2power,
    .morse_qsize = 50,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};