/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft450.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-450 using the "CAT" interface
 *
 *
 * $Id: ft450.c,v 1.1 2007-11-22 04:48:43 n0nb Exp $
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
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
#include "ft450.h"

/*
 * ft450 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps ft450_caps = {
    .rig_model =          RIG_MODEL_FT450,
    .model_name =         "FT-450",
    .mfg_name =           "Yaesu",
    .version =            "0.0.1",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_ALPHA,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,         /* Default rate per manual */
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   1,            /* Assumed since manual makes no mention */
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        FT450_WRITE_DELAY,
    .post_write_delay =   FT450_POST_WRITE_DELAY,
    .timeout =            2000,
    .retry =              0,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =       RIG_FUNC_NONE,
    .has_get_level =      RIG_LEVEL_NONE,
    .has_set_level =      RIG_LEVEL_NONE,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .ctcss_list =         NULL,
    .dcs_list =           NULL,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(0),
    .max_ifshift =        Hz(1000),
    .vfo_ops =            RIG_OP_TUNE,
    .targetable_vfo =     RIG_TARGETABLE_ALL,
    .transceive =         RIG_TRN_OFF,        /* May enable later as the 450 has an Auto Info command */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          { RIG_CHAN_END, },

    .rx_range_list1 =     {
        {kHz(30), MHz(56), FT450_ALL_RX_MODES, -1, -1, FT450_VFO_ALL, FT450_ANTS},   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* FIXME:  Are these the correct Region 1 values? */

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT450_OTHER_TX_MODES, W(5), W(100), FT450_VFO_ALL, FT450_ANTS),
        FRQ_RNG_HF(1, FT450_AM_TX_MODES, W(2), W(25), FT450_VFO_ALL, FT450_ANTS),	/* AM class */

        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(30), MHz(56), FT450_ALL_RX_MODES, -1, -1, FT450_VFO_ALL, FT450_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT450_OTHER_TX_MODES, W(5), W(100), FT450_VFO_ALL, FT450_ANTS),
        FRQ_RNG_HF(2, FT450_AM_TX_MODES, W(2), W(25), FT450_VFO_ALL, FT450_ANTS),	/* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT450_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT450_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT450_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT450_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT450_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT450_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,

    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {RIG_MODE_SSB,  kHz(2.4)},  /* standard SSB filter bandwidth */
        {RIG_MODE_CW,   kHz(2.4)},  /* normal CW filter */
        {RIG_MODE_CW,   kHz(0.5)},  /* CW filter with narrow selection (must be installed!) */
        {RIG_MODE_AM,   kHz(6)},    /* normal AM filter */
        {RIG_MODE_AM,   kHz(2.4)},  /* AM filter with narrow selection (SSB filter switched in) */
        {RIG_MODE_FM,   kHz(12)},   /* FM  */

        RIG_FLT_END,
    },

    .priv =               NULL,           /* private data FIXME: */

    .rig_init =           newcat_init,
    .rig_cleanup =        newcat_cleanup,
    .rig_open =           newcat_open,     /* port opened */
    .rig_close =          newcat_close,    /* port closed */

    .set_freq =           newcat_set_freq,
    .get_freq =           newcat_get_freq,
//    .set_mode =           newcat_set_mode,
//    .get_mode =           newcat_get_mode,
    .set_vfo =            newcat_set_vfo,
    .get_vfo =            newcat_get_vfo,
//    .set_ptt =            newcat_set_ptt,
//    .get_ptt =            newcat_get_ptt,
//    .set_split_vfo =      newcat_set_split_vfo,
//    .get_split_vfo =      newcat_get_split_vfo,
//    .set_rit =            newcat_set_rit,
//    .get_rit =            newcat_get_rit,
//    .set_func =           newcat_set_func,
//    .get_level =          newcat_get_level,
//    .vfo_op =             newcat_vfo_op,
};

