/*
 *  Hamlib CI-V backend - description of IC-7300 and variations
 *  Adapted by J.Watson from IC-7000 code (c) 2004 by Stephane Fillod
 *  Adapted from IC-7200 (c) 2016 by Michael Black W9MDB
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
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "token.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "bandplan.h"
#include "tones.h"

#define IC7300_ALL_RX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)
#define IC7300_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)
#define IC7300_NOT_TS_MODES (IC7300_ALL_RX_MODES &~IC7300_1HZ_TS_MODES)

#define IC7300_OTHER_TX_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define IC7300_AM_TX_MODES (RIG_MODE_AM|RIG_MODE_PKTAM)

#define IC7300_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_SCOPE|RIG_FUNC_TUNER)

#define IC7300_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB)

#define IC7300_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define IC7300_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_TIME|RIG_PARM_BEEP)

#define IC7300_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC7300_SCAN_OPS (RIG_SCAN_STOP|RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT)

#define IC7300_ANTS (RIG_ANT_1) /* ant-1 is Hf-6m */

struct cmdparams ic7300_extcmds[] =
{
    { {.s = RIG_PARM_BEEP}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x23}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x81}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_TIME}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x95}, CMD_DAT_TIM, 2 },
    { {.s = RIG_LEVEL_VOXDELAY}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x91}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_NONE} }
};

/*
 * IC-7300 S-meter levels measured from live signals on multiple bands. Provides a good approximation.
 */
#define IC7300_STR_CAL { 7, \
    { \
        {   0, -54 }, \
        {  10, -48 }, \
        {  30, -36 }, \
        {  60, -24 }, \
        {  90, -12 }, \
        { 120,  0 }, \
        { 241,  64 } \
    } }

#define IC7300_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC7300_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

#define IC7300_RFPOWER_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 143, 0.5f }, \
         { 213, 1.0f } \
    } }

#define IC7300_COMP_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 130, 15.0f }, \
         { 241, 30.0f } \
    } }

#define IC7300_VD_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 13, 10.0f }, \
         { 241, 16.0f } \
    } }

#define IC7300_ID_METER_CAL { 4, \
    { \
         { 0, 0.0f }, \
         { 97, 10.0f }, \
         { 146, 15.0f }, \
         { 241, 25.0f } \
    } }


/*
 * IC9700 items that differ from IC7300
 */
#define IC9700_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM|RIG_VFO_MAIN_A|RIG_VFO_MAIN_B|RIG_VFO_SUB_A|RIG_VFO_SUB_B)
#define IC9700_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_TIME|RIG_PARM_BEEP|RIG_PARM_SCREENSAVER)
#define IC9700_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_SCOPE|RIG_FUNC_SATMODE|RIG_FUNC_AFC)
#define IC9700_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB)
#define IC9700_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC9700_SCAN_OPS (RIG_SCAN_STOP|RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT)
#define IC9700_ALL_TX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_DSTAR|RIG_MODE_DD|RIG_MODE_DSTAR)
#define IC9700_ALL_RX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_DSTAR|RIG_MODE_DD|RIG_MODE_DSTAR)

struct cmdparams ic9700_extcmds[] =
{
    { {.s = RIG_PARM_BEEP}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x29}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x52}, CMD_DAT_LVL, 2 },
    { {.s = RIG_LEVEL_VOXDELAY}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x03, 0x30}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_SCREENSAVER}, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x67}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_NONE} }
};

#define IC9700_STR_CAL { 7, \
    { \
        {   0, -54 }, \
        {  10, -48 }, \
        {  30, -36 }, \
        {  60, -24 }, \
        {  90, -12 }, \
        { 120,  0 }, \
        { 241,  64 } \
    } }

#define IC9700_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC9700_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

#define IC9700_RFPOWER_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 143, 0.5f }, \
         { 213, 1.0f } \
    } }

#define IC9700_COMP_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 130, 15.0f }, \
         { 210, 25.5f } \
    } }

#define IC9700_VD_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 13, 10.0f }, \
         { 241, 16.0f } \
    } }

#define IC9700_ID_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 121, 10.0f }, \
         { 241, 20.0f } \
    } }

/*
 * IC-7300 rig capabilities.
 */
static const struct icom_priv_caps IC7300_priv_caps =
{
    0x94,   /* default address */
    0,      /* 731 mode */
    1,      /* no XCHG to avoid display flickering */
    ic7300_ts_sc_list,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = -1, .icom_level = 0 },
    },
    .extcmds = ic7300_extcmds,   /* Custom op parameters */
};

