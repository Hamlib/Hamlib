/*
 *  Hamlib Kenwood backend - TS-711 description
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


#define TS711_ALL_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

/* func and levels to be checked */
#define TS711_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_LOCK|RIG_FUNC_MUTE)

#define TS711_LEVEL_ALL (RIG_LEVEL_STRENGTH)

#define TS711_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS711_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)
#define TS711_SCAN_OP (RIG_SCAN_VFO)

/*
 * There are some platform differences:
 *
 * + Tone on/off isn't applicable to "kenwood" versions of the E rigs
 * + Tone frequency is only A/B rigs
 * + Offset is only TS-811A, TS-811B, TS-811E, TS-711A, TS-711E.
 *
 * So maybe this should have a separate set of backends for each
 * rig variation because of the frequency range differences, capability
 * differences, etc, etc.
 */

static struct kenwood_priv_caps  ts711_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/*
 * vfo defines
*/
#define VFO_A   '0'
#define VFO_B   '1'
#define VFO_MEM '2'


/* Note: The 140/680/711/811 need this to set the VFO on the radio */
static int
ts711_set_vfo(RIG *rig, vfo_t vfo)
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
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "FN%c", vfo_function);
    return kenwood_transaction(rig, cmdbuf, NULL, 0);
}

/*
 * ts711 rig capabilities.
 */
const struct rig_caps ts711_caps =
{
    RIG_MODEL(RIG_MODEL_TS711),
    .model_name = "TS-711",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  10,

    .has_get_func =  TS711_FUNC_ALL,
    .has_set_func =  TS711_FUNC_ALL,
    .has_get_level =  TS711_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS711_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .vfo_ops =  TS711_VFO_OP,
    .scan_ops =  TS711_SCAN_OP,
    .ctcss_list =  kenwood38_ctcss_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  kHz(9.9),
    .max_xit =  0,
    .max_ifshift =  0,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    /* FIXME: split memories, call channel, etc. */
    .chan_list =  {
        {  1, 59, RIG_MTYPE_MEM },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {MHz(144), MHz(146), TS711_ALL_MODES, -1, -1, TS711_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {MHz(144), MHz(146), TS711_ALL_MODES, W(5), W(25), TS711_VFO},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {MHz(144), MHz(148), TS711_ALL_MODES, -1, -1, TS711_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), TS711_ALL_MODES, W(5), W(25), TS711_VFO},
        RIG_FRNG_END,
    }, /* tx range */


    .tuning_steps =  {
        {TS711_ALL_MODES, 50},
        {TS711_ALL_MODES, 100},
        {TS711_ALL_MODES, kHz(1)},
        {TS711_ALL_MODES, kHz(5)},
        {TS711_ALL_MODES, kHz(9)},
        {TS711_ALL_MODES, kHz(10)},
        {TS711_ALL_MODES, 12500},
        {TS711_ALL_MODES, kHz(20)},
        {TS711_ALL_MODES, kHz(25)},
        {TS711_ALL_MODES, kHz(100)},
        {TS711_ALL_MODES, MHz(1)},
        {TS711_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(2.2)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts711_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_mode =  kenwood_set_mode,
    .get_mode = kenwood_get_mode_if,
    .set_vfo = ts711_set_vfo,
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

