/*
 *  Hamlib TenTenc backend - TT-565 description
 *  Copyright (c) 2004 by Stephane Fillod
 *
 *	$Id: orion.c,v 1.1 2004-05-03 22:33:13 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <hamlib/rig.h>
#include "tentec2.h"
#include "bandplan.h"


#define TT565_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
			RIG_MODE_RTTY|RIG_MODE_AM)
#define TT565_RXMODES (TT565_MODES)

#define TT565_FUNCS (RIG_FUNC_LOCK|RIG_FUNC_TUNER)

#define TT565_LEVELS (RIG_LEVEL_RAWSTR|/*RIG_LEVEL_NB|*/ \
				RIG_LEVEL_SQL|RIG_LEVEL_IF| \
				RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD| \
				RIG_LEVEL_RF|RIG_LEVEL_NR| \
				/*RIG_LEVEL_ANF|*/RIG_LEVEL_MICGAIN| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOX| \
				RIG_LEVEL_COMP|RIG_LEVEL_PREAMP| \
				RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT565_ANTS (RIG_ANT_1|RIG_ANT_2) 
#define TT565_RXANTS (TT565_ANTS|RIG_ANT_3) 

#define TT565_PARMS (RIG_PARM_NONE)

#define TT565_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT565_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|\
		RIG_OP_TO_VFO|RIG_OP_FROM_VFO| \
		RIG_OP_TUNE)


/*
 * tt565 transceiver capabilities.
 *
 * Protocol is documented at 
 *		http://www.rfsquared.com/
 *
 * Only set_freq is supposed to work.
 * This is a skelton.
 */
const struct rig_caps tt565_caps = {
.rig_model =  RIG_MODEL_TT565,
.model_name = "TT-565 Orion",
.mfg_name =  "Ten-Tec",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  57600,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  400,
.retry =  3,

.has_get_func =  TT565_FUNCS,
.has_set_func =  TT565_FUNCS,
.has_get_level =  TT565_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT565_LEVELS),
.has_get_parm =  TT565_PARMS,
.has_set_parm =  TT565_PARMS,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 20, RIG_DBLST_END },	/* TBC */
.attenuator =   { 6, 12, 18, RIG_DBLST_END },
.max_rit =  kHz(8),
.max_xit =  kHz(8),
.max_ifshift =  kHz(8),
.targetable_vfo =  RIG_TARGETABLE_ALL,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
		{   0, 199, RIG_MTYPE_MEM, TT_MEM_CAP },
		},

.rx_range_list1 =  {
	FRQ_RNG_HF(1,TT565_RXMODES, -1,-1,RIG_VFO_N(0),TT565_RXANTS),
	{kHz(500),MHz(30),TT565_RXMODES,-1,-1,RIG_VFO_N(1),TT565_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS),
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	FRQ_RNG_HF(2,TT565_RXMODES, -1,-1,RIG_VFO_N(0),TT565_RXANTS),
	{MHz(5.25),MHz(5.40),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(500),MHz(30),TT565_RXMODES,-1,-1,RIG_VFO_N(1),TT565_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS),
	{MHz(5.25),MHz(5.40),TT565_MODES,W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT565_RXMODES,1},
	 {TT565_RXMODES,10},
	 {TT565_RXMODES,100},
	 {TT565_RXMODES,kHz(1)},
	 {TT565_RXMODES,kHz(10)},
	 {TT565_RXMODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
	/* 9MHz IF filters: 15kHz, 6kHz, 2.4kHz, 1.0kHz */
	/* opt: 1.8kHz, 500Hz, 250Hz */
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, 100},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, kHz(6)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, 0}, /* 590 filters */
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_AM, kHz(4)},
		{RIG_MODE_FM, kHz(15)},
		RIG_FLT_END,
	},
.priv =  (void*)NULL,

/*
 * Of the following, maybe only tentec2_set_freq is expected
 * to work in compatibility mode. Can't they use a standard
 * protocol among their line of products?
 */
.set_freq =  tentec2_set_freq,
.get_freq =  tentec2_get_freq,
.set_vfo =  tentec2_set_vfo,
.get_vfo =  tentec2_get_vfo,
.set_mode =  tentec2_set_mode,
.get_mode =  tentec2_get_mode,
.set_split_vfo =  tentec2_set_split_vfo,
.get_split_vfo =  tentec2_get_split_vfo,
.set_ptt =  tentec2_set_ptt,
.reset =  tentec2_reset,
.get_info =  tentec2_get_info,

};

/*
 * Function definitions below
 */


