/*
 *  Hamlib CI-V backend - description of IC-785x and variations
 *  Derived from ic7800.c by W9MDB -- needs testing
 *  Copyright (c) 2009-2010 by Stephane Fillod
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

#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "token.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "bandplan.h"
#include "ic7300.h"

#define IC785x_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_PSK|RIG_MODE_PSKR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTAM|RIG_MODE_PKTFM)
#define IC785x_1HZ_TS_MODES IC785x_ALL_RX_MODES
#define IC785x_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_PSK|RIG_MODE_PSKR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM)
#define IC785x_AM_TX_MODES (RIG_MODE_AM|RIG_MODE_PKTAM)

#define IC785x_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_VSC|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_TUNER|RIG_FUNC_APF|RIG_FUNC_DUAL_WATCH|RIG_FUNC_TRANSCEIVE|RIG_FUNC_SPECTRUM|RIG_FUNC_SPECTRUM_HOLD)

#define IC785x_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_BALANCE|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_APF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_SPECTRUM_MODE|RIG_LEVEL_SPECTRUM_SPAN|RIG_LEVEL_SPECTRUM_SPEED|RIG_LEVEL_SPECTRUM_REF|RIG_LEVEL_SPECTRUM_AVG|RIG_LEVEL_SPECTRUM_EDGE_LOW|RIG_LEVEL_SPECTRUM_EDGE_HIGH|RIG_LEVEL_SPECTRUM_ATT)

#define IC785x_VFOS (RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM)
#define IC785x_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_TIME|RIG_PARM_BEEP)

#define IC785x_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC785x_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_DELTA|RIG_SCAN_PRIO)

#define IC785x_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3|RIG_ANT_4)

// IC-785x S-meter calibration data based on manual
#define IC785x_STR_CAL { 3, \
    { \
        {   0, -54 }, /* S0 */ \
        { 120,   0 }, /* S9 */ \
        { 241,  60 }  /* S9+60 */ \
    } }

#define IC785x_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC785x_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

#define IC785x_RFPOWER_METER_CAL { 13, \
    { \
         { 0, 0.0f }, \
         { 21, 5.0f }, \
         { 43, 10.0f }, \
         { 65, 15.0f }, \
         { 83, 20.0f }, \
         { 95, 25.0f }, \
         { 105, 30.0f }, \
         { 114, 35.0f }, \
         { 124, 40.0f }, \
         { 143, 50.0f }, \
         { 183, 75.0f }, \
         { 213, 100.0f }, \
         { 255, 120.0f } \
    } }


#define IC785x_COMP_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 130, 15.0f }, \
         { 241, 30.0f } \
    } }

#define IC785x_VD_METER_CAL { 4, \
    { \
         { 0, 0.0f }, \
         { 151, 44.0f }, \
         { 180, 48.0f }, \
         { 211, 52.0f } \
    } }

#define IC785x_ID_METER_CAL { 4, \
    { \
         { 0, 0.0f }, \
         { 97, 10.0f }, \
         { 146, 15.0f }, \
         { 241, 25.0f } \
    } }

extern int ic7800_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
extern int ic7800_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

int ic785x_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ic785x_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

struct cmdparams ic785x_extcmds[] =
{
    { {.s = RIG_PARM_BEEP}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x04}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x76}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_TIME}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x96}, CMD_DAT_TIM, 2 },
    { {.s = RIG_LEVEL_VOXDELAY}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x03, 0x09}, CMD_DAT_INT, 1 },
    { {.s = RIG_FUNC_TRANSCEIVE}, CMD_PARAM_TYPE_FUNC, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x55}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_SPECTRUM_AVG}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x87}, CMD_DAT_INT, 1 },
    { {.s = RIG_LEVEL_USB_AF}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x52}, CMD_DAT_LVL, 2 },
    { { 0 } }
};


int ic785x_ext_tokens[] =
{
    TOK_DRIVE_GAIN, TOK_DIGI_SEL_FUNC, TOK_DIGI_SEL_LEVEL,
    TOK_SCOPE_MSS, TOK_SCOPE_SDS, TOK_SCOPE_STX, TOK_SCOPE_CFQ, TOK_SCOPE_EDG, TOK_SCOPE_VBW, TOK_SCOPE_MKP,
    TOK_BACKEND_NONE
};

/*
 * IC-785x rig capabilities.
 */
