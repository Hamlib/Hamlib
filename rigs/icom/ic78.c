/*
 *  Hamlib CI-V backend - description of IC-78
 *  Copyright (c) 2004 by Stephane Fillod
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
#include "icom.h"
#include "bandplan.h"
#include "idx_builtin.h"

#define IC78_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)

#define IC78_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define IC78_AM_TX_MODES (RIG_MODE_AM)

#define IC78_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP)

#define IC78_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_RAWSTR|RIG_LEVEL_RF|RIG_LEVEL_AF|RIG_LEVEL_SQL)

#define IC78_VFO_ALL (RIG_VFO_A|RIG_VFO_MEM)

#define IC78_VFO_OPS (RIG_OP_NONE)
#define IC78_SCAN_OPS (RIG_SCAN_VFO|RIG_SCAN_MEM)

#define IC78_ANTS (RIG_ANT_1)

/*
 * IC78_STR_CAL is what the S-meter displays
 *
 * FIXME: real measures!
 */
#define IC78_STR_CAL { 2, \
    { \
        {  0, -60 }, \
        { 255, 60 } \
    } }

/*
 * ic78 rigs capabilities.
 */
static const struct icom_priv_caps ic78_priv_caps =
{
    0x62,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic706_ts_sc_list
};

const struct rig_caps ic78_caps =
{
    RIG_MODEL(RIG_MODEL_IC78),
    .model_name = "IC-78",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
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
    .has_get_func =  IC78_FUNC_ALL,
    .has_set_func =  IC78_FUNC_ALL,
    .has_get_level =  IC78_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC78_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .preamp =   { 10, 20, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC78_VFO_OPS,
    .scan_ops =  IC78_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM },
        { 100, 100, RIG_MTYPE_CALL },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(30), MHz(30) - 1, IC78_ALL_RX_MODES, -1, -1, IC78_VFO_ALL, IC78_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, IC78_OTHER_TX_MODES, W(2), W(100), IC78_VFO_ALL, IC78_ANTS),
        FRQ_RNG_HF(1, IC78_AM_TX_MODES, W(2), W(40), IC78_VFO_ALL, IC78_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {kHz(30), MHz(30) - 1, IC78_ALL_RX_MODES, -1, -1, IC78_VFO_ALL, IC78_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC78_OTHER_TX_MODES, W(2), W(100), IC78_VFO_ALL, IC78_ANTS),
        FRQ_RNG_HF(2, IC78_AM_TX_MODES, W(2), W(40), IC78_VFO_ALL, IC78_ANTS), /* AM class */
        RIG_FRNG_END,
    },


    .tuning_steps =     {
        {IC78_ALL_RX_MODES, 10},
        {IC78_ALL_RX_MODES, 100},
        {IC78_ALL_RX_MODES, kHz(1)},
        {IC78_ALL_RX_MODES, kHz(5)},
        {IC78_ALL_RX_MODES, kHz(9)},
        {IC78_ALL_RX_MODES, kHz(10)},
        {IC78_ALL_RX_MODES, 12500},
        {IC78_ALL_RX_MODES, kHz(20)},
        {IC78_ALL_RX_MODES, kHz(25)},
        {IC78_ALL_RX_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR | RIG_MODE_AM, kHz(2.4)},
        RIG_FLT_END,
    },
    .str_cal = IC78_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic78_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .scan =  icom_scan,
    .get_dcd =  icom_get_dcd,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


