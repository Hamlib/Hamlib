/*
 *  Hamlib Kenwood backend - TS690 description
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

extern int ts450_open(RIG *rig);

#define TS690_ALL_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_CW|RIG_MODE_RTTYR|RIG_MODE_CWR|RIG_MODE_SSB)
#define TS690_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_CWR)
#define TS690_AM_TX_MODES RIG_MODE_AM

/* FIXME: TBC */
#define TS690_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_AIP|RIG_FUNC_TONE)

#define TS690_LEVEL_ALL (RIG_LEVEL_STRENGTH|RIG_LEVEL_METER|RIG_LEVEL_SWR|RIG_LEVEL_ALC)

#define TS690_PARMS (RIG_PARM_ANN)  /* optional */

#define TS690_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)
#define TS690_SCAN_OPS (RIG_SCAN_VFO)

#define TS690_VFO (RIG_VFO_A|RIG_VFO_B)
#define TS690_ANTS (0)

#define TS690_CHANNEL_CAPS { \
    .freq=1,\
    .mode=1,\
    .tx_freq=1,\
    .tx_mode=1,\
    .split=1,\
    .funcs=RIG_FUNC_TONE, \
    .flags=RIG_CHFLAG_SKIP \
    }

static struct kenwood_priv_caps  ts690_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/*
 * ts690 rig capabilities.
 * written from specs:
 *  http://www.qsl.net/sm7vhs/radio/kenwood/ts690/specs.htm
 *
 * TODO: protocol to be checked with manual!
 *  - how to set_split in vfo mode?
 *  - ...
 */
const struct rig_caps ts690s_caps =
{
    RIG_MODEL(RIG_MODEL_TS690S),
    .model_name = "TS-690S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  100,
    .timeout =  1000,
    .retry =  10,

    .has_get_func =  TS690_FUNC_ALL,
    .has_set_func =  TS690_FUNC_ALL,
    .has_get_level =  TS690_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS690_LEVEL_ALL),
    .has_get_parm =  TS690_PARMS,
    .has_set_parm =  RIG_LEVEL_SET(TS690_PARMS),    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  kHz(2.2),
    .max_xit =  kHz(2.2),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_SLOW },
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .vfo_ops = TS690_VFO_OPS,
    .scan_ops =  TS690_SCAN_OPS,

    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM, TS690_CHANNEL_CAPS },  /* TBC */
        { 90, 99, RIG_MTYPE_EDGE, TS690_CHANNEL_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), TS690_ALL_MODES, -1, -1, TS690_VFO},
        {MHz(50), MHz(54), TS690_ALL_MODES, -1, -1, TS690_VFO},
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TS690_OTHER_TX_MODES, W(5), W(100), TS690_VFO, TS690_ANTS),
        FRQ_RNG_HF(1, TS690_AM_TX_MODES, W(4), W(40), TS690_VFO, TS690_ANTS), /* AM class */
        FRQ_RNG_6m(1, TS690_OTHER_TX_MODES, W(2.5), W(50), TS690_VFO, TS690_ANTS),
        FRQ_RNG_6m(1, TS690_AM_TX_MODES, W(2), W(20), TS690_VFO, TS690_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(500), MHz(30), TS690_ALL_MODES, -1, -1, TS690_VFO},
        {MHz(50), MHz(54), TS690_ALL_MODES, -1, -1, TS690_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TS690_OTHER_TX_MODES, W(5), W(100), TS690_VFO, TS690_ANTS),
        FRQ_RNG_HF(2, TS690_AM_TX_MODES, W(4), W(40), TS690_VFO, TS690_ANTS), /* AM class */
        FRQ_RNG_6m(2, TS690_OTHER_TX_MODES, W(2.5), W(50), TS690_VFO, TS690_ANTS),
        FRQ_RNG_6m(2, TS690_AM_TX_MODES, W(2), W(20), TS690_VFO, TS690_ANTS), /* AM class */
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {      /* FIXME: TBC */
        {TS690_ALL_MODES, 1},
        {TS690_ALL_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR | RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR | RIG_MODE_AM, Hz(500)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR | RIG_MODE_AM, kHz(12)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR, kHz(6)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts690_priv_caps,

    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = ts450_open,
    .rig_close = kenwood_close,
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
    .set_split_vfo =  kenwood_set_split_vfo,
    .get_split_vfo =  kenwood_get_split_vfo_if,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  kenwood_set_func,
    .get_func =  kenwood_get_func,
    .set_level =  kenwood_set_level,
    .get_level =  kenwood_get_level,
    .set_ext_parm = kenwood_set_ext_parm,
    .get_ext_parm = kenwood_get_ext_parm,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem_if,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .reset = kenwood_reset,
    .scan =  kenwood_scan,
    .get_channel = kenwood_get_channel,
    .set_channel = kenwood_set_channel,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

