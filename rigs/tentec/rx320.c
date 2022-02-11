/*
 *  Hamlib TenTenc backend - RX-320 PC-Radio description
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

#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "tentec.h"


#define RX320_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB)

/* TODO: LINEOUT */
#define RX320_LEVELS (RIG_LEVEL_AGC|RIG_LEVEL_AF|RIG_LEVEL_IF|RIG_LEVEL_RAWSTR|RIG_LEVEL_CWPITCH)

#define RX320_VFO (RIG_VFO_A)

/*
 * Modified 11/18/2008, Josh Rovero, KK1D
 * Calibration via comparison with JRC NRD-525.
 * Highy non-linear....
 */
#define RX320_STR_CAL { 17, { \
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
 * rx320 receiver capabilities.
 *
 * protocol is documented at
 *      http://www.tentec.com/rx320prg.zip
 *
 * TODO:
 */
const struct rig_caps rx320_caps =
{
    RIG_MODEL(RIG_MODEL_RX320),
    .model_name = "RX-320",
    .mfg_name =  "Ten-Tec",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_PCRECEIVER,
    .ptt_type =  RIG_PTT_NONE,
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
    .timeout =  200,
    .retry =  3,

    /*
     * Added S-meter read support, Josh Rovero KK1D
     * Only get_level is for RIG_LEVEL_RAWSTR
     */
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RX320_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(RX320_LEVELS),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 10000 } },
        [LVL_AF] = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 1.0 / 64 } },
        [LVL_IF] = { .min = { .i = -2000 }, .max = { .i = 2000 }, .step = { .i = 10} },
        [LVL_CWPITCH] = { .min = { .i = 0}, .max = { .i = 2000 }, .step = { .i = 100} }
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(2),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30), RX320_MODES, -1, -1, RX320_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), RX320_MODES, -1, -1, RX320_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {RX320_MODES, 10}, /* FIXME: add other ts */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_CW, 300},
        {RIG_MODE_CW, 450},
        {RIG_MODE_CW, 600},
        {RIG_MODE_CW, 750},
        {RIG_MODE_CW, 900},
        {RIG_MODE_CW, kHz(1.2)},
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(1.5)},
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(2.1)},
        {RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_CW, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_CW, kHz(3.0)},
        {RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_CW, kHz(4.2)},
        {RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_CW, kHz(5.1)},
        {RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_CW, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_CW, kHz(8)},
        RIG_FLT_END,
    },
    .str_cal = RX320_STR_CAL,

    .rig_init =  tentec_init,
    .rig_cleanup =  tentec_cleanup,
    .set_freq =  tentec_set_freq,
    .get_freq =  tentec_get_freq,
    .set_mode =  tentec_set_mode,
    .get_mode =  tentec_get_mode,
    .set_level =  tentec_set_level,
    .get_level =  tentec_get_level,
    .get_info =  tentec_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

