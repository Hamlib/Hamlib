/*
 *  Hamlib Kenwood backend - TS940 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ts940.c,v 1.6 2005-04-04 21:31:57 fillods Exp $
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
#include "bandplan.h"
#include "kenwood.h"
#include "ic10.h"


#define TS940_ALL_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_CW|RIG_MODE_SSB)
#define TS940_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS940_AM_TX_MODES RIG_MODE_AM

#define TS940_FUNC_ALL RIG_FUNC_LOCK

#define TS940_LEVEL_ALL RIG_LEVEL_NONE

#define TS940_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define TS940_ANTS (0)

#define TS940_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)
#define TS940_SCAN_OPS (RIG_SCAN_VFO)

static const struct kenwood_priv_caps  ts940_priv_caps  = {
	.cmdtrm =  EOM_KEN,
	.if_len =  29,
};

/*
 * ts940 rig capabilities.
 * written from specs:
 * 	http://www.qsl.net/sm7vhs/radio/kenwood/ts940/specs.htm
 *
 * TODO: protocol to be check with manual!
 */
const struct rig_caps ts940_caps = {
.rig_model =  RIG_MODEL_TS940,
.model_name = "TS-940S",
.mfg_name =  "Kenwood",
.version =  BACKEND_VER "." IC10_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  4800,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  100,
.post_write_delay =  150,
.timeout =  200,
.retry =  3,

.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  TS940_FUNC_ALL,
.has_get_level =  TS940_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS940_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.preamp =   { RIG_DBLST_END, },	/* FIXME: preamp list */
.attenuator =   { RIG_DBLST_END, },	/* TBC */
.max_rit =  kHz(9.99),
.max_xit =  kHz(9.99),
.max_ifshift =  Hz(0),
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.vfo_ops =  TS940_VFO_OPS,
.scan_ops =  TS940_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  {
			{  0, 89, RIG_MTYPE_MEM, {IC10_CHANNEL_CAPS}  },	/* TBC */
			{ 90, 99, RIG_MTYPE_EDGE, {IC10_CHANNEL_CAPS} },
			RIG_CHAN_END,
		},

.rx_range_list1 =  { 
	{kHz(500),MHz(30),TS940_ALL_MODES,-1,-1,TS940_VFO},
	RIG_FRNG_END,
  }, /* rx range */

.tx_range_list1 =  { 
	FRQ_RNG_HF(1,TS940_OTHER_TX_MODES, W(5),W(250),TS940_VFO,TS940_ANTS),
	FRQ_RNG_HF(1,TS940_AM_TX_MODES, W(4),W(140),TS940_VFO,TS940_ANTS),   /* AM class */
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(500),MHz(30),TS940_ALL_MODES,-1,-1,TS940_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TS940_OTHER_TX_MODES, W(5),W(250),TS940_VFO,TS940_ANTS),
	FRQ_RNG_HF(2,TS940_AM_TX_MODES, W(4),W(140),TS940_VFO,TS940_ANTS),   /* AM class */
	RIG_FRNG_END,
  }, /* tx range */

.tuning_steps =  {		/* FIXME: TBC */
	 {TS940_ALL_MODES,10},
	 {TS940_ALL_MODES,kHz(100)},
	 {TS940_ALL_MODES,MHz(1)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_CW, Hz(500)},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM, kHz(2.4)},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts940_priv_caps,

.set_freq =  kenwood_set_freq,
.get_freq =  ic10_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_xit =  kenwood_set_xit,
.get_xit =  kenwood_get_xit,
.set_mode =  kenwood_set_mode,
.get_mode =  ic10_get_mode,
.set_vfo =  ic10_set_vfo,
.get_vfo =  ic10_get_vfo,
.set_split_vfo =  ic10_set_split_vfo,
.get_split_vfo =  ic10_get_split_vfo,
.set_ptt =  kenwood_set_ptt,
.get_ptt =  ic10_get_ptt,
.set_func =  kenwood_set_func,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  ic10_get_mem,
.set_trn =  kenwood_set_trn,
.scan =  kenwood_scan,
.set_channel = ic10_set_channel,
.get_channel = ic10_get_channel,
.decode_event = ic10_decode_event,

};

/*
 * Function definitions below
 */

