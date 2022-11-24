/*
 *  Hamlib Kenwood backend - TS-790 description
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


#include <hamlib/rig.h>
#include "kenwood.h"

#define TS790_ALL_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM)
#define TS790_HI_MODES (RIG_MODE_CW|RIG_MODE_FM)
#define TS790_LO_MODES (RIG_MODE_SSB)

/* func and levels to be checked */
#define TS790_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_LOCK|RIG_FUNC_MUTE)

#define TS790_LEVEL_ALL (RIG_LEVEL_STRENGTH)

#define TS790_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS790_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define TS790_SCAN_OP (RIG_SCAN_VFO)

#define TS790_CHANNEL_CAPS \
    .freq=1,\
    .mode=1,\
    .tx_freq=1,\
    .tx_mode=1,\
    .split=1,\
    .flags=RIG_CHFLAG_SKIP, \
    .ctcss_tone=1

/*
 * Function definitions below
 */

static struct kenwood_priv_caps  ts790_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/*
 * TS790 A/E rig capabilities.
 * Interface provided by optional IF-232C
 *
 * TODO: get_ts, ctcss_sql, set_rptr_shift
 *
 * part of infos comes from http://www.kenwood.net/
 */
const struct rig_caps ts790_caps =
{
    RIG_MODEL(RIG_MODEL_TS790),
    .model_name = "TS-790",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".0",
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
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  10,

    .has_get_func =  TS790_FUNC_ALL,
    .has_set_func =  TS790_FUNC_ALL,
    .has_get_level =  TS790_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS790_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .vfo_ops =  TS790_VFO_OP,
    .scan_ops =  TS790_SCAN_OP,
    .ctcss_list =  kenwood38_ctcss_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { /* 12, */ RIG_DBLST_END, },
    .max_rit =  kHz(9.9),   /* this is for FM, SSB/CW is 1.9kHz */
    .max_xit =  0,
    .max_ifshift =  0,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    // No AGC levels
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { {  1, 59, RIG_MTYPE_MEM, { TS790_CHANNEL_CAPS } },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {MHz(144), MHz(146), TS790_ALL_MODES, -1, -1, TS790_VFO},
        {MHz(430), MHz(440), TS790_ALL_MODES, -1, -1, TS790_VFO},
#if 0
        /* 23 cm option */
        {MHz(1240), MHz(1300), TS790_ALL_MODES, -1, -1, TS790_VFO},
#endif
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {MHz(144), MHz(146), TS790_LO_MODES, W(5), W(35), TS790_VFO},
        {MHz(144), MHz(146), TS790_HI_MODES, W(5), W(45), TS790_VFO},
        {MHz(430), MHz(440), TS790_LO_MODES, W(5), W(30), TS790_VFO},
        {MHz(430), MHz(440), TS790_HI_MODES, W(5), W(40), TS790_VFO},
#if 0
        {MHz(1240), MHz(1300), TS790_ALL_MODES, W(5), W(10), TS790_VFO},
#endif
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {MHz(144), MHz(148), TS790_ALL_MODES, -1, -1, TS790_VFO},
        {MHz(430), MHz(450), TS790_ALL_MODES, -1, -1, TS790_VFO},
#if 0
        {MHz(1240), MHz(1300), TS790_ALL_MODES, -1, -1, TS790_VFO},
#endif
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), TS790_LO_MODES, W(5), W(35), TS790_VFO},
        {MHz(144), MHz(148), TS790_HI_MODES, W(5), W(45), TS790_VFO},
        {MHz(430), MHz(450), TS790_LO_MODES, W(5), W(30), TS790_VFO},
        {MHz(430), MHz(450), TS790_HI_MODES, W(5), W(40), TS790_VFO},
#if 0
        {MHz(1240), MHz(1300), TS790_ALL_MODES, W(5), W(10), TS790_VFO},
#endif
        RIG_FRNG_END,
    }, /* tx range */


    .tuning_steps =  {
        {TS790_ALL_MODES, 50},
        {TS790_ALL_MODES, 100},
        {TS790_ALL_MODES, kHz(1)},
        {TS790_ALL_MODES, kHz(5)},
        {TS790_ALL_MODES, kHz(9)},
        {TS790_ALL_MODES, kHz(10)},
        {TS790_ALL_MODES, 12500},
        {TS790_ALL_MODES, kHz(20)},
        {TS790_ALL_MODES, kHz(25)},
        {TS790_ALL_MODES, kHz(100)},
        {TS790_ALL_MODES, MHz(1)},
        {TS790_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR, kHz(2.1)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts790_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode_if,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_split_vfo =  kenwood_set_split_vfo,
    .get_split_vfo =  kenwood_get_split_vfo_if,
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
    .scan =  kenwood_scan,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_channel = kenwood_set_channel,
    .get_channel = kenwood_get_channel,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .get_info =  kenwood_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


