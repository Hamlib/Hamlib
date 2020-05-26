/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft2000.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *            (C) Stephane Fillod 2008
 *            (C) Terry Embry 2008-2009
 * ft1200.c - (C) David Fannin 2015 (kk6df at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-1200 using the "CAT" interface
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ft1200.h"
#include "idx_builtin.h"
#include "tones.h"

/*
 * ft1200 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps ft1200_caps =
{
    RIG_MODEL(RIG_MODEL_FT1200),
    .model_name =         "FT-1200",
    .mfg_name =           "Yaesu",
    .version =            NEWCAT_VER ".0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,         /* Default rate per manual */
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   1,            /* Assumed since manual makes no mention */
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_HARDWARE,
    .write_delay =        FT1200_WRITE_DELAY,
    .post_write_delay =   FT1200_POST_WRITE_DELAY,
    .timeout =            2000,
    .retry =              3,
    .has_get_func =       FT1200_FUNCS,
    .has_set_func =       FT1200_FUNCS,
    .has_get_level =      FT1200_LEVELS,
    .has_set_level =      RIG_LEVEL_SET(FT1200_LEVELS),
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 1050 }, .step = { .i = 10 } },
    },
    .ctcss_list =         common_ctcss_list,
    .dcs_list =           NULL,
    .preamp =             { 10, 20, RIG_DBLST_END, }, /* TBC */
    .attenuator =         { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        Hz(1000),
    .vfo_ops =            FT1200_VFO_OPS,
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,        /* May enable later as the 1200 has an Auto Info command */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .str_cal =            FT1200_STR_CAL,
    .chan_list =          {
        {   1,  99, RIG_MTYPE_MEM,  NEWCAT_MEM_CAP },
        { 100, 117, RIG_MTYPE_EDGE, NEWCAT_MEM_CAP },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =     {
        /* General coverage + ham, ANT_5 is RX only antenna */
        {kHz(30), MHz(56), FT1200_ALL_RX_MODES, -1, -1, FT1200_VFO_ALL, FT1200_TX_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT1200_OTHER_TX_MODES, W(5), W(100), FT1200_VFO_ALL, FT1200_TX_ANTS),
        FRQ_RNG_HF(1, FT1200_AM_TX_MODES, W(2), W(25), FT1200_VFO_ALL, FT1200_TX_ANTS), /* AM class */
        FRQ_RNG_6m(1, FT1200_OTHER_TX_MODES, W(5), W(100), FT1200_VFO_ALL, FT1200_TX_ANTS),
        FRQ_RNG_6m(1, FT1200_AM_TX_MODES, W(2), W(25), FT1200_VFO_ALL, FT1200_TX_ANTS), /* AM class */

        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(30), MHz(56), FT1200_ALL_RX_MODES, -1, -1, FT1200_VFO_ALL, FT1200_TX_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT1200_OTHER_TX_MODES, W(5), W(100), FT1200_VFO_ALL, FT1200_TX_ANTS),
        FRQ_RNG_HF(2, FT1200_AM_TX_MODES, W(2), W(25), FT1200_VFO_ALL, FT1200_TX_ANTS), /* AM class */
        FRQ_RNG_6m(2, FT1200_OTHER_TX_MODES, W(5), W(100), FT1200_VFO_ALL, FT1200_TX_ANTS),
        FRQ_RNG_6m(2, FT1200_AM_TX_MODES, W(2), W(25), FT1200_VFO_ALL, FT1200_TX_ANTS), /* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT1200_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT1200_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT1200_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT1200_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT1200_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT1200_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,

    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {FT1200_CW_RTTY_PKT_RX_MODES,  Hz(2400)},   /* Normal CW, RTTY, PKT/USER */
        {FT1200_CW_RTTY_PKT_RX_MODES,  Hz(500)},    /* Narrow CW, RTTY, PKT/USER */
        {FT1200_CW_RTTY_PKT_RX_MODES,  Hz(2400)},   /* Wide   CW, RTTY, PKT/USER */
        {RIG_MODE_SSB,                 Hz(2400)},   /* Normal SSB */
        {RIG_MODE_SSB,                 Hz(1800)},   /* Narrow SSB */
        {RIG_MODE_SSB,                 Hz(4000)},   /* Wide   SSB */
        {RIG_MODE_AM,                  Hz(9000)},   /* Normal AM  */
        {RIG_MODE_AM,                  Hz(6000)},   /* Narrow AM  */
        {FT1200_FM_RX_MODES,           Hz(12000)},  /* Normal FM  */
        {FT1200_FM_RX_MODES,           Hz(8000)},   /* Narrow FM  */

        RIG_FLT_END,
    },

    .priv =               NULL,

    .rig_init =           newcat_init,
    .rig_cleanup =        newcat_cleanup,
    .rig_open =           newcat_open,     /* port opened */
    .rig_close =          newcat_close,    /* port closed */

    .cfgparams =          newcat_cfg_params,
    .set_conf =           newcat_set_conf,
    .get_conf =           newcat_get_conf,
    .set_freq =           newcat_set_freq,
    .get_freq =           newcat_get_freq,
    .set_mode =           newcat_set_mode,
    .get_mode =           newcat_get_mode,
    .set_vfo =            newcat_set_vfo,
    .get_vfo =            newcat_get_vfo,
    .set_ptt =            newcat_set_ptt,
    .get_ptt =            newcat_get_ptt,
    .set_split_vfo =      newcat_set_split_vfo,
    .get_split_vfo =      newcat_get_split_vfo,
    .set_rit =            newcat_set_rit,
    .get_rit =            newcat_get_rit,
    .set_xit =            newcat_set_xit,
    .get_xit =            newcat_get_xit,
    .set_ant =            newcat_set_ant,
    .get_ant =            newcat_get_ant,
    .get_func =           newcat_get_func,
    .set_func =           newcat_set_func,
    .get_level =          newcat_get_level,
    .set_level =          newcat_set_level,
    .get_mem =            newcat_get_mem,
    .set_mem =            newcat_set_mem,
    .vfo_op =             newcat_vfo_op,
    .get_info =           newcat_get_info,
    .power2mW =           newcat_power2mW,
    .mW2power =           newcat_mW2power,
    .set_rptr_shift =     newcat_set_rptr_shift,
    .get_rptr_shift =     newcat_get_rptr_shift,
    .set_ctcss_tone =     newcat_set_ctcss_tone,
    .get_ctcss_tone =     newcat_get_ctcss_tone,
    .set_ctcss_sql  =     newcat_set_ctcss_sql,
    .get_ctcss_sql  =     newcat_get_ctcss_sql,
    .set_powerstat =      newcat_set_powerstat,
    .get_powerstat =      newcat_get_powerstat,
    .get_ts =             newcat_get_ts,
    .set_ts =             newcat_set_ts,
    .set_trn =            newcat_set_trn,
    .get_trn =            newcat_get_trn,
    .set_channel =        newcat_set_channel,
    .get_channel =        newcat_get_channel,

};
