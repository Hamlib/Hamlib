/*
 *  Hamlib Kenwood backend - TS440 description
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


#define TS440_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS440_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS440_AM_TX_MODES RIG_MODE_AM

#define TS440_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT)

#define TS440_LEVEL_ALL RIG_LEVEL_NONE

#define TS440_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS440_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)
#define TS440_SCAN_OPS (RIG_SCAN_VFO)

static struct kenwood_priv_caps  ts440_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .if_len =  37,
};

/*
 * ts440 rig capabilities.
 *
 * part of infos comes from .http = //www.n7uic.net/radio/kenwood/ts440/specs.htm
 * .http = //public.srce.hr/9A1CDD/mods/kenwood/knwdif.mod
 * .http = //www.ifrance.fr/clucas/modposte/ts440/mod440.htm
 *
 */
const struct rig_caps ts440_caps =
{
    RIG_MODEL(RIG_MODEL_TS440),
    .model_name = "TS-440S",
    .mfg_name =  "Kenwood",
    .version =  IC10_VER ".3",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  20,
    .timeout =  500,
    .retry =  10,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  TS440_FUNC_ALL,
    .has_get_level =  TS440_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS440_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(1270),
    .max_xit =  Hz(1270),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .vfo_ops =  TS440_VFO_OPS,
    .scan_ops =  TS440_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,


    .chan_list =  { {   0,  99, RIG_MTYPE_MEM, {IC10_CHANNEL_CAPS} },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), TS440_ALL_MODES, -1, -1, TS440_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {kHz(1800), MHz(2) - 1, TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {kHz(3500), MHz(4) - 1, TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {kHz(3500), MHz(4) - 1, TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {MHz(7), kHz(7300), TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {MHz(7), kHz(7300), TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {kHz(10100), kHz(10150), TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {kHz(10100), kHz(10150), TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {MHz(14), kHz(14350), TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {MHz(14), kHz(14350), TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {kHz(18068), kHz(18168), TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {kHz(18068), kHz(18168), TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {MHz(21), kHz(21450), TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {MHz(21), kHz(21450), TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {kHz(24890), kHz(24990), TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {kHz(24890), kHz(24990), TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        {MHz(28), kHz(29700), TS440_OTHER_TX_MODES, 5000, W(200), TS440_VFO},
        {MHz(28), kHz(29700), TS440_AM_TX_MODES, 2000, W(110), TS440_VFO},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS440_ALL_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.2)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts440_priv_caps,

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
    .set_mode =  ic10_set_mode,
    .get_mode =  ic10_get_mode,
    .set_vfo =  ic10_set_vfo,
    .get_vfo =  ic10_get_vfo,
    .set_split_freq = ic10_set_split_freq,
    .get_split_freq = ic10_get_split_freq,
    .set_split_vfo =  ic10_set_split_vfo,
    .get_split_vfo =  ic10_get_split_vfo,
    .set_ptt =  ic10_set_ptt,
    .get_ptt =  ic10_get_ptt,
    .set_func =  kenwood_set_func,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  ic10_get_mem,
    .set_trn =  kenwood_set_trn,
    .scan =  kenwood_scan,
    .set_channel = ic10_set_channel,
    .get_channel = ic10_get_channel,
    .decode_event = ic10_decode_event,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

