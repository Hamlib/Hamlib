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
 * FIRMWARE NOTES (v1.08+):
 * - RT (RIT on/off) and XT (XIT on/off) commands return '?' - not implemented
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

/*
 * FTX-1 specific private data structure
 * Tracks SPA-1 amplifier detection for command guardrails
 */
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