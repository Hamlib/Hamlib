/*
 *  Hamlib Kenwood backend - R5000 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
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
#include "kenwood.h"
#include "ic10.h"


#define R5000_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)

#define R5000_FUNC_ALL (RIG_FUNC_LOCK)

#define R5000_LEVEL_ALL RIG_LEVEL_NONE

#define R5000_PARM_ALL (RIG_PARM_TIME)

#define R5000_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define R5000_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)

#define R5000_SCAN_OPS (RIG_SCAN_VFO)

#define R5000_ANTS (RIG_ANT_1|RIG_ANT_2)

static struct kenwood_priv_caps  r5000_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .if_len =  32,
};

/*
 * R5000 rig capabilities, with IF-10 interface (like TS440).
 *
 * TODO: scan, get/set_channel, RIT
 */
const struct rig_caps r5000_caps =
{
    RIG_MODEL(RIG_MODEL_R5000),
    .model_name = "R-5000",
    .mfg_name =  "Kenwood",
    .version =  IC10_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  2,
    .timeout =  500,
    .retry =  3,

    .has_get_func =  R5000_FUNC_ALL,
    .has_set_func =  R5000_FUNC_ALL,
    .has_get_level =  R5000_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(R5000_LEVEL_ALL),
    .has_get_parm =  R5000_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(R5000_PARM_ALL),
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .vfo_ops =  R5000_VFO_OPS,
    .scan_ops =  R5000_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,


    .chan_list =  { {   0,  99, RIG_MTYPE_MEM, {IC10_CHANNEL_CAPS} },   /* TBC */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30), R5000_ALL_MODES, -1, -1, R5000_VFO},
        {MHz(108), MHz(174), R5000_ALL_MODES, -1, -1, R5000_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), R5000_ALL_MODES, -1, -1, R5000_VFO},
        {MHz(108), MHz(174), R5000_ALL_MODES, -1, -1, R5000_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  { RIG_FRNG_END, }, /* tx range */

    .tuning_steps =  {
        {R5000_ALL_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv = (void *)& r5000_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  ic10_set_freq,
    .get_freq =  ic10_get_freq,
    .set_mode =  ic10_set_mode,
    .get_mode =  ic10_get_mode,
    .set_vfo =  ic10_set_vfo,
    .get_vfo =  ic10_get_vfo,
    .set_func =  ic10_set_func,
    .get_func =  ic10_get_func,
    .set_parm =  ic10_set_parm,
    .get_parm =  ic10_get_parm,
    .set_ant =  ic10_set_ant,
    .get_ant =  ic10_get_ant,
    .set_mem =  ic10_set_mem,
    .get_mem =  ic10_get_mem,
    .vfo_op =  ic10_vfo_op,
    .set_trn =  ic10_set_trn,
    .get_trn =  ic10_get_trn,
    .scan =  ic10_scan,
    .set_channel =  ic10_set_channel,
    .get_channel =  ic10_get_channel,
    .set_powerstat =  ic10_set_powerstat,
    .get_powerstat =  ic10_get_powerstat,
    .get_info = ic10_get_info,
    .decode_event = ic10_decode_event,

    .hamlib_check_rig_caps = "HAMLIB_CHECK_RIG_CAPS"
};

/*
 * Function definitions below
 */

