/*
 *  Hamlib CI-V backend - description of IC-736 and variations
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

#include <hamlib/config.h>

#include <stdlib.h>

#include <hamlib/rig.h>
#include <bandplan.h>
#include "icom.h"


/*
 * IC-736
 */


#define IC736_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

/*
 * 100W in all modes but AM (40W)
 */
#define IC736_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define IC736_AM_TX_MODES (RIG_MODE_AM)

#define IC736_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define IC736_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_CPY)
#define IC736_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_VFO)

#define IC736_ANTS 0    /* FIXME: declare both antenna connectors */

#define IC736_STR_CAL { 0, { } }

/*
 */
static const struct icom_priv_caps ic736_priv_caps =
{
    0x40,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic737_ts_sc_list
};

const struct rig_caps ic736_caps =
{
    RIG_MODEL(RIG_MODEL_IC736),
    .model_name = "IC-736",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC736_VFO_OPS,
    .scan_ops =  IC736_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,   89, RIG_MTYPE_MEM, IC_MIN_MEM_CAP },
        {  90,   99, RIG_MTYPE_MEM, IC_MIN_MEM_CAP },    /* FIXME: split */
        { 100,  101, RIG_MTYPE_EDGE, IC_MIN_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(500), MHz(30), IC736_ALL_RX_MODES, -1, -1, IC736_VFO_ALL, IC736_ANTS},
        {MHz(50), MHz(54), IC736_ALL_RX_MODES, -1, -1, IC736_VFO_ALL, IC736_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC736_OTHER_TX_MODES, W(5), W(100), IC736_VFO_ALL, IC736_ANTS),
        FRQ_RNG_6m(1, IC736_OTHER_TX_MODES, W(5), W(100), IC736_VFO_ALL, IC736_ANTS),
        FRQ_RNG_HF(1, IC736_AM_TX_MODES, W(4), W(40), IC736_VFO_ALL, IC736_ANTS), /* AM class */
        FRQ_RNG_6m(1, IC736_AM_TX_MODES, W(4), W(40), IC736_VFO_ALL, IC736_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 = {
        {kHz(500), MHz(30), IC736_ALL_RX_MODES, -1, -1, IC736_VFO_ALL, IC736_ANTS},
        {MHz(50), MHz(54), IC736_ALL_RX_MODES, -1, -1, IC736_VFO_ALL, IC736_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC736_OTHER_TX_MODES, W(5), W(100), IC736_VFO_ALL, IC736_ANTS),
        FRQ_RNG_6m(2, IC736_OTHER_TX_MODES, W(5), W(100), IC736_VFO_ALL, IC736_ANTS),
        FRQ_RNG_HF(2, IC736_AM_TX_MODES, W(4), W(40), IC736_VFO_ALL, IC736_ANTS), /* AM class */
        FRQ_RNG_6m(2, IC736_AM_TX_MODES, W(4), W(40), IC736_VFO_ALL, IC736_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps = {           /* TBC */
        {IC736_ALL_RX_MODES, 10},
        {IC736_ALL_RX_MODES, kHz(1)},
        {IC736_ALL_RX_MODES, kHz(2)},
        {IC736_ALL_RX_MODES, kHz(3)},
        {IC736_ALL_RX_MODES, kHz(4)},
        {IC736_ALL_RX_MODES, kHz(5)},
        {IC736_ALL_RX_MODES, kHz(6)},
        {IC736_ALL_RX_MODES, kHz(7)},
        {IC736_ALL_RX_MODES, kHz(8)},
        {IC736_ALL_RX_MODES, kHz(9)},
        {IC736_ALL_RX_MODES, kHz(10)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(2.1)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic736_priv_caps,
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
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ts =  icom_set_ts,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

