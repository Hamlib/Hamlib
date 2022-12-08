/*
 *  Hamlib Kenwood backend - TS930 description
 *  Copyright (c) 2000-2003 by Stephane Fillod
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


#include <hamlib/rig.h>
#include <bandplan.h>
#include "kenwood.h"


#define TS930_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TS930_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TS930_AM_TX_MODES RIG_MODE_AM

/* FIXME: TBC */
#define TS930_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_ANF|RIG_FUNC_LOCK)

#define TS930_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_AGC)

#define TS930_VFO (RIG_VFO_A|RIG_VFO_B)
#define TS930_ANTS (0)

static struct kenwood_priv_caps  ts930_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/*
 * ts930 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts930_caps =
{
    RIG_MODEL(RIG_MODEL_TS930),
    .model_name = "TS-930",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  10,

    .has_get_func =  TS930_FUNC_ALL,
    .has_set_func =  TS930_FUNC_ALL,
    .has_get_level =  TS930_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS930_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .preamp =   { RIG_DBLST_END, }, /* FIXME: preamp list */
    .attenuator =   { 6, 12, 18, RIG_DBLST_END, },  /* TBC */
    .max_rit =  kHz(9.99),
    .max_xit =  kHz(9.99),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    // AGC levels unknown
    .bank_qty =   0,
    .chan_desc_sz =  0,


    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM  }, /* TBC */
        { 90, 99, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(150), MHz(30), TS930_ALL_MODES, -1, -1, TS930_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TS930_OTHER_TX_MODES, W(5), W(250), TS930_VFO, TS930_ANTS),
        FRQ_RNG_HF(1, TS930_AM_TX_MODES, W(4), W(80), TS930_VFO, TS930_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(150), MHz(30), TS930_ALL_MODES, -1, -1, TS930_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(1, TS930_OTHER_TX_MODES, W(5), W(250), TS930_VFO, TS930_ANTS),
        FRQ_RNG_HF(1, TS930_AM_TX_MODES, W(4), W(80), TS930_VFO, TS930_ANTS), /* AM class */
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {      /* FIXME: TBC */
        {TS930_ALL_MODES, 50},
        {TS930_ALL_MODES, 100},
        {TS930_ALL_MODES, kHz(1)},
        {TS930_ALL_MODES, kHz(5)},
        {TS930_ALL_MODES, kHz(9)},
        {TS930_ALL_MODES, kHz(10)},
        {TS930_ALL_MODES, 12500},
        {TS930_ALL_MODES, kHz(20)},
        {TS930_ALL_MODES, kHz(25)},
        {TS930_ALL_MODES, kHz(100)},
        {TS930_ALL_MODES, MHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_AM, kHz(2.7)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts930_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open, // we don't know the ID for this rig
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
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

