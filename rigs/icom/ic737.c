/*
 *  Hamlib CI-V backend - description of IC-737
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
 * IC-737
 */


#define IC737_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

/*
 * 100W in all modes but AM (40W)
 */
#define IC737_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define IC737_AM_TX_MODES (RIG_MODE_AM)

#define IC737_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define IC737_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_CPY)
#define IC737_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_VFO)

#define IC737_ANTS (RIG_ANT_1|RIG_ANT_2)

static const struct icom_priv_caps ic737_priv_caps =
{
    0x3c,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic737_ts_sc_list,
    .antack_len = 2,
    .ant_count = 2
};

const struct rig_caps ic737_caps =
{
    RIG_MODEL(RIG_MODEL_IC737),
    .model_name = "IC-737",
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
    .vfo_ops =  IC737_VFO_OPS,
    .scan_ops =  IC737_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  100, RIG_MTYPE_MEM },
        { 101,  101, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(30), MHz(30), IC737_ALL_RX_MODES, -1, -1, IC737_VFO_ALL, IC737_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC737_OTHER_TX_MODES, W(10), W(100), IC737_VFO_ALL, IC737_ANTS),
        FRQ_RNG_HF(1, IC737_AM_TX_MODES, W(10), W(40), IC737_VFO_ALL, IC737_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 = {
        {kHz(30), MHz(30), IC737_ALL_RX_MODES, -1, -1, IC737_VFO_ALL, IC737_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC737_OTHER_TX_MODES, W(10), W(100), IC737_VFO_ALL, IC737_ANTS),
        FRQ_RNG_HF(2, IC737_AM_TX_MODES, W(10), W(40), IC737_VFO_ALL, IC737_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC737_ALL_RX_MODES, 10},
        {IC737_ALL_RX_MODES, kHz(1)},
        {IC737_ALL_RX_MODES, kHz(2)},
        {IC737_ALL_RX_MODES, kHz(3)},
        {IC737_ALL_RX_MODES, kHz(4)},
        {IC737_ALL_RX_MODES, kHz(5)},
        {IC737_ALL_RX_MODES, kHz(6)},
        {IC737_ALL_RX_MODES, kHz(7)},
        {IC737_ALL_RX_MODES, kHz(8)},
        {IC737_ALL_RX_MODES, kHz(9)},
        {IC737_ALL_RX_MODES, kHz(10)},
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

    .priv = (void *)& ic737_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,

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

