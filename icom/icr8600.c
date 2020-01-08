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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "misc.h"
#include "idx_builtin.h"
#include "token.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

#define ICR8600_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_RTTY|\
    RIG_MODE_FM|RIG_MODE_WFM|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_SAM|RIG_MODE_SAL|\
    RIG_MODE_SAH|RIG_MODE_P25|RIG_MODE_DSTAR|RIG_MODE_DPMR|RIG_MODE_NXDNVN|\
    RIG_MODE_NXDN_N|RIG_MODE_DCR)

#define ICR8600_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_ANF|RIG_FUNC_MN|RIG_FUNC_AFC|\
    RIG_FUNC_NR|RIG_FUNC_AIP|RIG_FUNC_LOCK|RIG_FUNC_VSC|RIG_FUNC_RESUME|RIG_FUNC_TSQL|\
    RIG_FUNC_CSQL|RIG_FUNC_DSQL)

#define ICR8600_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|\
    RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_PREAMP|\
    RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

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

struct cmdparams icr8600_rigparms[] = {
    { {.s=RIG_PARM_BEEP}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x38}, CMD_DAT_BOL, 1 },
    { {.s=RIG_PARM_BACKLIGHT}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x15}, CMD_DAT_LVL, 2 },
    { {.s=RIG_PARM_KEYLIGHT}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x16}, CMD_DAT_LVL, 2 },
    { {.s=RIG_PARM_TIME}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x32}, CMD_DAT_TIM, 2 },
    { {.s=RIG_PARM_NONE} }
};

int icr8600_tokens[] = { TOK_DSTAR_DSQL, TOK_DSTAR_CALL_SIGN, TOK_DSTAR_MESSAGE, TOK_DSTAR_STATUS,
    TOK_DSTAR_GPS_DATA, TOK_DSTAR_GPS_MESS, TOK_DSTAR_CODE, TOK_DSTAR_TX_DATA,
    TOK_BACKEND_NONE };

struct confparams icr8600_ext[] = {
    { 0 }
};

struct cmdparams icr8600_extcmds[] = {
    { {.t=TOK_LINK} }
};

/*
 * channel caps.
 */
#define ICR8600_MEM_CAP {   \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .ant = 1,   \
    .levels = RIG_LEVEL_ATT|RIG_LEVEL_PREAMP,   \
    .channel_desc = 1,  \
    .flags = 1, \
}

static const struct icom_priv_caps icr8600_priv_caps =
{
    0x96,                       /* default address */
    0,                          /* 731 mode */
    0,                          /* no XCHG */
    r8600_ts_sc_list,           /* list of tuning steps */
    .offs_len = 4,              /* Repeater offset is 4 bytes */
    .serial_USB_echo_check = 1, /* USB CI-V may not echo */
    .rigparms = icr8600_rigparms,
    .extcmds = icr8600_extcmds
};

const struct rig_caps icr8600_caps =
{
    .rig_model = RIG_MODEL_ICR8600,
    .model_name = "IC-R8600",
    .mfg_name = "Icom",
    .version = "0.8",
    .copyright = "LGPL",
    .status = RIG_STATUS_ALPHA,
    .rig_type = RIG_TYPE_RECEIVER,
    .ptt_type = RIG_PTT_NONE,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 300,
    .serial_rate_max = 19200, // USB can do up to 115000
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
    .level_gran = { [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } } },
    .parm_gran = { [PARM_TIME] = { .min = { .i = 0 }, .max = { .i = 86399} } },
    .ext_tokens = icr8600_tokens,
    .extlevels = icr8600_ext,
    .extparms = icom_ext_parms,
    .ctcss_list = common_ctcss_list,
    .dcs_list = common_dcs_list,
    .preamp = { 20, RIG_DBLST_END, },   /* 20 on HF, 14 on VHF, UHF, same setting */
    .attenuator = { 10, 20, 30, RIG_DBLST_END, },
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
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

    .cfgparams = icom_cfg_params,

    .set_conf = icom_set_conf,
    .get_conf = icom_get_conf,

    .priv = (void *)& icr8600_priv_caps,
    .rig_init = icom_init,
    .rig_cleanup = icom_cleanup,
    .rig_open = icom_rig_open,
    .rig_close = NULL,

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

};
