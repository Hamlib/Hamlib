/*
 * hamlib - (C) Stephane Fillod 2002, 2003 (fillods at users.sourceforge.net)
 *
 * ft990.c - 
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-990 using the "CAT" interface
 *
 *
 * $Id: ft990.c,v 1.1 2003-10-12 18:04:02 fillods Exp $
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

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft990.h"

static int ft990_init(RIG *rig);


/*
 * ft990 rigs capabilities.
 * Also this struct is READONLY!
 *
 * FIXME: INITIAL SKELTON REVISION, no support yet!
 */

const struct rig_caps ft990_caps = {
  .rig_model =          RIG_MODEL_FT990,
  .model_name =         "FT-990",
  .mfg_name =           "Yaesu",
  .version =            "0.0.0",
  .copyright =          "LGPL",
  .status =             RIG_STATUS_NEW,
  .rig_type =           RIG_TYPE_TRANSCEIVER,
  .ptt_type =           RIG_PTT_RIG,
  .dcd_type =           RIG_DCD_NONE,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    4800,
  .serial_data_bits =   8,
  .serial_stop_bits =   2,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        FT990_WRITE_DELAY,
  .post_write_delay =   FT990_POST_WRITE_DELAY,
  .timeout =            2000,
  .retry =              0,
  .has_get_func =       RIG_FUNC_TUNER,
  .has_set_func =       RIG_FUNC_TUNER,
  .has_get_level =      RIG_LEVEL_STRENGTH,
  .has_set_level =      RIG_LEVEL_NONE,
  .has_get_parm =       RIG_PARM_NONE,
  .has_set_parm =       RIG_PARM_NONE,
  .ctcss_list =         NULL,
  .dcs_list =           NULL,
  .preamp =             { RIG_DBLST_END, },
  .attenuator =         { RIG_DBLST_END, },
  .max_rit =            Hz(9999),
  .max_xit =            Hz(0),
  .max_ifshift =        Hz(0),
  .vfo_ops =            RIG_OP_TUNE,
  .targetable_vfo =     RIG_TARGETABLE_ALL,
  .transceive =         RIG_TRN_OFF,        /* Yaesus have to be polled, sigh */
  .bank_qty =           0,
  .chan_desc_sz =       0,
  .chan_list =          { RIG_CHAN_END, },  /* FIXME: memory channel list: 32 */

  .rx_range_list1 =     {
    {kHz(100), MHz(30), FT990_ALL_RX_MODES, -1, -1, FT990_VFO_ALL, FT990_ANTS},   /* General coverage + ham */
    RIG_FRNG_END,
  }, /* FIXME:  Are these the correct Region 1 values? */

  .tx_range_list1 =     {
	FRQ_RNG_HF(1, FT990_OTHER_TX_MODES, W(5), W(100), FT990_VFO_ALL, FT990_ANTS),
	FRQ_RNG_HF(1, FT990_AM_TX_MODES, W(2), W(25), FT990_VFO_ALL, FT990_ANTS),	/* AM class */

    RIG_FRNG_END,
  },

  .rx_range_list2 =     {
    {kHz(100), MHz(30), FT990_ALL_RX_MODES, -1, -1, FT990_VFO_ALL, FT990_ANTS},
    RIG_FRNG_END,
  },

  .tx_range_list2 =     {
	FRQ_RNG_HF(2, FT990_OTHER_TX_MODES, W(5), W(100), FT990_VFO_ALL, FT990_ANTS),
	FRQ_RNG_HF(2, FT990_AM_TX_MODES, W(2), W(25), FT990_VFO_ALL, FT990_ANTS),	/* AM class */

    RIG_FRNG_END,
  },

  .tuning_steps =       {
    {FT990_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
    {FT990_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

    {FT990_AM_RX_MODES,     Hz(100)},   /* Normal */
    {FT990_AM_RX_MODES,     kHz(1)},    /* Fast */

    {FT990_FM_RX_MODES,     Hz(100)},   /* Normal */
    {FT990_FM_RX_MODES,     kHz(1)},    /* Fast */

    RIG_TS_END,

  },

    /* mode/filter list, .remember =  order matters! */
  .filters =            {
    {RIG_MODE_SSB,  kHz(2.2)},  /* standard SSB filter bandwidth */
    {RIG_MODE_CW,   kHz(2.2)},  /* normal CW filter */
    {RIG_MODE_CW,   kHz(0.5)},  /* CW filter with narrow selection (must be installed!) */
    {RIG_MODE_AM,   kHz(6)},    /* normal AM filter */
    {RIG_MODE_AM,   kHz(2.2)},  /* AM filter with narrow selection (SSB filter switched in) */
    {RIG_MODE_FM,   kHz(12)},   /* FM  */

    RIG_FLT_END,
  },

  .priv =               NULL,           /* private data FIXME: */

  .rig_init =           ft990_init,
//  .rig_cleanup =        ft990_cleanup,
//  .rig_open =           ft990_open,     /* port opened */
//  .rig_close =          ft990_close,    /* port closed */

//  .set_freq =           ft990_set_freq,
};


/*
 * ************************************
 *
 * Hamlib API functions
 *
 * ************************************
 */

/*
 * rig_init
 *
 */
static int ft990_init(RIG *rig) {
 
  return -RIG_ENIMPL;
}


