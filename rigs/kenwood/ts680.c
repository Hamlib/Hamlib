/*
 *  Hamlib Kenwood backend - TS680 description
 *  Copyright (c) 2000-2008 by Stephane Fillod
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

#include "hamlib/rig.h"
#include "bandplan.h"
#include "kenwood.h"


#define TS680_ALL_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_CWR)
#define TS680_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_CWR)
#define TS680_AM_TX_MODES RIG_MODE_AM
#define TS680_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define TS680_ANTS (0)

/*
 * vfo defines
*/
#define VFO_A   '0'
#define VFO_B   '1'
#define VFO_MEM '2'

static struct kenwood_priv_caps  ts680_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

static int ts680_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[16];
    char vfo_function;

    switch (vfo)
    {
    case RIG_VFO_VFO:
    case RIG_VFO_A: vfo_function = VFO_A; break;

    case RIG_VFO_B: vfo_function = VFO_B; break;

    case RIG_VFO_MEM: vfo_function = VFO_MEM; break;

    case RIG_VFO_CURR: return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "ts680_set_vfo: unsupported VFO %s\n",
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "FN%c",
             vfo_function); /* The 680 and 140 need this to set the VFO on the radio */
    return kenwood_transaction(rig, cmdbuf, NULL, 0);
}

/*
 * ts680 rig capabilities.
 *  GW0VNR 09042006
 */

const struct rig_caps ts680s_caps =
{
    RIG_MODEL(RIG_MODEL_TS680S),
    .model_name = "TS-680S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  9600,   /* Rig only capable of 4800 baud from factory and 9k6 with jumper change */
    .serial_data_bits =  8,
    .serial_stop_bits =  2,     /* TWO stop bits. This is correct. */
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  10,

    .has_get_func =  RIG_FUNC_LOCK,
    .has_set_func =  RIG_FUNC_LOCK,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* No PARAMS controllable */
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .preamp =   { RIG_DBLST_END, }, /* Not controllable */
    .attenuator =   { RIG_DBLST_END, }, /* Not controllable */
    .max_rit =  kHz(1.2),
    .max_xit =  kHz(1.2),
    .max_ifshift =  Hz(0), /* Not controllable */
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,


    .chan_list =  {
        {  0, 19, RIG_MTYPE_MEM  },
        { 20, 30, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(50), kHz(34999), TS680_ALL_MODES, -1, -1, TS680_VFO},
        {MHz(45), kHz(59999), TS680_ALL_MODES, -1, -1, TS680_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TS680_OTHER_TX_MODES, W(5), W(100), TS680_VFO, TS680_ANTS),
        FRQ_RNG_HF(1, TS680_AM_TX_MODES, W(2), W(40), TS680_VFO, TS680_ANTS),
        FRQ_RNG_6m(1, TS680_OTHER_TX_MODES, W(1), W(10), TS680_VFO, TS680_ANTS),
        FRQ_RNG_6m(1, TS680_AM_TX_MODES, W(1), W(4), TS680_VFO, TS680_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(50), kHz(34999), TS680_ALL_MODES, -1, -1, TS680_VFO},
        {MHz(45), kHz(59999), TS680_ALL_MODES, -1, -1, TS680_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TS680_OTHER_TX_MODES, W(5), W(100), TS680_VFO, TS680_ANTS),
        FRQ_RNG_HF(2, TS680_AM_TX_MODES, W(2), W(40), TS680_VFO, TS680_ANTS),
        FRQ_RNG_6m(2, TS680_OTHER_TX_MODES, W(1), W(10), TS680_VFO, TS680_ANTS),
        FRQ_RNG_6m(2, TS680_AM_TX_MODES, W(1), W(4), TS680_VFO, TS680_ANTS),
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {      /* FIXME: Done */
        {TS680_ALL_MODES, 10},
        {TS680_ALL_MODES, 100},
        {TS680_ALL_MODES, kHz(1)},
        {TS680_ALL_MODES, kHz(5)},
        {TS680_ALL_MODES, kHz(9)},
        {TS680_ALL_MODES, kHz(10)},
        {TS680_ALL_MODES, 12500},
        {TS680_ALL_MODES, kHz(20)},
        {TS680_ALL_MODES, kHz(25)},
        {TS680_ALL_MODES, kHz(100)},
        {TS680_ALL_MODES, MHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_AM, kHz(2.2)},
        {RIG_MODE_CWR, 600},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts680_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open, // we don't know the ID for this rig
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_mode =  kenwood_set_mode,
    .get_mode = kenwood_get_mode_if,
    .set_vfo = ts680_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_ptt =  kenwood_set_ptt,
    .set_func = kenwood_set_func,
    .get_func =  kenwood_get_func,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem = kenwood_get_mem_if,
    .reset =  kenwood_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

