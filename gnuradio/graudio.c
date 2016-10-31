/*
 *  Hamlib GNUradio backend - graudio/any rig
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
 * GNU Radio, sound card based.
 *
 */

#define GRAUDIO_FUNC  RIG_FUNC_NONE
#define GRAUDIO_LEVEL (RIG_LEVEL_AF|RIG_LEVEL_RF)
#define GRAUDIO_PARM  RIG_PARM_NONE
#define GRAUDIO_VFO_OP  (RIG_OP_UP|RIG_OP_DOWN)
#define GRAUDIO_SCAN	RIG_SCAN_NONE

/*
 * GNU Radio Audio has no WFM mode because bandwidth is too wide!
 */
#define GRAUDIO_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_SSB)

#define GRAUDIO_VFO (RIG_VFO_A|RIG_VFO_B)

static const struct gnuradio_priv_caps graudio_priv_caps = {
	.tuner_model = RIG_MODEL_DUMMY,
	.input_rate = 48000,	/* To be fixed, later */
	.IF_center_freq = 0 /* -kHz(10) */,
};


const struct rig_caps graudio_caps = {
  .rig_model =      RIG_MODEL_GRAUDIO,
  .model_name =     "GNU Radio GrAudio",
  .mfg_name =       "GNU",
  .version =        "0.1.2",
  .copyright = 	    "LGPL",
  .status =         RIG_STATUS_ALPHA,
  .rig_type =       RIG_TYPE_PCRECEIVER,
  .targetable_vfo = 	 RIG_TARGETABLE_ALL,
  .ptt_type =       RIG_PTT_RIG,
  .dcd_type =       RIG_DCD_RIG,
  .port_type =      RIG_PORT_NONE,
  .has_get_func =   GRAUDIO_FUNC,
  .has_set_func =   GRAUDIO_FUNC,
  .has_get_level =  GRAUDIO_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(GRAUDIO_LEVEL),
  .has_get_parm = 	 GRAUDIO_PARM,
  .has_set_parm = 	 RIG_PARM_SET(GRAUDIO_PARM),
  .ctcss_list = 	 NULL,
  .dcs_list =   	 NULL,
  .chan_list = 	 {
			RIG_CHAN_END,
		 },
  .scan_ops = 	 GRAUDIO_SCAN,
  .vfo_ops = 	 GRAUDIO_VFO_OP,
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { RIG_DBLST_END, },
  .preamp = 	 { RIG_DBLST_END, },
  .rx_range_list2 =  { {.start=kHz(100),.end=MHz(30),.modes=GRAUDIO_MODES,
		    .low_power=-1,.high_power=-1,GRAUDIO_VFO},
		    RIG_FRNG_END, },
  .tx_range_list2 =  { RIG_FRNG_END, },
  .tuning_steps =  { {GRAUDIO_MODES,1}, {GRAUDIO_MODES,RIG_TS_ANY}, RIG_TS_END, },
  .filters =      {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.4)},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM, kHz(15)},
		{GRAUDIO_MODES, RIG_FLT_ANY},
		RIG_FLT_END,
  },

  .priv =  (void*)&graudio_priv_caps,

  .rig_init =     gr_init,
  .rig_cleanup =  gr_cleanup,
  .rig_open =     graudio_open,
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

const struct rig_caps graudioiq_caps = {
  .rig_model =      RIG_MODEL_GRAUDIOIQ,
  .model_name =     "GNU Radio GrAudio I&Q",
  .mfg_name =       "GNU",
  .version =        "0.1.2",
  .copyright = 	    "LGPL",
  .status =         RIG_STATUS_UNTESTED,
  .rig_type =       RIG_TYPE_PCRECEIVER,
  .targetable_vfo = 	 RIG_TARGETABLE_ALL,
  .ptt_type =       RIG_PTT_RIG,
  .dcd_type =       RIG_DCD_RIG,
  .port_type =      RIG_PORT_NONE,
  .has_get_func =   GRAUDIO_FUNC,
  .has_set_func =   GRAUDIO_FUNC,
  .has_get_level =  GRAUDIO_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(GRAUDIO_LEVEL),
  .has_get_parm = 	 GRAUDIO_PARM,
  .has_set_parm = 	 RIG_PARM_SET(GRAUDIO_PARM),
  .ctcss_list = 	 NULL,
  .dcs_list =   	 NULL,
  .chan_list = 	 {
			RIG_CHAN_END,
		 },
  .scan_ops = 	 GRAUDIO_SCAN,
  .vfo_ops = 	 GRAUDIO_VFO_OP,
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { RIG_DBLST_END, },
  .preamp = 	 { RIG_DBLST_END, },
  .rx_range_list2 =  { {.start=kHz(100),.end=MHz(30),.modes=GRAUDIO_MODES,
		    .low_power=-1,.high_power=-1,GRAUDIO_VFO},
		    RIG_FRNG_END, },
  .tx_range_list2 =  { RIG_FRNG_END, },
  .tuning_steps =  { {GRAUDIO_MODES,1}, {GRAUDIO_MODES,RIG_TS_ANY}, RIG_TS_END, },
  .filters =      {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.4)},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM, kHz(15)},
		{GRAUDIO_MODES, RIG_FLT_ANY},
		RIG_FLT_END,
  },

  .priv =  (void*)&graudio_priv_caps,

  .rig_init =     gr_init,
  .rig_cleanup =  gr_cleanup,
  .rig_open =     graudioiq_open,
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

