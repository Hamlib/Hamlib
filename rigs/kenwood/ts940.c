/*
 *  Hamlib Kenwood backend - TS940 description
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


#include <hamlib/rig.h>
#include "bandplan.h"
#include "kenwood.h"


#define TS940_ALL_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_CW|RIG_MODE_SSB)
#define TS940_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS940_AM_TX_MODES RIG_MODE_AM

#define TS940_FUNC_ALL RIG_FUNC_LOCK

#define TS940_LEVEL_ALL RIG_LEVEL_NONE

#define TS940_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define TS940_ANTS (0)

#define TS940_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)
#define TS940_SCAN_OPS (RIG_SCAN_VFO)

/* memory capabilities */
#define TS940_MEM_CAP { \
    .freq = 1,      \
    .mode = 1,      \
    .tx_freq=1,     \
    .tx_mode=1,     \
}

static rmode_t ts940_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_NONE,
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [4] = RIG_MODE_FM,
    [5] = RIG_MODE_AM,
    [6] = RIG_MODE_NONE,
    [7] = RIG_MODE_NONE,
    [8] = RIG_MODE_NONE,
    [9] = RIG_MODE_NONE
};

static struct kenwood_priv_caps  ts940_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .mode_table = ts940_mode_table
};

/*
 * ts940 rig capabilities.
 * written from specs:
 *  http://www.qsl.net/sm7vhs/radio/kenwood/ts940/specs.htm
 *
 * TODO: protocol to be check with manual!
 */
const struct rig_caps ts940_caps =
{
    RIG_MODEL(RIG_MODEL_TS940),
    .model_name = "TS-940S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".0",
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
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .timeout =  600,
    .retry =  10,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  TS940_FUNC_ALL,
    .has_get_level =  TS940_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS940_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .preamp =   { RIG_DBLST_END, }, /* FIXME: preamp list */
    .attenuator =   { RIG_DBLST_END, }, /* TBC */
    .max_rit =  kHz(9.99),
    .max_xit =  kHz(9.99),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .vfo_ops =  TS940_VFO_OPS,
    .scan_ops =  TS940_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    // Unknown AGC levels
    .bank_qty =   0,
    .chan_desc_sz =  0,

    /* four banks of 10 memories */
    .chan_list =  {
        {  0, 0, RIG_MTYPE_EDGE, TS940_MEM_CAP},    /* TBC */
        {  1, 8, RIG_MTYPE_MEM, TS940_MEM_CAP}, /* TBC */
        {  9, 9, RIG_MTYPE_EDGE, TS940_MEM_CAP},    /* TBC */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), TS940_ALL_MODES, -1, -1, TS940_VFO},
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TS940_OTHER_TX_MODES, W(5), W(250), TS940_VFO, TS940_ANTS),
        FRQ_RNG_HF(1, TS940_AM_TX_MODES, W(4), W(140), TS940_VFO, TS940_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(500), MHz(30), TS940_ALL_MODES, -1, -1, TS940_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TS940_OTHER_TX_MODES, W(5), W(250), TS940_VFO, TS940_ANTS),
        FRQ_RNG_HF(2, TS940_AM_TX_MODES, W(4), W(140), TS940_VFO, TS940_ANTS), /* AM class */
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {      /* FIXME: TBC */
        {TS940_ALL_MODES, 10},
        {TS940_ALL_MODES, kHz(100)},
        {TS940_ALL_MODES, MHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts940_priv_caps,

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
    .set_split_vfo =  kenwood_set_split,
    .get_split_vfo =  kenwood_get_split_vfo_if,
    .set_ptt =  kenwood_set_ptt,
    .get_ptt =  kenwood_get_ptt,
    .set_func =  kenwood_set_func,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem_if,
    .set_trn =  kenwood_set_trn,
    .scan =  kenwood_scan,
    .set_channel = kenwood_set_channel,
    .get_channel = kenwood_get_channel,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

