/*
 *  Hamlib CI-V backend - description of IC-R8600
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *  Copyright (c) 2018 by Ekki Plicht
 *  Copyright (c) 2019 by Malcolm Herring
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <hamlib/config.h>

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "token.h"

#include "icom.h"
#include "icom_defs.h"

#define ICR8600_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_RTTY|\
    RIG_MODE_FM|RIG_MODE_WFM|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_SAM|RIG_MODE_SAL|\
    RIG_MODE_SAH|RIG_MODE_P25|RIG_MODE_DSTAR|RIG_MODE_DPMR|RIG_MODE_NXDNVN|\
    RIG_MODE_NXDN_N|RIG_MODE_DCR)

#define ICR8600_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_ANF|RIG_FUNC_MN|RIG_FUNC_AFC|\
    RIG_FUNC_NR|RIG_FUNC_AIP|RIG_FUNC_LOCK|RIG_FUNC_VSC|RIG_FUNC_RESUME|RIG_FUNC_TSQL|\
    RIG_FUNC_CSQL|RIG_FUNC_DSQL|RIG_FUNC_TRANSCEIVE|RIG_FUNC_SPECTRUM|RIG_FUNC_SPECTRUM_HOLD|RIG_FUNC_OVF_STATUS)

#define ICR8600_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|\
    RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_PREAMP|\
    RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_SPECTRUM_MODE|RIG_LEVEL_SPECTRUM_SPAN|\
    RIG_LEVEL_SPECTRUM_SPEED|RIG_LEVEL_SPECTRUM_REF|RIG_LEVEL_SPECTRUM_AVG|\
    RIG_LEVEL_SPECTRUM_EDGE_LOW|RIG_LEVEL_SPECTRUM_EDGE_HIGH|RIG_LEVEL_USB_AF)

#define ICR8600_PARM_ALL (RIG_PARM_BACKLIGHT|RIG_PARM_BEEP|RIG_PARM_TIME|RIG_PARM_KEYLIGHT)

#define ICR8600_VFO_ALL (RIG_VFO_VFO|RIG_VFO_MEM)

#define ICR8600_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)

#define ICR8600_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_SLCT|\
    RIG_SCAN_PRIO|RIG_SCAN_PRIO|RIG_SCAN_DELTA|RIG_SCAN_STOP)

#define ICR8600_ANTS_HF (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)
#define ICR8600_ANTS_VHF (RIG_ANT_1)

#define ICR8600_STR_CAL { 2, {\
    {   0, -60 }, \
    { 255, 60 }, \
} }

struct cmdparams icr8600_extcmds[] =
{
    { {.s = RIG_PARM_BEEP}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x38}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x15}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_KEYLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x16}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_TIME}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x32}, CMD_DAT_TIM, 2 },
    { {.s = RIG_FUNC_TRANSCEIVE}, CMD_PARAM_TYPE_FUNC, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x92}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_SPECTRUM_AVG}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x40}, CMD_DAT_INT, 1 },
    { {.s = RIG_LEVEL_USB_AF}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x81}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_NONE} }
};

int icr8600_tokens[] =
{
    TOK_DSTAR_DSQL, TOK_DSTAR_CALL_SIGN, TOK_DSTAR_MESSAGE, TOK_DSTAR_STATUS,
    TOK_DSTAR_GPS_DATA, TOK_DSTAR_GPS_MESS, TOK_DSTAR_CODE, TOK_DSTAR_TX_DATA,
    TOK_SCOPE_CFQ, TOK_SCOPE_VBW,
    TOK_BACKEND_NONE
};

#define ICR8600_MEM_CAP {   \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .ant = 1,   \
    .levels = RIG_LEVEL_ATT|RIG_LEVEL_PREAMP,   \
    .channel_desc = 1,  \
    .flags = 1, \
}

static struct icom_priv_caps icr8600_priv_caps =
{
    0x96,                           /* default address */
    0,                              /* 731 mode */
    0,                              /* no XCHG */
    r8600_ts_sc_list,               /* list of tuning steps */
    .antack_len = 2,
    .ant_count = 3,
    .offs_len = 4,                  /* Repeater offset is 4 bytes */
    .serial_USB_echo_check = 1,     /* USB CI-V may not echo */
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -1 },
    },
    .spectrum_scope_caps = {
        .spectrum_line_length = 475,
        .single_frame_data_length = 50,
        .data_level_min = 0,
        .data_level_max = 160,
        .signal_strength_min = -100, // TODO: signal strength to be confirmed
        .signal_strength_max = 0,
    },
    .spectrum_edge_frequency_ranges = {
        {
            .range_id = 1,
            .low_freq = 0,
            .high_freq = 3000000000,
        },
    },
    .extcmds = icr8600_extcmds      /* Custom ext_cmd parameters */
};