static struct icom_priv_caps ic785x_priv_caps =
{
    0x8e, /* default address */
    0,    /* 731 mode */
    0,    /* no XCHG */
    ic756pro_ts_sc_list,
    .antack_len = 3,
    .ant_count = 4,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_OFF, .icom_level = 0 },
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -1 },
    },
    .spectrum_scope_caps = {
        .spectrum_line_length = 689,
        .single_frame_data_length = 50,
        .data_level_min = 0,
        .data_level_max = 200,
        .signal_strength_min = -100,
        .signal_strength_max = 0,
    },
    .spectrum_edge_frequency_ranges = {
        {
            .range_id = 1,
            .low_freq = 30000,
            .high_freq = 1600000,
        },
        {
            .range_id = 2,
            .low_freq = 1600000,
            .high_freq = 2000000,
        },
        {
            .range_id = 3,
            .low_freq = 2000000,
            .high_freq = 6000000,
        },
        {
            .range_id = 4,
            .low_freq = 6000000,
            .high_freq = 8000000,
        },
        {
            .range_id = 5,
            .low_freq = 8000000,
            .high_freq = 11000000,
        },
        {
            .range_id = 6,
            .low_freq = 11000000,
            .high_freq = 15000000,
        },
        {
            .range_id = 7,
            .low_freq = 15000000,
            .high_freq = 20000000,
        },
        {
            .range_id = 8,
            .low_freq = 20000000,
            .high_freq = 22000000,
        },
        {
            .range_id = 9,
            .low_freq = 22000000,
            .high_freq = 26000000,
        },
        {
            .range_id = 10,
            .low_freq = 26000000,
            .high_freq = 30000000,
        },
        {
            .range_id = 11,
            .low_freq = 30000000,
            .high_freq = 45000000,
        },
        {
            .range_id = 12,
            .low_freq = 45000000,
            .high_freq = 60000000,
        },
        {
            .range_id = 0,
            .low_freq = 0,
            .high_freq = 0,
        },
    },
    .extcmds = ic785x_extcmds,
};

