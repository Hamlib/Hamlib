/*
 *  Hamlib CI-V backend - description of Xiegu X108G and variations
 *  Adapted from IC-7000 code 2017 by Michael Black W9MDB
 *  As of this date there is no CAT manual for this rig
 *  The X108G is supposed to emulate the IC-7000 but there are a few
 *  differences as of 2017-02-11 in the return data from the rig
 *  It's quite possible they may fix all these eventually
 *  If they do the functions below can go back to the icom routines
 *  #1 the response to PTT
 *  #2 the inability to set_dsp_mode
 *  #3 setting split which impacts 3 routines
 *
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

#include <stdlib.h>

#include <hamlib/rig.h>
#include "token.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "bandplan.h"
#include "serial.h"

#define X108G_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_WFM|RIG_MODE_FMN|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB)
#define X108G_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB)
#define X108G_NOT_TS_MODES (X108G_ALL_RX_MODES &~X108G_1HZ_TS_MODES)

#define X108G_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB)
#define X108G_AM_TX_MODES (RIG_MODE_AM)

#define X108G_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_VSC|RIG_FUNC_LOCK|RIG_FUNC_ARO)

#define X108G_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC)

#define X108G_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define X108G_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_APO|RIG_PARM_TIME|RIG_PARM_BEEP)

#define X108G_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define X108G_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT|RIG_SCAN_PRIO)

#define X108G_ANTS (RIG_ANT_1|RIG_ANT_2) /* ant-1 is Hf-6m, ant-2 is 2m-70cm */

/*
 * Measurement by Mark, WA0TOP
 *
 * s/n 0503103.
 * Preamp off, ATT off, mode AM, f=10 MHz
 */
#define X108G_STR_CAL { 14, \
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

/*
 *
 * X108G channel caps.
 */
#define X108G_MEM_CAP {    \
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
        .funcs = X108G_FUNCS, \
        .levels = RIG_LEVEL_SET(X108G_LEVELS), \
}

/*
 * Prototypes
*/
static int x108g_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int x108g_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int x108g_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int x108g_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width);