const struct rig_caps icr8600_caps =
{
    RIG_MODEL(RIG_MODEL_ICR8600),
    .model_name = "IC-R8600",
    .mfg_name = "Icom",
    .version =  BACKEND_VER ".3",
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,
    .rig_type = RIG_TYPE_RECEIVER,
    .ptt_type = RIG_PTT_NONE,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 300,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 1,
    .timeout = 1000,
    .retry = 3,
    .has_get_func = ICR8600_FUNC_ALL,
    .has_set_func = ICR8600_FUNC_ALL,
    .has_get_level = ICR8600_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(ICR8600_LEVEL_ALL),
    .has_get_parm = ICR8600_PARM_ALL,
    .has_set_parm = RIG_PARM_SET(ICR8600_PARM_ALL),
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 }, .step = {.i = 0}},
        [LVL_SPECTRUM_SPEED] = {.min = {.i = 0}, .max = {.i = 2}, .step = {.i = 1}},
        [LVL_SPECTRUM_REF] = {.min = {.f = -20.0f}, .max = {.f = 20.0f}, .step = {.f = 0.5f}},
        [LVL_SPECTRUM_AVG] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
        [LVL_USB_AF] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.f = 1.0f / 255.0f }},
    },
    .parm_gran = { [PARM_TIME] = { .min = { .i = 0 }, .max = { .i = 86399} } },
    .ext_tokens = icr8600_tokens,
    .extlevels = icom_ext_levels,
    .extfuncs = icom_ext_funcs,
    .extparms = icom_ext_parms,
    .ctcss_list = common_ctcss_list,
    .dcs_list = common_dcs_list,
    .preamp = { 20, RIG_DBLST_END, },   /* 20 on HF, 14 on VHF, UHF, same setting */
    .attenuator = { 10, 20, 30, RIG_DBLST_END, },
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo = 0,
    .vfo_ops = ICR8600_VFO_OPS,
    .scan_ops = ICR8600_SCAN_OPS,
    .transceive = RIG_TRN_RIG,
    .bank_qty = 100,
    .chan_desc_sz = 16,

    .chan_list = {
        { 0, 99, RIG_MTYPE_MEM, ICR8600_MEM_CAP },
        {   0, 99, RIG_MTYPE_EDGE, ICR8600_MEM_CAP },
        /* to be extended */
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        { kHz(10), MHz(3000), ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_VHF },
        { kHz(10), MHz(30),   ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_HF },
        RIG_FRNG_END,
    },

    .tx_range_list1 = { RIG_FRNG_END, },

    .rx_range_list2 = {
        { kHz(10), MHz(3000), ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_VHF },
        { kHz(10), MHz(30),   ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_HF },
        RIG_FRNG_END,
    },

    .tx_range_list2 = { RIG_FRNG_END, },

    .tuning_steps = {
        {ICR8600_MODES, Hz(100)},
        {ICR8600_MODES, kHz(1)},
        {ICR8600_MODES, kHz(2.5)},
        {ICR8600_MODES, kHz(3.125)},
        {ICR8600_MODES, kHz(5)},
        {ICR8600_MODES, kHz(6.25)},
        {ICR8600_MODES, kHz(8.33)},
        {ICR8600_MODES, kHz(9)},
        {ICR8600_MODES, kHz(10)},
        {ICR8600_MODES, kHz(12.5)},
        {ICR8600_MODES, kHz(20)},
        {ICR8600_MODES, kHz(25)},
        {ICR8600_MODES, kHz(100)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(1.9)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(15)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(6)},
        RIG_FLT_END,
    },

    .str_cal = ICR8600_STR_CAL,

    .spectrum_scopes = {
        {
            .id = 0,
            .name = "Main",
        },
        {
            .id = -1,
            .name = NULL,
        },
    },
    .spectrum_modes = {
        RIG_SPECTRUM_MODE_CENTER,
        RIG_SPECTRUM_MODE_FIXED,
        RIG_SPECTRUM_MODE_NONE,
    },
    .spectrum_spans = {
        5000,
        10000,
        20000,
        50000,
        100000,
        200000,
        500000,
        1000000,
        2000000,
        5000000,
        0,
    },
    .spectrum_avg_modes = {
        {
            .id = 0,
            .name = "OFF",
        },
        {
            .id = 1,
            .name = "2",
        },
        {
            .id = 2,
            .name = "3",
        },
        {
            .id = 3,
            .name = "4",
        },
    },

    .async_data_supported = 1,
    .read_frame_direct = icom_read_frame_direct,
    .is_async_frame = icom_is_async_frame,
    .process_async_frame = icom_process_async_frame,

    .cfgparams = icom_cfg_params,

    .set_conf = icom_set_conf,
    .get_conf = icom_get_conf,
    .set_powerstat = icom_set_powerstat,
    .get_powerstat = icom_get_powerstat,

    .priv = (void *)& icr8600_priv_caps,
    .rig_init = icom_init,
    .rig_cleanup = icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq = icom_set_freq,
    .get_freq = icom_get_freq,
    .set_mode = icom_set_mode,
    .get_mode = icom_get_mode,
    .set_vfo = icom_set_vfo,
    .set_bank = icom_set_bank,
    .get_rptr_offs = icom_get_rptr_offs,
    .set_rptr_offs = icom_set_rptr_offs,
    .get_rptr_shift = icom_get_rptr_shift,
    .set_rptr_shift = icom_set_rptr_shift,
    .set_ant = icom_set_ant,
    .get_ant = icom_get_ant,

    .decode_event = icom_decode_event,
    .set_func = icom_set_func,
    .get_func = icom_get_func,
    .set_level = icom_set_level,
    .get_level = icom_get_level,
    .set_parm = icom_set_parm,
    .get_parm = icom_get_parm,
    .set_ext_parm = icom_set_ext_parm,
    .get_ext_parm = icom_get_ext_parm,
    .set_ext_func = icom_set_ext_func,
    .get_ext_func = icom_get_ext_func,
    .set_ext_level =  icom_set_ext_level,
    .get_ext_level =  icom_get_ext_level,
    .get_dcd = icom_get_dcd,
    .set_mem = icom_set_mem,
    .vfo_op = icom_vfo_op,
    .scan = icom_scan,
    .set_ts = icom_set_ts,
    .get_ts = icom_get_ts,
    .set_ctcss_sql = icom_set_ctcss_sql,
    .get_ctcss_sql = icom_get_ctcss_sql,
    .set_dcs_sql = icom_set_dcs_sql,
    .get_dcs_sql = icom_get_dcs_sql,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