const struct rig_caps ic785x_caps =
{
    RIG_MODEL(RIG_MODEL_IC785x),
    .model_name = "IC-7850/7851",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".4",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC785x_FUNCS,
    .has_set_func =  IC785x_FUNCS,
    .has_get_level =  IC785x_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC785x_LEVELS),
    .has_get_parm =  IC785x_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC785x_PARMS),    /* FIXME: parms */
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = { .min = { .i = 6 }, .max = { .i = 48 }, .step = { .i = 1 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 1 } },
        [LVL_SPECTRUM_SPEED] = {.min = {.i = 0}, .max = {.i = 2}, .step = {.i = 1}},
        [LVL_SPECTRUM_REF] = {.min = {.f = -20.0f}, .max = {.f = 20.0f}, .step = {.f = 0.5f}},
        [LVL_SPECTRUM_AVG] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
        [LVL_USB_AF] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.f = 1.0f / 255.0f }},
    },
    .parm_gran =  {},
    .ext_tokens = ic785x_ext_tokens,
    .extlevels = icom_ext_levels,
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 12, 20, RIG_DBLST_END, },
    .attenuator =   { 3, 6, 9, 12, 15, 18, 21, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0),
    .agc_level_count = 4,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_SPECTRUM,
    .vfo_ops =  IC785x_VFO_OPS,
    .scan_ops =  IC785x_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(60), IC785x_ALL_RX_MODES, -1, -1, IC785x_VFOS, IC785x_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC785x_OTHER_TX_MODES, W(5), W(200), IC785x_VFOS, IC785x_ANTS),
        FRQ_RNG_6m(1, IC785x_OTHER_TX_MODES, W(5), W(200), IC785x_VFOS, IC785x_ANTS),
        FRQ_RNG_HF(1, IC785x_AM_TX_MODES, W(5), W(50), IC785x_VFOS, IC785x_ANTS), /* AM class */
        FRQ_RNG_6m(1, IC785x_AM_TX_MODES, W(5), W(50), IC785x_VFOS, IC785x_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(60), IC785x_ALL_RX_MODES, -1, -1, IC785x_VFOS, IC785x_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC785x_OTHER_TX_MODES, W(5), W(200), IC785x_VFOS, IC785x_ANTS),
        FRQ_RNG_6m(2, IC785x_OTHER_TX_MODES, W(5), W(200), IC785x_VFOS, IC785x_ANTS),
        FRQ_RNG_HF(2, IC785x_AM_TX_MODES, W(5), W(50), IC785x_VFOS, IC785x_ANTS), /* AM class */
        FRQ_RNG_6m(2, IC785x_AM_TX_MODES, W(5), W(50), IC785x_VFOS, IC785x_ANTS), /* AM class */
        /* USA only, TBC: end of range and modes */
        {MHz(5.33050), MHz(5.33350), IC785x_OTHER_TX_MODES, W(2), W(100), IC785x_VFOS, IC785x_ANTS}, /* USA only */
        {MHz(5.34650), MHz(5.34950), IC785x_OTHER_TX_MODES, W(2), W(100), IC785x_VFOS, IC785x_ANTS}, /* USA only */
        {MHz(5.36650), MHz(5.36950), IC785x_OTHER_TX_MODES, W(2), W(100), IC785x_VFOS, IC785x_ANTS}, /* USA only */
        {MHz(5.37150), MHz(5.37450), IC785x_OTHER_TX_MODES, W(2), W(100), IC785x_VFOS, IC785x_ANTS}, /* USA only */
        {MHz(5.40350), MHz(5.40650), IC785x_OTHER_TX_MODES, W(2), W(100), IC785x_VFOS, IC785x_ANTS}, /* USA only */
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC785x_1HZ_TS_MODES, 1},
        {IC785x_ALL_RX_MODES, Hz(100)},
        {IC785x_ALL_RX_MODES, kHz(1)},
        {IC785x_ALL_RX_MODES, kHz(5)},
        {IC785x_ALL_RX_MODES, kHz(9)},
        {IC785x_ALL_RX_MODES, kHz(10)},
        {IC785x_ALL_RX_MODES, kHz(12.5)},
        {IC785x_ALL_RX_MODES, kHz(20)},
        {IC785x_ALL_RX_MODES, kHz(25)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(3)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PSK | RIG_MODE_PSKR, Hz(400)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PSK | RIG_MODE_PSKR, Hz(50)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_PSK | RIG_MODE_PSKR, kHz(1.0)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(3)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(9)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(12)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(8)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(15)},
        RIG_FLT_END,
    },
    .str_cal = IC785x_STR_CAL,
    .swr_cal = IC785x_SWR_CAL,
    .alc_cal = IC785x_ALC_CAL,
    .rfpower_meter_cal = IC785x_RFPOWER_METER_CAL,
    .comp_meter_cal = IC785x_COMP_METER_CAL,
    .vd_meter_cal = IC785x_VD_METER_CAL,
    .id_meter_cal = IC785x_ID_METER_CAL,

    .spectrum_scopes = {
        {
            .id = 0,
            .name = "Main",
        },
        {
            .id = 1,
            .name = "Sub",
        },
        {
            .id = -1,
            .name = NULL,
        },
    },
    .spectrum_modes = {
        RIG_SPECTRUM_MODE_CENTER,
        RIG_SPECTRUM_MODE_FIXED,
        RIG_SPECTRUM_MODE_CENTER_SCROLL,
        RIG_SPECTRUM_MODE_FIXED_SCROLL,
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
    .spectrum_attenuator = { 10, 20, 30, RIG_DBLST_END, },

    .async_data_supported = 1,
    .read_frame_direct = icom_read_frame_direct,
    .is_async_frame = icom_is_async_frame,
    .process_async_frame = icom_process_async_frame,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic785x_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
    .set_vfo =  icom_set_vfo,
    .get_vfo =  icom_get_vfo,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,

    .set_rit =  icom_set_rit_new,
    .get_rit =  icom_get_rit_new,
    .get_xit =  icom_get_rit_new,
    .set_xit =  icom_set_xit_new,

    .decode_event =  icom_decode_event,
    .set_level =  ic785x_set_level,
    .get_level =  ic785x_get_level,
    .set_ext_level =  icom_set_ext_level,
    .get_ext_level =  icom_get_ext_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_parm =  icom_set_parm,
    .get_parm =  icom_get_parm,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_ptt =  icom_get_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_ctcss_tone =  icom_set_ctcss_tone,
    .get_ctcss_tone =  icom_get_ctcss_tone,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  icom_get_split_vfo,
    .set_powerstat =  icom_set_powerstat,
    .get_powerstat =  icom_get_powerstat,
    .send_morse = icom_send_morse,
    .stop_morse = icom_stop_morse,
    .wait_morse = rig_wait_morse,
    .set_clock = ic7300_set_clock,
    .get_clock = ic7300_get_clock,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

int ic785x_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    return ic7800_set_level(rig, vfo, level, val);
}

int ic785x_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    return ic7800_get_level(rig, vfo, level, val);
}
