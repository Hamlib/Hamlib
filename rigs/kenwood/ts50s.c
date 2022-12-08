/*
 *  Hamlib Kenwood backend - TS50 description
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
#include "kenwood.h"


#define TS50_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define TS50_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define TS50_AM_TX_MODES RIG_MODE_AM

#define TS50_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_TSQL|RIG_FUNC_TONE|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_LOCK|RIG_FUNC_BC)

#define TS50_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN)

#define TS50_VFO (RIG_VFO_A|RIG_VFO_B)
#define TS50_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

static struct kenwood_priv_caps  ts50_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/*
 * ts50 rig capabilities.
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts50s_caps =
{
    RIG_MODEL(RIG_MODEL_TS50),
    .model_name = "TS-50S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  10,

    .has_get_func =  TS50_FUNC_ALL,
    .has_set_func =  TS50_FUNC_ALL,
    .has_get_level =  TS50_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS50_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, }, /* FIXME: preamp list */
    .attenuator =   { 18, RIG_DBLST_END, },
    .max_rit =  kHz(1.1),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  TS50_VFO_OP,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,


    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM  },
        { 90, 99, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },
    .rx_range_list1 =  {
        {kHz(500), MHz(30), TS50_ALL_MODES, -1, -1, TS50_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {kHz(1810), kHz(1850) - 1, TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS50_AM_TX_MODES, 5000, 25000, TS50_VFO}, /* 25W class */
        {kHz(3500), kHz(3800) - 1, TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(3500), kHz(3800) - 1, TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(7), kHz(7100), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(7), kHz(7100), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {kHz(10100), kHz(10150), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(10100), kHz(10150), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(14), kHz(14350), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(14), kHz(14350), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {kHz(18068), kHz(18168), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(18068), kHz(18168), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(21), kHz(21450), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(21), kHz(21450), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {kHz(24890), kHz(24990), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(24890), kHz(24990), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(28), kHz(29700), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(28), kHz(29700), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(500), MHz(30), TS50_ALL_MODES, -1, -1, TS50_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS50_AM_TX_MODES, 5000, 25000, TS50_VFO}, /* 25W class */
        {kHz(3500), MHz(4) - 1, TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(3500), MHz(4) - 1, TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(7), kHz(7300), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(7), kHz(7300), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {kHz(10100), kHz(10150), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(10100), kHz(10150), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(14), kHz(14350), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(14), kHz(14350), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {kHz(18068), kHz(18168), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(18068), kHz(18168), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(21), kHz(21450), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(21), kHz(21450), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {kHz(24890), kHz(24990), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {kHz(24890), kHz(24990), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        {MHz(28), kHz(29700), TS50_OTHER_TX_MODES, 5000, 100000, TS50_VFO},
        {MHz(28), kHz(29700), TS50_AM_TX_MODES, 5000, 25000, TS50_VFO},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS50_ALL_MODES, 50},
        {TS50_ALL_MODES, 100},
        {TS50_ALL_MODES, kHz(1)},
        {TS50_ALL_MODES, kHz(5)},
        {TS50_ALL_MODES, kHz(9)},
        {TS50_ALL_MODES, kHz(10)},
        {TS50_ALL_MODES, 12500},
        {TS50_ALL_MODES, kHz(20)},
        {TS50_ALL_MODES, kHz(25)},
        {TS50_ALL_MODES, kHz(100)},
        {TS50_ALL_MODES, MHz(1)},
        {TS50_ALL_MODES, 0},   /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(2.2)},
        {RIG_MODE_AM, kHz(5)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts50_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode_if,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  kenwood_set_func,
    .get_func =  kenwood_get_func,
    .set_level =  kenwood_set_level,
    .get_level =  kenwood_get_level,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .reset =  kenwood_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

