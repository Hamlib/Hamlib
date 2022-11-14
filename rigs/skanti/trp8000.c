/*
 *  Hamlib Skanti backend - TRP8000 description
 *  Copyright (c) 2004-2005 by Stephane Fillod
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


#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "skanti.h"


/* modes: what about MCW, R3E ? */
#define TRP8000_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TRP8000_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TRP8000_AM_TX_MODES RIG_MODE_AM

#define TRP8000_FUNC (RIG_FUNC_NONE)

#define TRP8000_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AGC|RIG_LEVEL_ATT|RIG_LEVEL_PREAMP)

#define TRP8000_PARM_ALL (RIG_PARM_NONE)

#define TRP8000_VFO (RIG_VFO_A) /* TBC */

/*
 * trp8000 rig capabilities.
 *
 *
 * TODO: TUNING, BFO, SENSITIVITY(RF gain?)
 */
const struct rig_caps trp8000_caps =
{
    RIG_MODEL(RIG_MODEL_TRP8000),
    .model_name = "TRP8000",
    .mfg_name =  "Skanti",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  300,
    .serial_data_bits =  7,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_ODD,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  2000,
    .retry =  3,

    .has_get_func =  TRP8000_FUNC,
    .has_set_func =  TRP8000_FUNC,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_SET(TRP8000_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_SET(TRP8000_PARM_ALL),
    .vfo_ops =  RIG_OP_TUNE,
    .preamp =   { 10, RIG_DBLST_END },  /* TBC */
    .attenuator =   { 20, RIG_DBLST_END },  /* TBC */
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { RIG_CHAN_END, },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(500), MHz(30), TRP8000_ALL_MODES, -1, -1, TRP8000_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(2), MHz(30), TRP8000_AM_TX_MODES, W(4), W(40), TRP8000_VFO},
        {MHz(2), MHz(30), TRP8000_OTHER_TX_MODES, W(10), W(100), TRP8000_VFO},
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {TRP8000_ALL_MODES, 10},
        {TRP8000_ALL_MODES, 100},
        {TRP8000_ALL_MODES, kHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* rough guesses */
        {TRP8000_ALL_MODES, kHz(2.7)},  /* intermit */
        {TRP8000_ALL_MODES, kHz(2.1)},  /* narrow */
        {TRP8000_ALL_MODES, kHz(6)},    /* wide */
        {TRP8000_ALL_MODES, Hz(500)},   /* very narrow */
        RIG_FLT_END,
    },

    .set_freq =  skanti_set_freq,
    .set_mode =  skanti_set_mode,
    .set_split_freq =  skanti_set_split_freq,
    .set_ptt =  skanti_set_ptt,
    .vfo_op =  skanti_vfo_op,
    .set_level =  skanti_set_level,
    .reset =  skanti_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

