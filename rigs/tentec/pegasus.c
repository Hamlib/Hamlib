/*
 *  Hamlib TenTenc backend - TT-550 PC-Radio description
 *  Copyright (c) 2002-2004 by Stephane Fillod
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
#include "tt550.h"


#define TT550_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_AM)
#define TT550_RXMODES (TT550_MODES)

#define TT550_FUNCS (RIG_FUNC_VOX|RIG_FUNC_ANF|RIG_FUNC_TUNER| \
                RIG_FUNC_NR|RIG_FUNC_VOX)


#define TT550_LEVELS (RIG_LEVEL_AGC|RIG_LEVEL_AF|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH| \
                                RIG_LEVEL_RF|RIG_LEVEL_COMP|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SQL| \
                                RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD| \
                                RIG_LEVEL_SWR|RIG_LEVEL_ATT|RIG_LEVEL_NR|RIG_LEVEL_IF| \
                                RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX)



#define TT550_VFO (RIG_VFO_A )
#define TT550_VFO_OPS (RIG_OP_TUNE)
/*
 * a bit coarse, but I don't have a TT550, and the manual is not
 * verbose on the subject. Please test it and report! --SF
 */
#define TT550_STR_CAL { 2, { \
        {      0, -60 }, \
        {  10000,  20 }, \
    } }

/*
 * tt550 receiver capabilities.
 *
 * protocol is documented at
 *      http://www.tentec.com/550/550prg2.pdf
 *
 * TODO:
 */
const struct rig_caps tt550_caps =
{
    RIG_MODEL(RIG_MODEL_TT550),
    .model_name = "TT-550",
    .mfg_name = "Ten-Tec",
    .version = "20190817.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_COMPUTER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 57600,
    .serial_rate_max = 57600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_HARDWARE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 100,
    .retry = 4,

    .has_get_func = TT550_FUNCS,
    .has_set_func = TT550_FUNCS,
    .has_get_level = TT550_LEVELS,
    .has_set_level = RIG_LEVEL_SET(TT550_LEVELS),
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = {.min = {.i = 0}, .max = {.i = 65535}},
    },
    .parm_gran = {},
    .ctcss_list = NULL,
    .dcs_list = NULL,
    .preamp = {RIG_DBLST_END},
    .attenuator = {20, RIG_DBLST_END},
    .max_rit = Hz(10000),
    .max_xit = Hz(10000),
    .max_ifshift = Hz(2000),
    .targetable_vfo = RIG_TARGETABLE_ALL,
    .vfo_ops = TT550_VFO_OPS,
    .transceive = RIG_TRN_RIG,
    .bank_qty = 0,
    .chan_desc_sz = 0,

    .chan_list = {
        RIG_CHAN_END,
    },

    .rx_range_list1 = {RIG_FRNG_END,},    /* FIXME: enter region 1 setting */
    .tx_range_list1 = {RIG_FRNG_END,},

    .rx_range_list2 = {
        {kHz(100), MHz(30), TT550_RXMODES, -1, -1, TT550_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        {kHz(1800), MHz(2) - 1, TT550_MODES, 5000, 100000, TT550_VFO},
        {kHz(3500), MHz(4) - 1, TT550_MODES, 5000, 100000, TT550_VFO},
        {kHz(5330), kHz(5407) - 1, RIG_MODE_USB, 5000, 50000, TT550_VFO},
        {MHz(7), kHz(7300), TT550_MODES, 5000, 100000, TT550_VFO},
        {kHz(10100), kHz(10150), TT550_MODES, 5000, 100000, TT550_VFO},
        {MHz(14), kHz(14350), TT550_MODES, 5000, 100000, TT550_VFO},
        {kHz(18068), kHz(18168), TT550_MODES, 5000, 100000, TT550_VFO},
        {MHz(21), kHz(21450), TT550_MODES, 5000, 100000, TT550_VFO},
        {kHz(24890), kHz(24990), TT550_MODES, 5000, 100000, TT550_VFO},
        {MHz(28), kHz(29700), TT550_MODES, 5000, 100000, TT550_VFO},
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {TT550_RXMODES, RIG_TS_ANY},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_CW, Hz(450)},
        {RIG_MODE_CW, Hz(300)},
        {RIG_MODE_CW, Hz(750)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_FM, Hz(4200)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(8)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(2400)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(2700)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(2100)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(5700)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(5400)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(5100)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(4800)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(4500)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(4200)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(3900)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(3600)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(3300)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(2850)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(8000)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(2550)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(2400)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(2250)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(6000)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(1950)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(1800)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(1650)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(1500)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(1350)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(1200)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(1050)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(900)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(750)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(675)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(600)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(525)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(450)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(375)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(330)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_CW, Hz(300)},
        RIG_FLT_END,
    },
    .str_cal = TT550_STR_CAL,

    .rig_init = tt550_init,
    .rig_cleanup = tt550_cleanup,
    .rig_open = tt550_trx_open,
    .reset = tt550_reset,
    .set_freq = tt550_set_freq,
    .get_freq = tt550_get_freq,
    .set_mode = tt550_set_mode,
    .get_mode = tt550_get_mode,
    .set_func = tt550_set_func,
    .get_func = tt550_get_func,
    .set_level = tt550_set_level,
    .get_level = tt550_get_level,
    .get_info = tt550_get_info,
    .set_ptt = tt550_set_ptt,
    .get_ptt = tt550_get_ptt,
    .set_split_freq = tt550_set_tx_freq,
    .get_split_freq = tt550_get_tx_freq,
    .set_split_mode = tt550_set_tx_mode,
    .get_split_mode = tt550_get_tx_mode,
    .set_split_vfo = tt550_set_split_vfo,
    .get_split_vfo = tt550_get_split_vfo,
    .decode_event = tt550_decode_event,
    .set_ts = tt550_set_tuning_step,
    .get_ts = tt550_get_tuning_step,
    .vfo_op = tt550_vfo_op,
    .set_rit =  tt550_set_rit,
    .get_rit =  tt550_get_rit,
    .set_xit =  tt550_set_xit,
    .get_xit =  tt550_get_xit,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */
