/*
 *  Hamlib Drake backend - R-8B description
 *  Copyright (c) 2001-2002 by Stephane Fillod
 *
 *	$Id: r8b.c,v 1.3 2002-10-20 20:46:32 fillods Exp $
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

#include <hamlib/rig.h>
#include "drake.h"


#define R8B_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM|RIG_MODE_FM)

#define R8B_FUNC (RIG_FUNC_NONE)

#define R8B_LEVEL_ALL (RIG_LEVEL_NONE)

#define R8B_PARM_ALL (RIG_PARM_NONE)

#define R8B_VFO (RIG_VFO_A|RIG_VFO_B)

#define R8B_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_CPY)


/*
 * R-8B rig capabilities.
 *
 * manual: http://www.rldrake.com/swl/R8B.pdf
 *
 */

const struct rig_caps r8b_caps = {
.rig_model =  RIG_MODEL_DKR8B,
.model_name = "R-8B",
.mfg_name =  "Drake",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,		/* and only basic support */
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  1,
.timeout =  200,
.retry =  3,

.has_get_func =  R8B_FUNC,
.has_set_func =  R8B_FUNC,
.has_get_level =  R8B_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(R8B_LEVEL_ALL),
.has_get_parm =  R8B_PARM_ALL,
.has_set_parm =  RIG_PARM_SET(R8B_PARM_ALL),
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 10, RIG_DBLST_END },
.attenuator =   { 10, RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,	/* TODO: acutally has RIG_TRN_RIG */
.bank_qty =   0,
.chan_desc_sz =  7,
.vfo_ops =  R8B_VFO_OPS,

.chan_list =  {
		RIG_CHAN_END,	/* FIXME */
	},

.rx_range_list1 =  { 
	{kHz(10),MHz(30),R8B_MODES,-1,-1,R8B_VFO},
	RIG_FRNG_END,
  },
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
	{kHz(10),MHz(30),R8B_MODES,-1,-1,R8B_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  { RIG_FRNG_END, },

.tuning_steps =  {
	 {R8B_MODES,10},
	 {R8B_MODES,100},
	 {R8B_MODES,kHz(1)},
	 {R8B_MODES,kHz(10)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM, kHz(2.3)},	/* normal */
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM, kHz(6)},	/* wide */
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM, kHz(4)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM, kHz(1.8)},	/* narrow */
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM, Hz(500)},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  NULL,

.set_freq =  drake_set_freq,
.set_vfo =  drake_set_vfo,
.set_mode =  drake_set_mode,

.get_info =  drake_get_info,

};

/*
 * Function definitions below
 */

