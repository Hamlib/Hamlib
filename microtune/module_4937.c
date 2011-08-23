/*
 *  Hamlib microtune backend - 4937 file
 *  Copyright (c) 2003 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; see the file COPYING.  If not, write to
 *   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <hamlib/rig.h>

#include "microtune.h"

/*
 * Microtune 4937 rig capabilities.
 *
 * TODO: set_ptt
 */

#define M4937_FUNC  RIG_FUNC_NONE
#define M4937_LEVEL RIG_LEVEL_NONE
#define M4937_PARM  RIG_PARM_NONE

#define M4937_MODES (RIG_MODE_NONE)	/* FIXME: IF */

#define M4937_VFO RIG_VFO_A

static const struct confparams module_4937_ext_parms[] = {
	{ TOK_AGCGAIN, "agcgain", "AGC gain level", "RF and IF AGC levels",
		NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
	},
	{ RIG_CONF_END, NULL, }
};


const struct rig_caps module_4937_caps = {
  .rig_model =      RIG_MODEL_MICROTUNE_4937,
  .model_name =     "4937 DI5 tuner module",
  .mfg_name =       "Microtune",
  .version =        "0.2.1",
  .copyright =      "GPL",
  .status =         RIG_STATUS_UNTESTED,
  .rig_type =       RIG_TYPE_TUNER,
  .targetable_vfo = 	 0,
  .ptt_type =       RIG_PTT_RIG,
  .dcd_type =       RIG_DCD_NONE,
  .port_type =      RIG_PORT_PARALLEL,
  .has_get_func =   M4937_FUNC,
  .has_set_func =   M4937_FUNC,
  .has_get_level =  M4937_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(M4937_LEVEL),
  .has_get_parm = 	 M4937_PARM,
  .has_set_parm = 	 RIG_PARM_SET(M4937_PARM),
  .chan_list = 	 {
			RIG_CHAN_END,
		 },
  .scan_ops = 	 RIG_SCAN_NONE,
  .vfo_ops = 	 RIG_OP_NONE,
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { RIG_DBLST_END, },
  .preamp = 	 { RIG_DBLST_END, },
  .rx_range_list2 =  { {.start=MHz(55),.end=MHz(860),.modes=M4937_MODES,
		    .low_power=-1,.high_power=-1,M4937_VFO},
		    RIG_FRNG_END, },
  .tx_range_list2 =  { {.start=MHz(5),.end=MHz(42),.modes=M4937_MODES,
		    .low_power=mW(1),.high_power=W(1),M4937_VFO}, /* FIXME: power */
		    RIG_FRNG_END, },

  /* minimum tuning step with a reference divider of 640 (see Advance data sheet) */
  .tuning_steps =  { {M4937_MODES,kHz(50)},
			RIG_TS_END,
  },
  .extparms	= module_4937_ext_parms,
  .priv =  NULL,	/* priv */

  .rig_init =     microtune_init,
  .rig_cleanup =  microtune_cleanup,
  .rig_open =     module_4937_open,
  .rig_close =    microtune_close,

  .set_freq =     microtune_set_freq,
  .get_freq =     microtune_get_freq,
  .set_ext_level =     microtune_set_ext_level,
};

