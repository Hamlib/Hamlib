/*
 *  Hamlib Kenwood backend - TRC-80 description
 *  Copyright (c) 2000-2009 by Stephane Fillod
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


#include "hamlib/rig.h"
#include "bandplan.h"
#include "kenwood.h"


#define TRC80_ALL_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_RTTY)
#define TRC80_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TRC80_AM_TX_MODES RIG_MODE_AM

/* TODO: make sure they are implemented by kenwood generic backend, and compatible */
#define TRC80_FUNC_ALL (RIG_FUNC_TUNER|RIG_FUNC_AIP|\
        RIG_FUNC_TONE|RIG_FUNC_AIP|RIG_FUNC_NB|RIG_FUNC_VOX)

#define TRC80_LEVEL_ALL (RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_MICGAIN|\
        RIG_LEVEL_RFPOWER|RIG_LEVEL_CWPITCH|RIG_LEVEL_BKIN_DLYMS|\
        RIG_LEVEL_SQL|RIG_LEVEL_VOXDELAY)

#define TRC80_PARMS (RIG_PARM_NONE)

#define TRC80_VFO_OPS (RIG_OP_NONE)
#define TRC80_SCAN_OPS (RIG_SCAN_VFO)

#define TRC80_VFO (RIG_VFO_MEM)
#define TRC80_ANTS (0)

#define TRC80_CHANNEL_CAPS { \
    .freq=1,\
    .mode=1,\
    .tx_freq=1,\
    .tx_mode=1,\
    .split=1,\
    .flags=RIG_CHFLAG_SKIP \
    }

static struct kenwood_priv_caps trc80_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/*
 * TRC-80/TK-80 rig capabilities.
 */
const struct rig_caps trc80_caps =
{
    RIG_MODEL(RIG_MODEL_TRC80),
    .model_name = "TRC-80",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  100,
    .timeout =  1000,
    .retry =  3,

    .has_get_func =  TRC80_FUNC_ALL,
    .has_set_func =  TRC80_FUNC_ALL,
    .has_get_level =  TRC80_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TRC80_LEVEL_ALL),
    .has_get_parm =  TRC80_PARMS,
    .has_set_parm =  RIG_LEVEL_SET(TRC80_PARMS),    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  kHz(1.1),
    .max_xit =  0,
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .vfo_ops = TRC80_VFO_OPS,
    .scan_ops =  TRC80_SCAN_OPS,

    .chan_list =  {
        {  1, 80, RIG_MTYPE_MEM, TRC80_CHANNEL_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), TRC80_ALL_MODES, -1, -1, TRC80_VFO},
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list1 =  {
        {MHz(1.8), MHz(30), TRC80_OTHER_TX_MODES, W(15), W(100), TRC80_VFO, TRC80_ANTS},
        {MHz(1.8), MHz(30), TRC80_AM_TX_MODES, W(5), W(25), TRC80_VFO, TRC80_ANTS}, /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(500), MHz(30), TRC80_ALL_MODES, -1, -1, TRC80_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(1.8), MHz(30), TRC80_OTHER_TX_MODES, W(15), W(100), TRC80_VFO, TRC80_ANTS},
        {MHz(1.8), MHz(30), TRC80_AM_TX_MODES, W(5), W(25), TRC80_VFO, TRC80_ANTS}, /* AM class */
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {      /* FIXME: TBC */
        {TRC80_ALL_MODES, 1},
        {TRC80_ALL_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.2)},
        {RIG_MODE_AM, kHz(5)},
        RIG_FLT_END,
    },
    .priv = (void *)& trc80_priv_caps,

    .rig_init    = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,

#ifdef XXREMOVEDXX
    .set_freq =  kenwood_set_freq,
#endif
    .get_freq =  kenwood_get_freq_if,
    .get_split_vfo =  kenwood_get_split_vfo_if,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  kenwood_set_func,
    .get_func =  kenwood_get_func,
    .set_level =  kenwood_set_level,
    .get_level =  kenwood_get_level,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .get_channel =  kenwood_get_channel,
    .scan =  kenwood_scan,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .get_info =  kenwood_get_info,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

