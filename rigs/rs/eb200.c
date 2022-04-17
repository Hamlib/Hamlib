/*
 *  Hamlib R&S backend - EB200 description
 *  Copyright (c) 2009-2010 by Stephane Fillod
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
#include "rs.h"


/* TODO: PULSE and IQ ? */
#define EB200_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define EB200_FUNC (RIG_FUNC_SQL|RIG_FUNC_AFC|RIG_FUNC_LOCK)

#define EB200_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_AGC|\
                        RIG_LEVEL_RF|RIG_LEVEL_AF|RIG_LEVEL_STRENGTH)

#define EB200_PARM_ALL (RIG_PARM_NONE)

#define EB200_VFO (RIG_VFO_A)

#define EB200_VFO_OPS (RIG_OP_NONE)

#define EB200_ANTS (RIG_ANT_1)

#define EB200_MEM_CAP {    \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
        .ant = 1,     \
        .funcs = EB200_FUNC, \
        .levels = RIG_LEVEL_SET(EB200_LEVEL_ALL), \
        .channel_desc=1, \
        .flags = RIG_CHFLAG_SKIP, \
}


/*
 * EB200 rig capabilities.
 *
 * Needs option ESMBR2 for computer operation RS-232C
 *
 * TODO
 *  - set/get_channels
 *  - get_dcd
 *  - set_ant
 */

const struct rig_caps eb200_caps =
{
    RIG_MODEL(RIG_MODEL_EB200),
    .model_name = "EB200",
    .mfg_name =  "Rohde&Schwarz",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,    /* 7E2 */
    .serial_rate_max =  115200, /* 7E1, RTS/CTS */
    .serial_data_bits =  7,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_EVEN,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .has_get_func =  EB200_FUNC,
    .has_set_func =  EB200_FUNC,
    .has_get_level =  EB200_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(EB200_LEVEL_ALL),
    .has_get_parm =  EB200_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(EB200_PARM_ALL),
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { 32, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  7, /* FIXME */
    .vfo_ops =  EB200_VFO_OPS,

    .chan_list =  {
        { 0, 999, RIG_MTYPE_MEM, EB200_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(10), GHz(3), EB200_MODES, -1, -1, EB200_VFO, EB200_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(10), GHz(3), EB200_MODES, -1, -1, EB200_VFO, EB200_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {EB200_MODES, 1},
        {EB200_MODES, 10},
        {EB200_MODES, 100},
        {EB200_MODES, 1000},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_WFM, kHz(150)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(15)},
        {EB200_MODES, kHz(2.4)},
        {EB200_MODES, kHz(1.5)},
        {EB200_MODES,  Hz(150)},
        {EB200_MODES,  Hz(300)},
        {EB200_MODES,  Hz(600)},
        {EB200_MODES, kHz(6)},
        {EB200_MODES, kHz(9)},
        {EB200_MODES, kHz(15)},
        {EB200_MODES, kHz(30)},
        {EB200_MODES, kHz(50)},
        {EB200_MODES, kHz(120)},
        RIG_FLT_END,
    },
    .priv =  NULL,

    .set_freq =  rs_set_freq,
    .get_freq =  rs_get_freq,
    .set_mode =  rs_set_mode,
    .get_mode =  rs_get_mode,
    .set_level =  rs_set_level,
    .get_level =  rs_get_level,
    .set_func =  rs_set_func,
    .get_func =  rs_get_func,
    .get_info =  rs_get_info,
    .reset =  rs_reset,

#if 0
    /* TODO */
    .rig_open =  rs_rig_open,
    .set_channel =  rs_set_channel,
    .get_channel =  rs_get_channel,
#endif
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

