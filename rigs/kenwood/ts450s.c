/*
 *  Hamlib Kenwood backend - TS450S description
 *  Copyright (c) 2000-2011 by Stephane Fillod
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

#include <stdio.h>

#include <hamlib/rig.h>
#include "kenwood.h"

#include "bandplan.h"

#define TS450S_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS450S_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS450S_AM_TX_MODES RIG_MODE_AM
#define TS450S_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_AIP|RIG_FUNC_TONE)

#define TS450S_LEVEL_ALL (RIG_LEVEL_STRENGTH|RIG_LEVEL_CWPITCH|RIG_LEVEL_METER|RIG_LEVEL_SWR|RIG_LEVEL_ALC)
#define TS450S_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define TS450S_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)
#define TS450S_SCAN_OPS (RIG_SCAN_VFO)

#define TS450S_CHANNEL_CAPS { \
    .freq=1,\
    .mode=1,\
    .tx_freq=1,\
    .tx_mode=1,\
    .split=1,\
    .funcs=RIG_FUNC_TONE, \
    .flags=RIG_CHFLAG_SKIP \
    }

static struct kenwood_priv_caps ts450_priv_caps =
{
    .cmdtrm     = EOM_KEN,
};

static const struct confparams ts450_ext_parms[] =
{
    {
        TOK_FINE, "fine", "Fine", "Fine step mode",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_VOICE, "voice", "Voice", "Voice recall",
        NULL, RIG_CONF_BUTTON, { }
    },
    {
        TOK_XIT, "xit", "XIT", "XIT",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_RIT, "rit", "RIT", "RIT",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    { RIG_CONF_END, NULL, }
};

int ts450_open(RIG *rig)
{
    int err;
    int maxtries;

    err = kenwood_open(rig);

    if (err != RIG_OK)
    {
        return err;
    }

    maxtries = rig->state.rigport.retry;
    /* no retry for this command that may be missing */
    rig->state.rigport.retry = 0;

    err = kenwood_simple_transaction(rig, "TO", 3);

    if (err != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: tone unit not detected\n", __func__);
        rig->state.has_set_func &= ~RIG_FUNC_TONE;
        rig->state.has_get_func &= ~RIG_FUNC_TONE;
    }

    rig->state.rigport.retry = maxtries;

    return RIG_OK;
}

/*
 * ts450s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * TODO: protocol to be checked with manual (identical to TS690)
 *  - get_channel/set_channel: MR/MW
 *  - how to set_split in vfo mode?
 *  - ...
 *
 * specs: http://www.qsl.net/sm7vhs/radio/kenwood/ts450/specs.htm
 * infos comes from http://www.cnham.com/ts450/ts_450_ex_control.pdf
 */
const struct rig_caps ts450s_caps =
{
    RIG_MODEL(RIG_MODEL_TS450S),
    .model_name = "TS-450S",
    .mfg_name   = "Kenwood",
    .version    = BACKEND_VER ".0",
    .copyright  = "LGPL",
    .status     = RIG_STATUS_STABLE,
    .rig_type   = RIG_TYPE_TRANSCEIVER,
    .ptt_type   = RIG_PTT_RIG,
    .dcd_type   = RIG_DCD_RIG,
    .port_type  = RIG_PORT_SERIAL,

    .serial_rate_min    = 1200,
    .serial_rate_max    = 4800,
    .serial_data_bits   = 8,
    .serial_stop_bits   = 2,
    .serial_parity      = RIG_PARITY_NONE,
    .serial_handshake   = RIG_HANDSHAKE_HARDWARE,
    .write_delay        = 0,
    .post_write_delay   = 10,
    .timeout        = 1000,
    .retry          = 10,

    .has_get_func       = TS450S_FUNC_ALL,
    .has_set_func       = TS450S_FUNC_ALL,
    .has_get_level      = TS450S_LEVEL_ALL | RIG_LEVEL_RFPOWER,
    .has_set_level      = RIG_LEVEL_SET(TS450S_LEVEL_ALL),
    .has_get_parm       = 0,
    .has_set_parm       = 0,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran      = {},
    .extparms       = ts450_ext_parms,
    .ctcss_list     = NULL, /* hw dip-switch */
    .dcs_list       = NULL,
    .preamp         = { RIG_DBLST_END, },
    .attenuator     = { RIG_DBLST_END, }, /* can't be controlled */
    .max_rit        = Hz(9999),
    .max_xit        = Hz(9999),
    .max_ifshift        = Hz(0),
    .targetable_vfo     = RIG_TARGETABLE_FREQ,
    .transceive     = RIG_TRN_RIG,
    .bank_qty       = 0,
    .chan_desc_sz       = 0,
    .vfo_ops        = TS450S_VFO_OPS,
    .scan_ops       = TS450S_SCAN_OPS,

    .chan_list = {
        { 0, 89, RIG_MTYPE_MEM, TS450S_CHANNEL_CAPS },  /* TBC */
        { 90, 99, RIG_MTYPE_EDGE, TS450S_CHANNEL_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        { kHz(500), MHz(30), TS450S_ALL_MODES, -1, -1, TS450S_VFO },
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list1 = {
        FRQ_RNG_HF(1, TS450S_OTHER_TX_MODES, W(5), W(100), TS450S_VFO, 0),
        FRQ_RNG_HF(1, TS450S_AM_TX_MODES, W(2), W(40), TS450S_VFO, 0), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 = {
        {kHz(500), MHz(30), TS450S_ALL_MODES, -1, -1, TS450S_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 = {
        {kHz(1800), MHz(2) - 1, TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO}, /* 40W class */
        {kHz(3500), MHz(4) - 1, TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {kHz(3500), MHz(4) - 1, TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        {MHz(7), kHz(7300), TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {MHz(7), kHz(7300), TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        {kHz(10100), kHz(10150), TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {kHz(10100), kHz(10150), TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        {MHz(14), kHz(14350), TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {MHz(14), kHz(14350), TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        {kHz(18068), kHz(18168), TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {kHz(18068), kHz(18168), TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        {MHz(21), kHz(21450), TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {MHz(21), kHz(21450), TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        {kHz(24890), kHz(24990), TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {kHz(24890), kHz(24990), TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        {MHz(28), kHz(29700), TS450S_OTHER_TX_MODES, 5000, 100000, TS450S_VFO},
        {MHz(28), kHz(29700), TS450S_AM_TX_MODES, 2000, 40000, TS450S_VFO},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps = {
        { TS450S_ALL_MODES, 1 },
        { TS450S_ALL_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_FM, kHz(12) },
        {RIG_MODE_FM | RIG_MODE_AM, kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR | RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR | RIG_MODE_AM, Hz(500)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR | RIG_MODE_AM, kHz(12)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_CWR | RIG_MODE_RTTYR, kHz(6)},
        RIG_FLT_END,
    },

    .priv = (void *)& ts450_priv_caps,

    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = ts450_open,
    .rig_close = kenwood_close,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,
    .get_xit = kenwood_get_xit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode_if,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
    .set_level = kenwood_set_level,
    .get_level = kenwood_get_level,
    .set_ext_parm = kenwood_set_ext_parm,
    .get_ext_parm = kenwood_get_ext_parm,
    .vfo_op = kenwood_vfo_op,
    .set_mem = kenwood_set_mem,
    .get_mem = kenwood_get_mem_if,
    .set_trn = kenwood_set_trn,
    .get_trn = kenwood_get_trn,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .reset = kenwood_reset,
    .scan = kenwood_scan,
    .get_channel = kenwood_get_channel,
    .set_channel = kenwood_set_channel,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
