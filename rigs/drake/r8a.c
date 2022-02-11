/*
 *  Hamlib Drake backend - R-8A description
 *  Copyright (c) 2001-2010 by Stephane Fillod
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
#include "drake.h"


#define R8A_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_FM)

#define R8A_FUNC (RIG_FUNC_MN|RIG_FUNC_LOCK|RIG_FUNC_NB)

#define R8A_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define R8A_PARM_ALL (RIG_PARM_TIME)

#define R8A_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_VFO|RIG_VFO_MEM)

#define R8A_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_MCL|RIG_OP_CPY)

#define R8A_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)

#define R8A_STR_CAL { 16, { \
        {   0, -60 }, \
        {   1, -48 }, \
        {  11, -42 }, \
        {  27, -36 }, \
        {  39, -30 }, \
        {  51, -24 }, \
        {  64, -18 }, \
        {  80, -12 }, \
        {  97, -6 }, \
        { 116,  0 }, \
        { 138, 10 }, \
        { 163, 20 }, \
        { 195, 30 }, \
        { 217, 40 }, \
        { 228, 50 }, \
        { 255, 60 }, \
    } }

/*
 * channel caps.
 */
#define DRAKE_MEM_CAP { \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .ant = 1,   \
    .funcs = R8A_FUNC,  \
    .levels = RIG_LEVEL_AGC|RIG_LEVEL_ATT|RIG_LEVEL_PREAMP, \
    .channel_desc = 1,  \
}

/*
 * R-8A rig capabilities.
 *
 * specs: http://www.dxing.com/rx/r8.htm
 *
 */

const struct rig_caps r8a_caps =
{
    RIG_MODEL(RIG_MODEL_DKR8A),
    .model_name = "R-8A",
    .mfg_name =  "Drake",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  1,
    .timeout =  200,
    .retry =  3,

    .has_get_func =  R8A_FUNC,
    .has_set_func =  R8A_FUNC,
    .has_get_level =  R8A_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(R8A_LEVEL_ALL),
    .has_get_parm =  R8A_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(R8A_PARM_ALL),
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END },
    .attenuator =   { 10, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF, /* TODO: actually has RIG_TRN_RIG */
    .bank_qty =   0,
    .chan_desc_sz =  7,
    .vfo_ops =  R8A_VFO_OPS,

    .chan_list =  {
        {   0,  439, RIG_MTYPE_MEM, DRAKE_MEM_CAP },
        RIG_CHAN_END
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30), R8A_MODES, -1, -1, R8A_VFO, R8A_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), R8A_MODES, -1, -1, R8A_VFO, R8A_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {R8A_MODES, 10},
        {R8A_MODES, 100},
        {R8A_MODES, kHz(1)},
        {R8A_MODES, kHz(10)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(4)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(2.3)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(4)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_CW, kHz(1.8)},
        RIG_FLT_END,
    },
    .str_cal = R8A_STR_CAL,
    .priv =  NULL,

    .rig_init = drake_init,
    .rig_cleanup = drake_cleanup,

    .set_freq =  drake_set_freq,
    .get_freq =  drake_get_freq,
    .set_vfo =  drake_set_vfo,
    .get_vfo =  drake_get_vfo,
    .set_mode =  drake_set_mode,
    .get_mode =  drake_get_mode,
    .set_func = drake_set_func,
    .get_func = drake_get_func,
    .set_level = drake_set_level,
    .get_level = drake_get_level,
    .set_ant =  drake_set_ant,
    .get_ant =  drake_get_ant,
    .set_mem = drake_set_mem,
    .get_mem = drake_get_mem,
    .set_channel = drake_set_chan,
    .get_channel = drake_get_chan,
    .vfo_op = drake_vfo_op,
    .set_powerstat = drake_set_powerstat,
    .get_powerstat = drake_get_powerstat,
    .get_info =  drake_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

