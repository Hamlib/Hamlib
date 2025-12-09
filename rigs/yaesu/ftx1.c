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
 */

#include <stdlib.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "sprintflst.h"
#include "yaesu.h"
#include "newcat.h"

// Define FTX-1 rig ID (firmware returns ID0763;, not ID0840; as documented in manual)
#define NC_RIGID_FTX1 0x763

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
    .rig_open = newcat_open,
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