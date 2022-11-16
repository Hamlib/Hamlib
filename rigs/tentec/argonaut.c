/*
 *  Hamlib TenTenc backend - TT-516 PC-Radio description
 *  Copyright (c) 2003-2008 by Stephane Fillod
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
#include "tentec2.h"
#include "bandplan.h"


#define TT516_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB)
#define TT516_RXMODES (TT516_MODES|RIG_MODE_AM)

#define TT516_FUNCS (RIG_FUNC_NONE)

#define TT516_LEVELS (RIG_LEVEL_RAWSTR|/* RIG_LEVEL_NB| */ \
                RIG_LEVEL_SQL|/*RIG_LEVEL_PBT|*/ \
                RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD| \
                RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT516_ANTS RIG_ANT_1

#define TT516_VFO (RIG_VFO_A|RIG_VFO_B)

// Taken from RX320_STR_CAL -- unknown if accurate for TT516
#define TT516_STR_CAL { 17, { \
                {      0, -60 }, \
                {     10, -50 }, \
                {     20, -40 }, \
                {     30, -30 }, \
                {     40, -20 }, \
                {     50, -15 }, \
                {    100, -10 }, \
                {    200, -5 }, \
                {    225, -3 }, \
                {    256,  0 }, \
                {    512,  1 }, \
                {    768,  3}, \
                {   1024,  4 }, \
                {   1280,  5 }, \
                {   2560,  10 }, \
                {   5120,  20 }, \
                {  10000,  30 }, \
        } }



/*
 * tt516 receiver capabilities.
 *
 * protocol is documented at
 *      http://www.rfsquared.com/
 *
 */
const struct rig_caps tt516_caps =
{
    RIG_MODEL(RIG_MODEL_TT516),
    .model_name = "TT-516 Argonaut V",
    .mfg_name =  "Ten-Tec",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  2000,
    .retry =  3,

    .has_get_func =  TT516_FUNCS,
    .has_set_func =  TT516_FUNCS,
    .has_get_level =  TT516_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(TT516_LEVELS),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =  {
        [LVL_RAWSTR]        = { .min = { .i = 0 },     .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { 15, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1, 100, RIG_MTYPE_MEM, TT_MEM_CAP },
    },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), TT516_RXMODES, -1, -1, TT516_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TT516_MODES, W(1), W(20), TT516_VFO, TT516_ANTS),
        FRQ_RNG_HF(1, RIG_MODE_AM, W(1), W(5), TT516_VFO, TT516_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(500), MHz(30), TT516_RXMODES, -1, -1, TT516_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TT516_MODES, W(1), W(20), TT516_VFO, TT516_ANTS),
        FRQ_RNG_HF(2, RIG_MODE_AM, W(1), W(5), TT516_VFO, TT516_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {RIG_MODE_SSB | RIG_MODE_CW, 10},
        /* {RIG_MODE_SSB|RIG_MODE_CW,kHz(1)}, */
        {RIG_MODE_AM, 100},
        /* {RIG_MODE_AM,kHz(5)}, */
        {RIG_MODE_FM, kHz(2.5)},
        /* {RIG_MODE_FM,kHz(5)}, */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* FIXME: add increments -> 34 filters? */
        {RIG_MODE_CW | RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_CW | RIG_MODE_SSB, 200},
        {RIG_MODE_CW | RIG_MODE_SSB, 0}, /*  Filters are 200 Hz to 1000 Hz in 50 Hz steps, 1000 to 2800 Hz in 100 Hz steps */
        {RIG_MODE_AM, kHz(4)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },
    .str_cal = TT516_STR_CAL,
    .priv = (void *)NULL,

    .set_freq =  tentec2_set_freq,
    .get_freq =  tentec2_get_freq,
    .set_vfo =  tentec2_set_vfo,
    .get_vfo =  tentec2_get_vfo,
    .set_mode =  tentec2_set_mode,
    .get_mode =  tentec2_get_mode,
    .set_split_vfo =  tentec2_set_split_vfo,
    .get_split_vfo =  tentec2_get_split_vfo,
    .set_ptt =  tentec2_set_ptt,
    .get_ptt =  tentec2_get_ptt,
    .reset =  tentec2_reset,
    .get_info =  tentec2_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */


