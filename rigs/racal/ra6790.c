/*
 *  Hamlib Racal backend - RA6790 description
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


#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "racal.h"


/* FIXME: ISB */
#define RA6790_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AMS|RIG_MODE_FM)

#define RA6790_FUNC (RIG_FUNC_NONE)

#define RA6790_LEVEL_ALL (RIG_LEVEL_RF|RIG_LEVEL_AGC|RIG_LEVEL_IF)

#define RA6790_PARM_ALL (RIG_PARM_NONE)

#define RA6790_VFO (RIG_VFO_A)

/*
 * ra6790 rig capabilities.
 *
 * Required A6A1 serial asynchronous interface
 *
 */
const struct rig_caps ra6790_caps =
{
    RIG_MODEL(RIG_MODEL_RA6790),
    .model_name = "RA6790/GM",
    .mfg_name =  "Racal",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  9600,
    .serial_data_bits =  7,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_EVEN,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  10,
    .timeout =  2000,
    .retry =  3,

    .has_get_func =  RA6790_FUNC,
    .has_set_func =  RA6790_FUNC,
    .has_get_level =  RA6790_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(RA6790_LEVEL_ALL),
    .has_get_parm =  RA6790_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(RA6790_PARM_ALL),
    .vfo_ops =  RIG_OP_NONE,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(8),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { RIG_CHAN_END, },

    .rx_range_list1 =  {
        {kHz(500), MHz(30) - 1, RA6790_MODES, -1, -1, RA6790_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(500), MHz(30) - 1, RA6790_MODES, -1, -1, RA6790_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {RA6790_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* at -3dB */
        {RIG_MODE_SSB, kHz(2.55)},      /* BW2 */
        {RA6790_MODES, Hz(300)},    /* BW1 */
        {RA6790_MODES, kHz(1)},     /* BW2 */
        {RA6790_MODES, kHz(3.2)},   /* BW3 */
        {RA6790_MODES, kHz(6)},     /* BW4 */
        {RA6790_MODES, kHz(20)},    /* BW5 */
        {RA6790_MODES, 0},  /* accept any BW */
        RIG_FLT_END,
    },

    .cfgparams = racal_cfg_params,

    .rig_init =  racal_init,
    .rig_cleanup =  racal_cleanup,
    .rig_open =  racal_open,
    .rig_close =  racal_close,
    .set_conf =  racal_set_conf,
    .get_conf =  racal_get_conf,

    .set_freq =  racal_set_freq,
    .get_freq =  racal_get_freq,
    .set_mode =  racal_set_mode,
    .get_mode =  racal_get_mode,
    .set_level =  racal_set_level,
    .get_level =  racal_get_level,

    .reset = racal_reset,
    .get_info = racal_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

