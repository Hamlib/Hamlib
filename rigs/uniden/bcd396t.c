/*
 *  Hamlib Uniden backend - BCD396T description
 *  Copyright (c) 2001-2008 by Stephane Fillod
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

#include "hamlib/rig.h"
/*#include "uniden.h"*/
#include "uniden_digital.h"


/* TODO: All these defines have to be filled in properly */

#define BCD396T_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define BCD396T_FUNC (RIG_FUNC_MUTE)

#define BCD396T_LEVEL_ALL (RIG_LEVEL_ATT)

#define BCD396T_PARM_ALL (RIG_PARM_BEEP|RIG_PARM_BACKLIGHT)

#define BCD396T_VFO (RIG_VFO_A|RIG_VFO_B)

#define BCD396T_CHANNEL_CAPS \
            .freq=1,\
            .flags=1, /* L/O */

/*
 * bcd396t rig capabilities.
 *
 * TODO: check this with manual or web site.
 */
const struct rig_caps bcd396t_caps =
{
    RIG_MODEL(RIG_MODEL_BCD396T),
    .model_name = "BCD-396T",
    .mfg_name =  "Uniden",
    .version =  BACKEND_DIGITAL_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_TRUNKSCANNER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  1,
    .timeout =  2000,
    .retry =  3,

    .has_get_func =  BCD396T_FUNC,
    .has_set_func =  BCD396T_FUNC,
    .has_get_level =  BCD396T_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(BCD396T_LEVEL_ALL),
    .has_get_parm =  BCD396T_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(BCD396T_PARM_ALL),
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,    /* FIXME: CTCSS list? */
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        { 0, 5999, RIG_MTYPE_MEM, {BCD396T_CHANNEL_CAPS} }, /* Really 6000 channels? */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {MHz(25), MHz(512), BCD396T_MODES, -1, -1, BCD396T_VFO},
        {MHz(764), MHz(775.9875), BCD396T_MODES, -1, -1, BCD396T_VFO},
        {MHz(794), MHz(823.9875), BCD396T_MODES, -1, -1, BCD396T_VFO},
        {MHz(849.0125), MHz(868.9875), BCD396T_MODES, -1, -1, BCD396T_VFO},
        {MHz(894.0125), MHz(956), BCD396T_MODES, -1, -1, BCD396T_VFO},
        {MHz(1240), MHz(1300), BCD396T_MODES, -1, -1, BCD396T_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {BCD396T_MODES, 10},   /* FIXME: add other ts */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_AM | RIG_MODE_FM, kHz(8)},
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },

    .get_info =  uniden_digital_get_info,
    .set_freq =  uniden_digital_set_freq,
    .get_freq =  uniden_digital_get_freq,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

