/*
 *  Hamlib Kenwood backend - TS140 description
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: ts140.c,v 1.2 2005-01-19 22:23:21 pa4tu Exp $
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
#include <bandplan.h>
#include "kenwood.h"


#define TS140_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define TS140_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define TS140_AM_TX_MODES RIG_MODE_AM
#define TS140_VFO (RIG_VFO_A|RIG_VFO_B)
#define TS140_ANTS (RIG_ANT_1)

static const struct kenwood_priv_caps  ts140_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/*
 * ts140 rig capabilities.
 * extrapolated from TS680, which is more or less the same radio
 *	MattD.. 2005-01-14
 *
 */
const struct rig_caps ts140_caps = {
.rig_model =  RIG_MODEL_TS140S,
.model_name = "TS-140S",
.mfg_name =  "Kenwood",
.version =  "0.2.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  RIG_FUNC_LOCK,
.has_set_func =  RIG_FUNC_LOCK,
.has_get_level =  RIG_LEVEL_NONE,
.has_set_level =  RIG_LEVEL_NONE,
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },	/* Not controllable */
.attenuator =   { RIG_DBLST_END, }, /* Not Controllable */
.max_rit =  kHz(1.2),
.max_xit =  kHz(1.2),
.max_ifshift =  Hz(0), /* Not controllable */
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  {
			{  0, 19, RIG_MTYPE_MEM  },	/* TBC */
			{ 20, 30, RIG_MTYPE_EDGE },
			RIG_CHAN_END,
		},

.rx_range_list1 =  { 
	{kHz(50),kHz(34999),TS140_ALL_MODES,-1,-1,TS140_VFO},
	RIG_FRNG_END,
  }, /* rx range */

.tx_range_list1 =  { 
	FRQ_RNG_HF(1,TS140_OTHER_TX_MODES, W(5),W(100),TS140_VFO,TS140_ANTS),	/* SSB actually 110W */
	FRQ_RNG_HF(1,TS140_AM_TX_MODES, W(4),W(40),TS140_VFO,TS140_ANTS),   /* AM class */
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(50),kHz(34999),TS140_ALL_MODES,-1,-1,TS140_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TS140_OTHER_TX_MODES, W(5),W(100),TS140_VFO,TS140_ANTS),	/* SSB actually 110W */
	FRQ_RNG_HF(2,TS140_AM_TX_MODES, W(4),W(40),TS140_VFO,TS140_ANTS),   /* AM class */
	RIG_FRNG_END,
  }, /* tx range */


.tuning_steps =  {
	 {TS140_ALL_MODES,10},
	 {TS140_ALL_MODES,100},
	 {TS140_ALL_MODES,kHz(1)},
	 {TS140_ALL_MODES,kHz(5)},
	 {TS140_ALL_MODES,kHz(9)},
	 {TS140_ALL_MODES,kHz(10)},
	 {TS140_ALL_MODES,12500},
	 {TS140_ALL_MODES,kHz(20)},
	 {TS140_ALL_MODES,kHz(25)},
	 {TS140_ALL_MODES,kHz(100)},
	 {TS140_ALL_MODES,MHz(1)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.2)},
		{RIG_MODE_CW, 600},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts140_priv_caps,

.set_freq =  kenwood_set_freq,
.get_freq =  kenwood_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_xit =  kenwood_set_xit,
.get_xit =  kenwood_get_xit,
.set_mode =  kenwood_set_mode, /* With the IF-10C, these can only set, not get mode */
.set_vfo =  kenwood_set_vfo,
.get_vfo =  kenwood_get_vfo,
.get_ptt =  kenwood_get_ptt,
.set_ptt =  kenwood_set_ptt,
.get_dcd =  kenwood_get_dcd,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  kenwood_get_mem,
.reset =  kenwood_reset,

};

/*
 * Function definitions below
 */


