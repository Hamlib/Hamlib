/*
 *  Hamlib CI-V backend - description of IC-7000 and variations
 *  Adapted from IC-7800 code 2006 by Kent Hill
 *  Copyright (c) 2004-2010 by Stephane Fillod
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
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "bandplan.h"

#define IC7000_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_WFM)
#define IC7000_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define IC7000_NOT_TS_MODES (IC7000_ALL_RX_MODES &~IC7000_1HZ_TS_MODES)

#define IC7000_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC7000_AM_TX_MODES (RIG_MODE_AM)

#define IC7000_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_VSC|RIG_FUNC_LOCK|RIG_FUNC_ARO)

#define IC7000_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_COMP_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB)

#define IC7000_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define IC7000_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_APO|RIG_PARM_TIME|RIG_PARM_BEEP)

#define IC7000_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC7000_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT|RIG_SCAN_PRIO)

#define IC7000_ANTS (RIG_ANT_1|RIG_ANT_2) /* ant-1 is Hf-6m, ant-2 is 2m-70cm */

/*
 * Measurement by Mark, WA0TOP
 *
 * s/n 0503103.
 * Preamp off, ATT off, mode AM, f=10 MHz
 */
#define IC7000_STR_CAL { 14, \
    { \
        {   0, -54 }, /* first one is made up */ \
        {   5, -29 }, \
        {  15, -27 }, \
        {  43, -22 }, \
        {  68, -17 }, \
        {  92, -12 }, \
        { 120,  -6 }, \
        { 141,   2 }, \
        { 162,  13 }, \
        { 182,  25 }, \
        { 202,  38 }, \
        { 222,  47 }, \
        { 241,  57 }, \
        { 255,  63 } \
    } }

#define IC7000_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC7000_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

#define IC7000_RFPOWER_METER_CAL { 13, \
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


#define IC7000_COMP_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 130, 15.0f }, \
         { 241, 30.0f } \
    } }

/*
 *
 * IC7000 channel caps.
 */
#define IC7000_MEM_CAP {    \
        .freq = 1,  \
        .mode = 1,  \
        .width = 1, \
        .split = 1, \
        .tx_freq = 1,   \
        .tx_mode = 1,   \
        .tx_width = 1,  \
        .rptr_offs = 1, \
        .rptr_shift = 1, \
        .ctcss_tone = 1, \
        .ctcss_sql = 1, \
        .funcs = IC7000_FUNCS, \
        .levels = RIG_LEVEL_SET(IC7000_LEVELS), \
}

struct cmdparams ic7000_extcmds[] =
{
    { {.s = RIG_LEVEL_VOXDELAY}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x17}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_NONE} }
};

/*
 *  This function does the special bandwidth coding for IC-7000
 */
static int ic7000_r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                           unsigned char *md, signed char *pd)
{
    int err;

    err = rig2icom_mode(rig, vfo, mode, width, md, pd);

    if (err != RIG_OK)
    {
        return err;
    }

    // CAT section of manual says: nn = 0 -40 > bw = 50Hz > 3600Hz
    // Tested by Ian G3VPX 20201029
    // 0 - 9 > bw 50Hz to 500Hz in 50Hz steps
    // 10 - 40 > bw 600Hz to 3600Hz in 100Hz steps
    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width <= 500) { *pd = width / 50 - 1; }
        else if (width <= 3600) { *pd = width / 100 + 4; }
        else { *pd = 40; }
    }

    return RIG_OK;
}

/*
 * IC-7000 rig capabilities.
 */
static const struct icom_priv_caps IC7000_priv_caps =
{
    0x70,    /* default address */
    0,       /* 731 mode */
    0,       /* no XCHG */
    ic7000_ts_sc_list,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -1 },
    },
    .extcmds = ic7000_extcmds,
    .r2i_mode = ic7000_r2i_mode
};

