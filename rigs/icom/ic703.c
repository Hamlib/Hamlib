/*
 *  Hamlib CI-V backend - description of IC-703
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#include <config.h>

#include <stdlib.h>

#include "hamlib/rig.h"
#include "icom.h"
#include "bandplan.h"
#include "idx_builtin.h"

#define IC703_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)

#define IC703_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC703_AM_TX_MODES (RIG_MODE_AM)

#define IC703_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_ANF|RIG_FUNC_MON)

#define IC703_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_ALC|RIG_LEVEL_SWR|RIG_LEVEL_METER|RIG_LEVEL_COMP|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_RF|RIG_LEVEL_AF|RIG_LEVEL_SQL|RIG_LEVEL_NR|RIG_LEVEL_IF|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT)

#define IC703_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define IC703_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC703_SCAN_OPS (RIG_SCAN_VFO|RIG_SCAN_MEM)

#define IC703_ANTS (RIG_ANT_1)

/*
 * IC703_REAL_STR_CAL is accurate measurements
 * IC703_STR_CAL is what the S-meter displays
 *
 * FIXME: calibration data cloned from IC706
 */
#define IC703_STR_CAL { 17, \
    { \
        {  45, -60 }, \
        {  46, -54 }, /* S0 */ \
        {  54, -48 }, \
        {  64, -42 }, \
        {  76, -36 }, \
        {  84, -30 }, \
        {  94, -24 }, \
        { 104, -18 }, \
        { 113, -12 }, \
        { 123, -6 }, \
        { 133, 0 },  /* S9 */ \
        { 144, 10 }, /* +10 */ \
        { 156, 20 }, /* +20 */ \
        { 170, 30 }, /* +30 */ \
        { 181, 40 }, /* +40 */ \
        { 192, 50 }, /* +50 */ \
        { 204, 60 }  /* +60 */ \
    } }

/*
 * ic703 rigs capabilities.
 */
static const struct icom_priv_caps ic703_priv_caps =
{
    0x68,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic706_ts_sc_list
};

const struct rig_caps ic703_caps =
{
    RIG_MODEL(RIG_MODEL_IC703),
    .model_name = "IC-703",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_NONE,
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
    .has_get_func =  IC703_FUNC_ALL,
    .has_set_func =  IC703_FUNC_ALL,
    .has_get_level =  IC703_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC703_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 10, 20, RIG_DBLST_END, }, /* FIXME: 2 levels */
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC703_VFO_OPS,
    .scan_ops =  IC703_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM },
        { 100, 105, RIG_MTYPE_EDGE },    /* two by two */
        { 106, 107, RIG_MTYPE_CALL },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(30), MHz(60), IC703_ALL_RX_MODES, -1, -1, IC703_VFO_ALL, IC703_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, IC703_OTHER_TX_MODES, W(0.1), W(10), IC703_VFO_ALL, IC703_ANTS),
        FRQ_RNG_6m(1, IC703_OTHER_TX_MODES, W(0.1), W(10), IC703_VFO_ALL, IC703_ANTS),
        FRQ_RNG_HF(1, IC703_AM_TX_MODES, W(0.1), W(4), IC703_VFO_ALL, IC703_ANTS), /* AM class */
        FRQ_RNG_6m(1, IC703_AM_TX_MODES, W(0.1), W(4), IC703_VFO_ALL, IC703_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {kHz(30), MHz(60), IC703_ALL_RX_MODES, -1, -1, IC703_VFO_ALL, IC703_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC703_OTHER_TX_MODES, W(0.1), W(10), IC703_VFO_ALL, IC703_ANTS),
        FRQ_RNG_6m(2, IC703_OTHER_TX_MODES, W(0.1), W(10), IC703_VFO_ALL, IC703_ANTS),
        FRQ_RNG_HF(2, IC703_AM_TX_MODES, W(0.1), W(4), IC703_VFO_ALL, IC703_ANTS), /* AM class */
        FRQ_RNG_6m(2, IC703_AM_TX_MODES, W(0.1), W(4), IC703_VFO_ALL, IC703_ANTS), /* AM class */
        RIG_FRNG_END,
    },


    .tuning_steps =     {
        {IC703_ALL_RX_MODES, 10},
        {IC703_ALL_RX_MODES, 100},
        {IC703_ALL_RX_MODES, kHz(1)},
        {IC703_ALL_RX_MODES, kHz(5)},
        {IC703_ALL_RX_MODES, kHz(9)},
        {IC703_ALL_RX_MODES, kHz(10)},
        {IC703_ALL_RX_MODES, 12500},
        {IC703_ALL_RX_MODES, kHz(20)},
        {IC703_ALL_RX_MODES, kHz(25)},
        {IC703_ALL_RX_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR, kHz(2.4)}, /* builtin FL-272 */
        {RIG_MODE_AM, kHz(9)},      /* mid w/ builtin FL-94 */
        {RIG_MODE_AM, kHz(2.4)},    /* narrow w/ builtin FL-272 */
        {RIG_MODE_FM, kHz(15)},     /* ?? TBC, mid w/ builtin FL-23+SFPC455E */
        {RIG_MODE_FM, kHz(9)},      /* narrow w/ builtin FL-94 */
        RIG_FLT_END,
    },
    .str_cal = IC703_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic703_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,
    .get_vfo =  icom_get_vfo,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_ptt =  icom_get_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .set_rptr_shift =  icom_set_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


