/*
 *  Hamlib Watkins-Johnson backend - WJ-8888 description
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
#include "wj.h"


/* modes: what about ISB(Idependant Sideband)? */
#define WJ8888_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_AMS)

#define WJ8888_FUNC (RIG_FUNC_NONE)

#define WJ8888_LEVEL (RIG_LEVEL_RF|RIG_LEVEL_AGC|RIG_LEVEL_IF|RIG_LEVEL_RAWSTR)

#define WJ8888_VFO (RIG_VFO_A)

/* FIXME: real measures */
#define WJ8888_STR_CAL { 2, \
    { \
        {  0x00, -60 }, /* -115.5dBm .. -99.5dBm */ \
        {  0x7f, 60 } \
    } }

/*
 * WJ-8888 receiver capabilities.
 *
 * Needs async I/O board
 *
 *
 * TODO: BFO
 */
const struct rig_caps wj8888_caps =
{
    RIG_MODEL(RIG_MODEL_WJ8888),
    .model_name = "WJ-8888",
    .mfg_name =  "Watkins-Johnson",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  75,     /* jumper E26 */
    .serial_rate_max =  9600,   /* jumper E19 */
    .serial_data_bits =  8,
    .serial_stop_bits =  1,     /* jumper between E5 & E6 */
    .serial_parity =  RIG_PARITY_NONE,  /* no jumper between E3 & E4 */
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  5, /* typical 5ms, max 15ms */
    .timeout =  2000,
    .retry =  3,

    .has_get_func =  WJ8888_FUNC,
    .has_set_func =  WJ8888_FUNC,
    .has_get_level =  WJ8888_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(WJ8888_LEVEL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .vfo_ops =  RIG_OP_NONE,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 0x7f } },
    },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(8), /* IF at 455kHz */
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .str_cal = WJ8888_STR_CAL,

    .cfgparams = wj_cfg_params,

    .chan_list =  { RIG_CHAN_END, },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), WJ8888_MODES, -1, -1, WJ8888_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(500), MHz(30), WJ8888_MODES, -1, -1, WJ8888_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {WJ8888_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {WJ8888_MODES, kHz(2)},
        {WJ8888_MODES, Hz(500)},
        {WJ8888_MODES, kHz(4)},
        {WJ8888_MODES, kHz(8)},
        /* option (in spare, to be fixed) */
        {WJ8888_MODES, Hz(200)},
        {WJ8888_MODES, kHz(1)},
        {WJ8888_MODES, kHz(3)},
        {WJ8888_MODES, kHz(6)},
        {WJ8888_MODES, kHz(12)},
        {WJ8888_MODES, kHz(16)},
        RIG_FLT_END,
    },

    .rig_init =  wj_init,
    .rig_cleanup =  wj_cleanup,
    .set_conf =  wj_set_conf,
    .get_conf =  wj_get_conf,

    .set_freq =  wj_set_freq,
    .get_freq =  wj_get_freq,
    .set_mode =  wj_set_mode,
    .get_mode =  wj_get_mode,
    .set_level =  wj_set_level,
    .get_level =  wj_get_level,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