const struct rig_caps ic7000_caps =
{
    RIG_MODEL(RIG_MODEL_IC7000),
    .model_name = "IC-7000",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".3",
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
    .has_get_func =  IC7000_FUNCS,
    .has_set_func =  IC7000_FUNCS,
    .has_get_level =  IC7000_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC7000_LEVELS),
    .has_get_parm =  IC7000_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC7000_PARMS),
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = { .min = { .i = 6 }, .max = { .i = 48 }, .step = { .i = 1 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 1 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { 10, RIG_DBLST_END, }, /* FIXME: TBC it's a guess */
    .attenuator =   { 12, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0), /* TODO */
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo =  0,
    .vfo_ops =  IC7000_VFO_OPS,
    .scan_ops =  IC7000_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   5,
    .chan_desc_sz =  0, /* TODO */

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM,  IC7000_MEM_CAP },
        { 100, 105, RIG_MTYPE_EDGE, IC7000_MEM_CAP },    /* two by two */
        { 106, 107, RIG_MTYPE_CALL, IC7000_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(199.999999), IC7000_ALL_RX_MODES, -1, -1, IC7000_VFOS},
        {MHz(400), MHz(470), IC7000_ALL_RX_MODES, -1, -1, IC7000_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC7000_OTHER_TX_MODES, W(2), W(100), IC7000_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(1, IC7000_OTHER_TX_MODES, W(2), W(100), IC7000_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(1, IC7000_OTHER_TX_MODES, W(2), W(50), IC7000_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, IC7000_OTHER_TX_MODES, W(2), W(35), IC7000_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(1, IC7000_AM_TX_MODES, W(1), W(40), IC7000_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, IC7000_AM_TX_MODES, W(1), W(40), IC7000_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(1, IC7000_AM_TX_MODES, W(2), W(20), IC7000_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, IC7000_OTHER_TX_MODES, W(2), W(14), IC7000_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(199.999999), IC7000_ALL_RX_MODES, -1, -1, IC7000_VFOS},
        {MHz(400), MHz(470), IC7000_ALL_RX_MODES, -1, -1, IC7000_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list2 =  { /* needs the 5 mhz channels added */
        FRQ_RNG_HF(2, IC7000_OTHER_TX_MODES, W(2), W(100), IC7000_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(2, IC7000_OTHER_TX_MODES, W(2), W(100), IC7000_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(2, IC7000_OTHER_TX_MODES, W(2), W(50), IC7000_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, IC7000_OTHER_TX_MODES, W(2), W(35), IC7000_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(2, IC7000_AM_TX_MODES, W(1), W(40), IC7000_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(2, IC7000_AM_TX_MODES, W(1), W(40), IC7000_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(2, IC7000_AM_TX_MODES, W(2), W(20), IC7000_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, IC7000_OTHER_TX_MODES, W(2), W(14), IC7000_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC7000_1HZ_TS_MODES, 1},
        {IC7000_NOT_TS_MODES, 10},
        {IC7000_ALL_RX_MODES, Hz(100)},
        {IC7000_ALL_RX_MODES, kHz(1)},
        {IC7000_ALL_RX_MODES, kHz(5)},
        {IC7000_ALL_RX_MODES, kHz(9)},
        {IC7000_ALL_RX_MODES, kHz(10)},
        {IC7000_ALL_RX_MODES, kHz(12.5)},
        {IC7000_ALL_RX_MODES, kHz(20)},
        {IC7000_ALL_RX_MODES, kHz(25)},
        {IC7000_ALL_RX_MODES, kHz(100)},
        {IC7000_NOT_TS_MODES, MHz(1)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! But duplication may speed up search.  Put the most commonly used modes first!  Remember these are defaults, with dsp rigs you can change them to anything you want except FM and WFM which are fixed */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(3)},
        {RIG_MODE_FM, kHz(10)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(7)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(250)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1.2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(3)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_WFM, kHz(280)},
        RIG_FLT_END,
    },

    .str_cal = IC7000_STR_CAL,
    .swr_cal = IC7000_SWR_CAL,
    .alc_cal = IC7000_ALC_CAL,
    .rfpower_meter_cal = IC7000_RFPOWER_METER_CAL,
    .comp_meter_cal = IC7000_COMP_METER_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& IC7000_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,
    .set_ant =  NULL,  /*automatically set by rig depending band */
    .get_ant =  NULL,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_parm =  NULL,
    .get_parm =  NULL,
    .set_mem =  icom_set_mem,
    .set_bank =  icom_set_bank,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_ptt =  icom_get_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  NULL,
    .set_rptr_shift =  icom_set_rptr_shift,
    .get_rptr_shift =  NULL,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
    .set_ctcss_tone =  icom_set_ctcss_tone,
    .get_ctcss_tone =  icom_get_ctcss_tone,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_dcs_code =  icom_set_dcs_code,
    .get_dcs_code =  icom_get_dcs_code,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  NULL,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