static const struct icom_priv_caps IC9700_priv_caps =
{
    0xA2,   /* default address */
    0,      /* 731 mode */
    1,      /* no XCHG to avoid display flickering */
    ic7300_ts_sc_list,
    .serial_USB_echo_check = 1,  /* USB CI-V may not echo */
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = -1, .icom_level = 0 },
    },
    .extcmds = ic9700_extcmds,   /* Custom op parameters */
};

const struct rig_caps ic7300_caps =
{
    RIG_MODEL(RIG_MODEL_IC7300),
    .model_name = "IC-7300",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC7300_FUNCS,
    .has_set_func =  IC7300_FUNCS,
    .has_get_level =  IC7300_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC7300_LEVELS),
    .has_get_parm =  IC7300_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC7300_PARMS),
    .level_gran = {
        [LVL_RAWSTR] = {.min = {.i = 0}, .max = {.i = 255}},
        [LVL_VOXDELAY] = {.min = {.i = 0}, .max = {.i = 20}, .step = {.i = 1}},
        [LVL_KEYSPD] = {.min = {.i = 6}, .max = {.i = 48}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 300}, .max = {.i = 900}, .step = {.i = 1}},
    },
    .parm_gran =  {},
    .extlevels = NULL,
    .ctcss_list =  full_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 1, 2, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC7300_VFO_OPS,
    .scan_ops =  IC7300_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   1,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(74.8), IC7300_ALL_RX_MODES, -1, -1, IC7300_VFOS}, RIG_FRNG_END, },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_60m(1, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(1, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_4m(1, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_HF(1, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_60m(1, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_4m(1, IC7300_AM_TX_MODES, W(1), W(12.5), IC7300_VFOS, RIG_ANT_1), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(74.8), IC7300_ALL_RX_MODES, -1, -1, IC7300_VFOS}, RIG_FRNG_END, },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_60m(2, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(2, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_HF(2, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_60m(2, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(2, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC7300_ALL_RX_MODES, Hz(1)},
        {IC7300_ALL_RX_MODES, kHz(1)},
        {IC7300_ALL_RX_MODES, kHz(5)},
        {IC7300_ALL_RX_MODES, kHz(9)},
        {IC7300_ALL_RX_MODES, kHz(10)},
        {IC7300_ALL_RX_MODES, kHz(12.5)},
        {IC7300_ALL_RX_MODES, kHz(20)},
        {IC7300_ALL_RX_MODES, kHz(25)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! But duplication may speed up search.  Put the most commonly used modes first!  Remember these are defaults, with dsp rigs you can change them to anything you want except FM and WFM which are fixed */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(3)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(250)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1.2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(3)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(9)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(10)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(7)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(15)},
        RIG_FLT_END,
    },

    .str_cal = IC7300_STR_CAL,
    .swr_cal = IC7300_SWR_CAL,
    .alc_cal = IC7300_ALC_CAL,
    .rfpower_meter_cal = IC7300_RFPOWER_METER_CAL,
    .comp_meter_cal = IC7300_COMP_METER_CAL,
    .vd_meter_cal = IC7300_VD_METER_CAL,
    .id_meter_cal = IC7300_ID_METER_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& IC7300_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
//.get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,
    .set_ant =  NULL,
    .get_ant =  NULL,

    .set_rit =  icom_set_rit_new,
    .get_rit =  icom_get_rit_new,
    .get_xit =  icom_get_rit_new,
    .set_xit =  icom_set_xit_new,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
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
    .set_rptr_shift =  icom_set_rptr_shift,
    .get_rptr_shift =  icom_get_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
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
    .set_powerstat = icom_set_powerstat,
    .get_powerstat = icom_get_powerstat,
    .power2mW = icom_power2mW,
    .mW2power = icom_mW2power,
    .send_morse = icom_send_morse,
    .send_voice_mem = icom_send_voice_mem
};

const struct rig_caps ic9700_caps =
{
    RIG_MODEL(RIG_MODEL_IC9700),
    .model_name = "IC-9700",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  38400,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC9700_FUNCS,
    .has_set_func =  IC9700_FUNCS,
    .has_get_level =  IC9700_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC9700_LEVELS),
    .has_get_parm =  IC9700_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC9700_PARMS),
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 6}, .max = {.i = 48}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 300}, .max = {.i = 900}, .step = {.i = 1}},
    },
    .parm_gran =  {},
    .extlevels = NULL,
    .ctcss_list =  full_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 1, 2, 3, RIG_DBLST_END, },
    .attenuator =   { 10, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .vfo_ops =  IC9700_VFO_OPS,
    .scan_ops =  IC9700_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   3,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        RIG_CHAN_END,
    },

    // Hopefully any future changes in bandplans can be fixed with firmware updates
    // So we use the global REGION2 macros in here
    .rx_range_list1 =   { // USA Version -- United States
        {MHz(144), MHz(148), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(430), MHz(450), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(1240), MHz(1300), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "USA"},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {MHz(144), MHz(148), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(430), MHz(450), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(1240), MHz(1300), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(144), MHz(148), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(430), MHz(450), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(1240), MHz(1300), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { // EUR Version -- Europe
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(430), MHz(440), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(1240), MHz(1300), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(430), MHz(440), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(1240), MHz(1300), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(430), MHz(440), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(1240), MHz(1300), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        RIG_FRNG_END,
    },

    .rx_range_list3 =   { // ITR Version -- Italy
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(430), MHz(434), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(435), MHz(438), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1240), MHz(1245), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1270), MHz(1298), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        RIG_FRNG_END,
    },
    .tx_range_list3 =  {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(430), MHz(434), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(435), MHz(438), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1240), MHz(1245), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1270), MHz(1298), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(430), MHz(434), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(435), MHz(438), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1240), MHz(1245), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1270), MHz(1298), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        RIG_FRNG_END,
    },

    .rx_range_list4 =   { // TPE Version -- Taiwan ??
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(430), MHz(432), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(1260), MHz(1265), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        RIG_FRNG_END,
    },
    .tx_range_list4 =  {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(430), MHz(432), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(1260), MHz(1265), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(430), MHz(432), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(1260), MHz(1265), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        RIG_FRNG_END,
    },

    .rx_range_list5 =   { // KOR Version -- Republic of Korea
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(430), MHz(440), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(1260), MHz(1300), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        RIG_FRNG_END,
    },
    .tx_range_list5 =  {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(430), MHz(440), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(1260), MHz(1300), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(430), MHz(440), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(1260), MHz(1300), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC9700_ALL_RX_MODES, Hz(1)},
        {IC9700_ALL_RX_MODES, Hz(10)},
        {IC9700_ALL_RX_MODES, Hz(100)},
        {IC9700_ALL_RX_MODES, Hz(500)},
        {IC9700_ALL_RX_MODES, kHz(1)},
        {IC9700_ALL_RX_MODES, kHz(5)},
        {IC9700_ALL_RX_MODES, kHz(6.25)},
        {IC9700_ALL_RX_MODES, kHz(10)},
        {IC9700_ALL_RX_MODES, kHz(12.5)},
        {IC9700_ALL_RX_MODES, kHz(20)},
        {IC9700_ALL_RX_MODES, kHz(25)},
        {IC9700_ALL_RX_MODES, kHz(50)},
        {IC9700_ALL_RX_MODES, kHz(100)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! But duplication may speed up search.  Put the most commonly used modes first!  Remember these are defaults, with dsp rigs you can change them to anything you want except FM and WFM which are fixed */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(3)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(250)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1.2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(3)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(9)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(15)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(7)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(10)},
        RIG_FLT_END,
    },

    .str_cal = IC9700_STR_CAL,
    .swr_cal = IC9700_SWR_CAL,
    .alc_cal = IC9700_ALC_CAL,
    .rfpower_meter_cal = IC9700_RFPOWER_METER_CAL,
    .comp_meter_cal = IC9700_COMP_METER_CAL,
    .vd_meter_cal = IC9700_VD_METER_CAL,
    .id_meter_cal = IC9700_ID_METER_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& IC9700_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
//.get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,
    .set_ant =  NULL,
    .get_ant =  NULL,

    .set_rit =  icom_set_rit_new,
    .get_rit =  icom_get_rit_new,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
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
    .set_rptr_shift =  icom_set_rptr_shift,
    .get_rptr_shift =  icom_get_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
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
    .set_powerstat = icom_set_powerstat,
    .power2mW = icom_power2mW,
    .mW2power = icom_mW2power,
    .send_morse = icom_send_morse,
    .send_voice_mem = icom_send_voice_mem
};
