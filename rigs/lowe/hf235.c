/*
 *  Hamlib Lowe backend - HF-235 description
 *  Copyright (c) 2003 by Stephane Fillod
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
#include "lowe.h"


#define HF235_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM|RIG_MODE_FM)

#define HF235_FUNC (RIG_FUNC_LOCK|RIG_FUNC_MUTE)

#define HF235_LEVEL_ALL (RIG_LEVEL_STRENGTH)

#define HF235_PARM_ALL (RIG_PARM_NONE)

#define HF235_VFO (RIG_VFO_A)

#define HF235_VFO_OPS (RIG_OP_NONE)


/*
 * HF-235 rig capabilities.
 *
 */

const struct rig_caps hf235_caps =
{
    RIG_MODEL(RIG_MODEL_HF235),
    .model_name = "HF-235",
    .mfg_name =  "Lowe",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,        /* and only basic support */
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  1,
    .timeout =  200,
    .retry =  3,

    .has_get_func =  HF235_FUNC,
    .has_set_func =  HF235_FUNC,
    .has_get_level =  HF235_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(HF235_LEVEL_ALL),
    .has_get_parm =  HF235_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(HF235_PARM_ALL),
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  7,
    .vfo_ops =  HF235_VFO_OPS,

    .chan_list =  {
        RIG_CHAN_END,   /* FIXME */
    },

    .rx_range_list1 =  {
        {kHz(30), MHz(30), HF235_MODES, -1, -1, HF235_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(30), MHz(30), HF235_MODES, -1, -1, HF235_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {HF235_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_AM, kHz(2.2)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv =  NULL,

    .set_freq =  lowe_set_freq,
    .get_freq =  lowe_get_freq,
    .set_mode =  lowe_set_mode,
    .get_mode =  lowe_get_mode,

//.set_func =  lowe_set_func,
    .get_level = lowe_get_level,
    .reset =  lowe_reset,
    .get_info =  lowe_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