static int x108g_rig_open(RIG *rig)
{
    int retval;

    ENTERFUNC;
    retval = icom_rig_open(rig);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_open failed with %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * taken from IC-7000 rig capabilities.
 *
 * TODO: complete command set (esp. the $1A bunch!) and testing..
 */
static struct icom_priv_caps x108g_priv_caps =
{
    0x70,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic7200_ts_sc_list
};


const struct rig_caps x108g_caps =
{
    RIG_MODEL(RIG_MODEL_X108G),
    .model_name = "X108G",
    .mfg_name =  "Xiegu",
    .version =  BACKEND_VER ".2",
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
    .has_get_func =  X108G_FUNCS,
    .has_set_func =  X108G_FUNCS,
    .has_get_level =  X108G_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(X108G_LEVELS),
    .has_get_parm =  X108G_PARMS,
    .has_set_parm =  RIG_PARM_SET(X108G_PARMS),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { 10, RIG_DBLST_END, }, /* FIXME: TBC it's a guess*/
    .attenuator =   { 12, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0), /* TODO */
    .targetable_vfo =  0,
    .vfo_ops =  X108G_VFO_OPS,
    .scan_ops =  X108G_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   5,
    .chan_desc_sz =  0, /* TODO */

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM,  X108G_MEM_CAP },
        { 100, 105, RIG_MTYPE_EDGE, X108G_MEM_CAP },    /* two by two */
        { 106, 107, RIG_MTYPE_CALL, X108G_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(1, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(1, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list2 =  { /* needs the 5 mhz channels added */
        FRQ_RNG_HF(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(2, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(2, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {X108G_1HZ_TS_MODES, 1},
        {X108G_NOT_TS_MODES, 10},
        {X108G_ALL_RX_MODES, Hz(100)},
        {X108G_ALL_RX_MODES, kHz(1)},
        {X108G_ALL_RX_MODES, kHz(5)},
        {X108G_ALL_RX_MODES, kHz(9)},
        {X108G_ALL_RX_MODES, kHz(10)},
        {X108G_ALL_RX_MODES, kHz(12.5)},
        {X108G_ALL_RX_MODES, kHz(20)},
        {X108G_ALL_RX_MODES, kHz(25)},
        {X108G_ALL_RX_MODES, kHz(100)},
        {X108G_NOT_TS_MODES, MHz(1)},
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

    .str_cal = X108G_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& x108g_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  x108g_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
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
    .set_ptt =  x108g_set_ptt,
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
    .set_split_freq =  x108g_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  x108g_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  x108g_set_split_vfo,
    .get_split_vfo =  NULL,
    //.set_powerstat = icom_set_powerstat,
    //.get_powerstat = icom_get_powerstat,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps x6100_caps =
{
    RIG_MODEL(RIG_MODEL_X6100),
    .model_name = "X6100",
    .mfg_name =  "Xiegu",
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
    .write_delay =  3,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  X108G_FUNCS,
    .has_set_func =  X108G_FUNCS,
    .has_get_level =  X108G_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(X108G_LEVELS),
    .has_get_parm =  X108G_PARMS,
    .has_set_parm =  RIG_PARM_SET(X108G_PARMS),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { 10, RIG_DBLST_END, }, /* FIXME: TBC it's a guess*/
    .attenuator =   { 12, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0), /* TODO */
    .vfo_ops =  X108G_VFO_OPS,
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .scan_ops =  X108G_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   5,
    .chan_desc_sz =  0, /* TODO */

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM,  X108G_MEM_CAP },
        { 100, 105, RIG_MTYPE_EDGE, X108G_MEM_CAP },    /* two by two */
        { 106, 107, RIG_MTYPE_CALL, X108G_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(1, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(1, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list2 =  { /* needs the 5 mhz channels added */
        FRQ_RNG_HF(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(2, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(2, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {X108G_1HZ_TS_MODES, 1},
        {X108G_NOT_TS_MODES, 10},
        {X108G_ALL_RX_MODES, Hz(100)},
        {X108G_ALL_RX_MODES, kHz(1)},
        {X108G_ALL_RX_MODES, kHz(5)},
        {X108G_ALL_RX_MODES, kHz(9)},
        {X108G_ALL_RX_MODES, kHz(10)},
        {X108G_ALL_RX_MODES, kHz(12.5)},
        {X108G_ALL_RX_MODES, kHz(20)},
        {X108G_ALL_RX_MODES, kHz(25)},
        {X108G_ALL_RX_MODES, kHz(100)},
        {X108G_NOT_TS_MODES, MHz(1)},
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

    .str_cal = X108G_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& x108g_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
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
    .set_ptt =  x108g_set_ptt,
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
    // testing with X6100 showed it rejected the 0x0f 0x01 command
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  NULL,
    //.set_powerstat = icom_set_powerstat,
    //.get_powerstat = icom_get_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps g90_caps =
{
    RIG_MODEL(RIG_MODEL_G90),
    .model_name = "G90",
    .mfg_name =  "Xiegu",
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
    .has_get_func =  X108G_FUNCS,
    .has_set_func =  X108G_FUNCS,
    .has_get_level =  X108G_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(X108G_LEVELS),
    .has_get_parm =  X108G_PARMS,
    .has_set_parm =  RIG_PARM_SET(X108G_PARMS),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { 10, RIG_DBLST_END, }, /* FIXME: TBC it's a guess*/
    .attenuator =   { 12, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0), /* TODO */
    .targetable_vfo =  0,
    .vfo_ops =  X108G_VFO_OPS,
    .scan_ops =  X108G_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   5,
    .chan_desc_sz =  0, /* TODO */

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM,  X108G_MEM_CAP },
        { 100, 105, RIG_MTYPE_EDGE, X108G_MEM_CAP },    /* two by two */
        { 106, 107, RIG_MTYPE_CALL, X108G_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(1, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(1, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list2 =  { /* needs the 5 mhz channels added */
        FRQ_RNG_HF(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(2, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(2, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {X108G_1HZ_TS_MODES, 1},
        {X108G_NOT_TS_MODES, 10},
        {X108G_ALL_RX_MODES, Hz(100)},
        {X108G_ALL_RX_MODES, kHz(1)},
        {X108G_ALL_RX_MODES, kHz(5)},
        {X108G_ALL_RX_MODES, kHz(9)},
        {X108G_ALL_RX_MODES, kHz(10)},
        {X108G_ALL_RX_MODES, kHz(12.5)},
        {X108G_ALL_RX_MODES, kHz(20)},
        {X108G_ALL_RX_MODES, kHz(25)},
        {X108G_ALL_RX_MODES, kHz(100)},
        {X108G_NOT_TS_MODES, MHz(1)},
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

    .str_cal = X108G_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& x108g_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
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
    .set_ptt =  x108g_set_ptt,
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
    //.set_split_freq =  x108g_set_split_freq,
    //.get_split_freq =  icom_get_split_freq,
    //.set_split_mode =  x108g_set_split_mode,
    //.get_split_mode =  icom_get_split_mode,
    //.set_split_vfo =  x108g_set_split_vfo,
    //.get_split_vfo =  NULL,
    //.set_powerstat = icom_set_powerstat,
    //.get_powerstat = icom_get_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps x5105_caps =
{
    RIG_MODEL(RIG_MODEL_X5105),
    .model_name = "X5105",
    .mfg_name =  "Xiegu",
    .version =  BACKEND_VER ".1",
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
    .has_get_func =  X108G_FUNCS,
    .has_set_func =  X108G_FUNCS,
    .has_get_level =  X108G_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(X108G_LEVELS),
    .has_get_parm =  X108G_PARMS,
    .has_set_parm =  RIG_PARM_SET(X108G_PARMS),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { 10, RIG_DBLST_END, }, /* FIXME: TBC it's a guess*/
    .attenuator =   { 12, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0), /* TODO */
    .targetable_vfo =  0,
    .vfo_ops =  X108G_VFO_OPS,
    .scan_ops =  X108G_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   5,
    .chan_desc_sz =  0, /* TODO */

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM,  X108G_MEM_CAP },
        { 100, 105, RIG_MTYPE_EDGE, X108G_MEM_CAP },    /* two by two */
        { 106, 107, RIG_MTYPE_CALL, X108G_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(1, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(1, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(1, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(1, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(199.999999), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS},
        {MHz(400), MHz(470), X108G_ALL_RX_MODES, -1, -1, X108G_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list2 =  { /* needs the 5 mhz channels added */
        FRQ_RNG_HF(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(2, X108G_OTHER_TX_MODES, W(2), W(100), X108G_VFOS, RIG_ANT_1),
        FRQ_RNG_2m(2, X108G_OTHER_TX_MODES, W(2), W(50), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(35), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_HF(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(2, X108G_AM_TX_MODES, W(1), W(40), X108G_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(2, X108G_AM_TX_MODES, W(2), W(20), X108G_VFOS, RIG_ANT_2),
        FRQ_RNG_70cm(2, X108G_OTHER_TX_MODES, W(2), W(14), X108G_VFOS, RIG_ANT_2),
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {X108G_1HZ_TS_MODES, 1},
        {X108G_NOT_TS_MODES, 10},
        {X108G_ALL_RX_MODES, Hz(100)},
        {X108G_ALL_RX_MODES, kHz(1)},
        {X108G_ALL_RX_MODES, kHz(5)},
        {X108G_ALL_RX_MODES, kHz(9)},
        {X108G_ALL_RX_MODES, kHz(10)},
        {X108G_ALL_RX_MODES, kHz(12.5)},
        {X108G_ALL_RX_MODES, kHz(20)},
        {X108G_ALL_RX_MODES, kHz(25)},
        {X108G_ALL_RX_MODES, kHz(100)},
        {X108G_NOT_TS_MODES, MHz(1)},
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

    .str_cal = X108G_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& x108g_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
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
    .set_ptt =  x108g_set_ptt,
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
    .set_split_freq =  NULL,
    .get_split_freq =  NULL,
    .set_split_mode =  NULL,
    .get_split_mode =  NULL,
    .set_split_vfo =  NULL,
    .get_split_vfo =  NULL,
    .set_powerstat = NULL,
    .get_powerstat = NULL,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * x108g_set_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL
 * The response from the x108g isn't quite right at this time
 * Eventually they may fix their firmware and we can use the icom_set_split_vfo
 */
int x108g_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char ackbuf[MAXFRAMELEN], pttbuf[1];
    int ack_len = sizeof(ackbuf), retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    pttbuf[0] = ptt == RIG_PTT_ON ? 1 : 0;

    retval = icom_transaction(rig, C_CTL_PTT, S_PTT, pttbuf, 1,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* X108G doesn't quite follow ICOM protocol -- returns 0x1c instead of 0xfb */
    if (ackbuf[0] != 0xfb && (ack_len != 3 || ackbuf[0] != 0x1c))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d, ptt=%d\n", __func__,
                  ackbuf[0], ack_len, ptt);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * The response from the x108g isn't quite right at this time
 * Eventually they may fix their firmware and we can use the icom_set_split_vfo
 * x108g_set_split
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int x108g_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct icom_priv_data *priv = (struct icom_priv_data *)rig->state.priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), rc;
    int split_sc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (split)
    {
    case RIG_SPLIT_OFF:
        split_sc = S_SPLT_OFF;
        break;

    case RIG_SPLIT_ON:
        split_sc = S_SPLT_ON;

        if (!priv->split_on)
        {
            /* ensure VFO A is Rx and VFO B is Tx as we assume that elsewhere */
            if ((rig->state.vfo_list & (RIG_VFO_A | RIG_VFO_B)) == (RIG_VFO_A | RIG_VFO_B))
            {
                if (RIG_OK != (rc = icom_set_vfo(rig, RIG_VFO_A))) { return rc; }
            }
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported split %d", __func__, split);
        return -RIG_EINVAL;
    }

    if (RIG_OK != (rc = icom_transaction(rig, C_CTL_SPLT, split_sc, NULL, 0,

                                         ackbuf, &ack_len))) { return rc; }

    if (ack_len != 2 || ackbuf[0] != 0x0f)   // instead of len=1 & ACK
    {
        rig_debug(RIG_DEBUG_ERR, "x108g_set_split: ack NG (%#.2x), "
                  "len=%d\n", ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    priv->split_on = RIG_SPLIT_ON == split;
    return RIG_OK;
}

/*
 * x108g_set_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *
 * Assumes also that the current VFO is the rx VFO.
 */
static int x108g_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    int rc;
    vfo_t rx_vfo, tx_vfo;
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rs = &rig->state;
    priv = (struct icom_priv_data *)rs->priv;

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (rc = icom_vfo_op(rig, vfo, RIG_OP_XCHG))) { return rc; }

        if (RIG_OK != (rc = icom_set_freq(rig, RIG_VFO_CURR, tx_freq))) { return rc; }

        if (RIG_OK != (rc = icom_vfo_op(rig, vfo, RIG_OP_XCHG))) { return rc; }

        return rc;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
             current VFO is VFO A and the split Tx VFO is always VFO B. These
             assumptions allow us to deal with the lack of VFO and split
             queries */
    if ((rig->state.vfo_list & (RIG_VFO_A | RIG_VFO_B)) == (RIG_VFO_A | RIG_VFO_B)
            && priv->split_on)                              /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
                 split for certainty */
        if (RIG_OK != (rc = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0,
                                             ackbuf, &ack_len))) { return rc; }

        if (ack_len != 2 || ackbuf[0] != 0x0f)
        {
            rig_debug(RIG_DEBUG_ERR, "x108g_set_split_freq: ack NG (%#.2x), "
                      "len=%d\n", ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    if (RIG_OK != (rc = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo))) { return rc; }

    if (RIG_OK != (rc = icom_set_vfo(rig, tx_vfo))) { return rc; }

    if (RIG_OK != (rc = rig_set_freq(rig, RIG_VFO_CURR, tx_freq))) { return rc; }

    if (RIG_OK != (rc = icom_set_vfo(rig, rx_vfo))) { return rc; }

    if ((rig->state.vfo_list & (RIG_VFO_A | RIG_VFO_B)) == (RIG_VFO_A | RIG_VFO_B)
            && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK != (rc = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0,
                                             ackbuf, &ack_len))) { return rc; }
    }

    return rc;
}

/*
 * x108g_set_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 */
static int x108g_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width)
{
    int rc;
    vfo_t rx_vfo, tx_vfo;
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rs = &rig->state;
    priv = (struct icom_priv_data *)rs->priv;

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (rc = icom_vfo_op(rig, vfo, RIG_OP_XCHG))) { return rc; }

        if (RIG_OK != (rc = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                                                tx_width))) { return rc; }

        if (RIG_OK != (rc = icom_vfo_op(rig, vfo, RIG_OP_XCHG))) { return rc; }

        return rc;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
             current VFO is VFO A and the split Tx VFO is always VFO B. These
             assumptions allow us to deal with the lack of VFO and split
             queries */
    if ((rig->state.vfo_list & (RIG_VFO_A | RIG_VFO_B)) == (RIG_VFO_A | RIG_VFO_B)
            && priv->split_on)                              /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
                 split for certainty */
        if (RIG_OK != (rc = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0,
                                             ackbuf, &ack_len))) { return rc; }

        if (ack_len != 2 || ackbuf[0] != 0x0f)
        {
            rig_debug(RIG_DEBUG_ERR, "x108g_set_split_mode: ack NG (%#.2x), "
                      "len=%d\n", ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    if (RIG_OK != (rc = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo))) { return rc; }

    if (RIG_OK != (rc = icom_set_vfo(rig, tx_vfo))) { return rc; }

    if (RIG_OK != (rc = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                                            tx_width))) { return rc; }

    if (RIG_OK != (rc = icom_set_vfo(rig, rx_vfo))) { return rc; }

    if ((rig->state.vfo_list & (RIG_VFO_A | RIG_VFO_B)) == (RIG_VFO_A | RIG_VFO_B)
            && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK != (rc = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0,
                                             ackbuf, &ack_len))) { return rc; }
    }

    return rc;
}

