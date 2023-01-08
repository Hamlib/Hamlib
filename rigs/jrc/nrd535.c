/*
 *  Hamlib JRC backend - NRD-535 DSP description
 *  Copyright (c) 2001-2004 by Stephane Fillod
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
#include "idx_builtin.h"
#include "jrc.h"


#define NRD535_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_ECSS|RIG_MODE_FAX)    /* + FAX */

#define NRD535_FUNC (RIG_FUNC_FAGC|RIG_FUNC_NB)

#define NRD535_LEVEL (RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_ATT|RIG_LEVEL_IF|RIG_LEVEL_AGC|RIG_LEVEL_CWPITCH) /*RIG_LEVEL_BWC*/

/* FIXME: add more from "U" command */
#define NRD535_PARM (RIG_PARM_TIME|RIG_PARM_BEEP)

#define NRD535_VFO (RIG_VFO_VFO|RIG_VFO_MEM)

/*
 * NRD-535, specs from http://mods.dk
 *
 * FIXME: measure S-meter levels
 */
#define NRD535_STR_CAL { 16, { \
        {   0, 60 }, \
        {  71, 50 }, \
        {  75, 40 }, \
        {  81, 30 }, \
        {  87, 20 }, \
        {  93, 10 }, \
        { 100,  0 }, \
        { 103, -6 }, \
        { 107, -12 }, \
        { 112, -18 }, \
        { 118, -24 }, \
        { 124, -30 }, \
        { 133, -36 }, \
        { 143, -42 }, \
        { 152, -48 }, \
        { 255, -60 }, \
    } }


/*
 * channel caps.
 */
#define NRD535_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .funcs = RIG_FUNC_FAGC, \
    .levels = RIG_LEVEL_ATT|RIG_LEVEL_AGC, \
}

static const struct jrc_priv_caps nrd535_priv_caps =
{
    .max_freq_len = 8,
    .info_len = 14,
    .mem_len = 17,
    .pbs_info_len = 7,
    .pbs_len = 4,
    .beep = 90,
    .beep_len = 2,
    .cw_pitch = "U2"
};

/*
 * NRD-535 rig capabilities.
 *
 */
const struct rig_caps nrd535_caps =
{
    RIG_MODEL(RIG_MODEL_NRD535),
    .model_name = "NRD-535D",
    .mfg_name =  "JRC",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  21,
    .timeout =  250,
    .retry =  3,

    .has_get_func =  NRD535_FUNC,
    .has_set_func =  NRD535_FUNC,
    .has_get_level =  NRD535_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(NRD535_LEVEL),
    .has_get_parm =  RIG_PARM_TIME,
    .has_set_parm =  RIG_PARM_SET(NRD535_PARM),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_ATT] = { .min = { .i = 0 }, .max = { .i = 20 } },
        [LVL_IF] = { .min = { .i = -2000 }, .max = { .i = 2000 } },
        [LVL_CWPITCH] = { .min = { .i = -5000 }, .max = { .i = 5000 } },
        /*[LVL_BWC] = { .min = { .i = 500 }, .max = { .i = 5500 }, .step = { .i = 10} },*/
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { 20, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(2),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_RIG,
    .vfo_ops =  RIG_OP_FROM_VFO,
    .scan_ops =  RIG_SCAN_STOP | RIG_SCAN_SLCT,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .priv = (void *)& nrd535_priv_caps,

    .chan_list =  {
        { 0, 199, RIG_MTYPE_MEM, NRD535_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(10), MHz(30), NRD535_MODES, -1, -1, NRD535_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(10), MHz(30), NRD535_MODES, -1, -1, NRD535_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {NRD535_MODES, 1},
        {NRD535_MODES, 10},
        {NRD535_MODES, 100},
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_FM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_ECSS, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_ECSS, kHz(2)},
        {RIG_MODE_AM | RIG_MODE_ECSS, kHz(12)},
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_FAX, kHz(2)},
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_FAX, kHz(1)},
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_FAX, kHz(6)},
        {RIG_MODE_CW, kHz(1)},
        {RIG_MODE_CW, kHz(2)},
        RIG_FLT_END,
    },
    .str_cal = NRD535_STR_CAL,

    .rig_open =  jrc_open,
    .rig_close =  jrc_close,
    .set_freq =  jrc_set_freq,
    .get_freq =  jrc_get_freq,
    .set_mode =  jrc_set_mode,
    .get_mode =  jrc_get_mode,
    .set_vfo = jrc_set_vfo,
    .set_func =  jrc_set_func,
    .get_func =  jrc_get_func,
    .set_level =  jrc_set_level,
    .get_level =  jrc_get_level,
    .set_parm =  jrc_set_parm,
    .get_parm =  jrc_get_parm,
    .get_dcd =  jrc_get_dcd,
    .set_trn =  jrc_set_trn,
    .reset =  jrc_reset,
    .set_mem =  jrc_set_mem,
    .get_mem =  jrc_get_mem,
    .set_channel =  jrc_set_chan,
    .get_channel =  jrc_get_chan,
    .vfo_op =  jrc_vfo_op,
    .scan =  jrc_scan,
    .set_powerstat =  jrc_set_powerstat,
    .get_powerstat =  jrc_get_powerstat,
    .decode_event =  jrc_decode_event,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */


