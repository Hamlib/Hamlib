/*
 *  Hamlib GNUradio backend - MC4020/eb4937
 *  Copyright (c) 2001-2003 by Stephane Fillod
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

#include <stdlib.h>

#include <hamlib/rig.h>

#include "gnuradio.h"

/*
 * GNU Radio MC4020 rig capabilities.
 *
 */

#define MC4020_FUNC  RIG_FUNC_NONE
#define MC4020_LEVEL (RIG_LEVEL_AF|RIG_LEVEL_RF)
#define MC4020_PARM  RIG_PARM_NONE
#define MC4020_VFO_OP  (RIG_OP_UP|RIG_OP_DOWN)
#define MC4020_SCAN	RIG_SCAN_NONE

#define MC4020_MODES (RIG_MODE_WFM|RIG_MODE_FM|RIG_MODE_SSB)

#define MC4020_VFO (RIG_VFO_A|RIG_VFO_B)

static const struct gnuradio_priv_caps mc4020_priv_caps = {
	.tuner_model = RIG_MODEL_MICROTUNE_4937,
	.input_rate = 20000000,
	.IF_center_freq = MHz(5.75),
};


const struct rig_caps mc4020_caps = {
  .rig_model =      RIG_MODEL_MC4020,
  .model_name =     "GNU Radio MC4020",
  .mfg_name =       "GNU",
  .version =        "0.1.1",
  .copyright = 	    "LGPL",
  .status =         RIG_STATUS_ALPHA,
  .rig_type =       RIG_TYPE_PCRECEIVER,
  .targetable_vfo = 	 RIG_TARGETABLE_ALL,
  .ptt_type =       RIG_PTT_RIG,
  .dcd_type =       RIG_DCD_RIG,
  .port_type =      RIG_PORT_NONE,
  .has_get_func =   MC4020_FUNC,
  .has_set_func =   MC4020_FUNC,
  .has_get_level =  MC4020_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(MC4020_LEVEL),
  .has_get_parm = 	 MC4020_PARM,
  .has_set_parm = 	 RIG_PARM_SET(MC4020_PARM),
  .ctcss_list = 	 NULL,
  .dcs_list =   	 NULL,
  .chan_list = 	 {
			RIG_CHAN_END,
		 },
  .scan_ops = 	 MC4020_SCAN,
  .vfo_ops = 	 MC4020_VFO_OP,
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { RIG_DBLST_END, },
  .preamp = 	 { RIG_DBLST_END, },
  .rx_range_list2 =  { {.start=MHz(55),.end=MHz(860),.modes=MC4020_MODES,
		    .low_power=-1,.high_power=-1,MC4020_VFO},
		    RIG_FRNG_END, },
  .tx_range_list2 =  { {.start=MHz(5),.end=MHz(42),.modes=MC4020_MODES,
		    .low_power=mW(1),.high_power=W(1),MC4020_VFO}, /* FIXME: power */
		    RIG_FRNG_END, },
  .tuning_steps =  { {MC4020_MODES,1}, {MC4020_MODES,RIG_TS_ANY}, RIG_TS_END, },
  .filters =      {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.4)},
		{RIG_MODE_AM, kHz(8)},
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_WFM, kHz(230)},
		{MC4020_MODES, RIG_FLT_ANY},
		RIG_FLT_END,
  },

  .priv =  (void*)&mc4020_priv_caps,

  .rig_init =     gr_init,
  .rig_cleanup =  gr_cleanup,
  .rig_open =     mc4020_open,
  .rig_close =    gr_close,

  .cfgparams =    gnuradio_cfg_params,
  .set_conf =     gnuradio_set_conf,
  .get_conf =     gnuradio_get_conf,

  .set_freq =     gr_set_freq,
  .get_freq =     gr_get_freq,

  .set_vfo =      gr_set_vfo,
  .get_vfo =      gr_get_vfo,
  .set_mode =     gr_set_mode,
  .get_mode =     gr_get_mode,

  .set_level =	  gnuradio_set_level,
  .get_level =	  gnuradio_get_level,

  .set_rit =	  gnuradio_set_rit,
  .get_rit =	  gnuradio_get_rit,
  .set_ts =	  gnuradio_set_ts,
  .get_ts =	  gnuradio_get_ts,
  .vfo_op =	  gnuradio_vfo_op,
};

