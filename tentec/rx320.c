/*
 *  Hamlib TenTenc backend - RX-320 PC-Radio description
 *  Copyright (c) 2001-2004 by Stephane Fillod
 *
 *	$Id: rx320.c,v 1.7 2004-06-14 21:12:14 fillods Exp $
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
#include "idx_builtin.h"
#include "tentec.h"


#define RX320_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB)

/* TODO: LINEOUT */
#define RX320_LEVELS (RIG_LEVEL_AGC|RIG_LEVEL_AF|RIG_LEVEL_RAWSTR)

#define RX320_VFO (RIG_VFO_A)

/* 
 * a bit coarse, but I don't have a RX320, and the manual is not
 * verbose on the subject. Please test it and report! --SF
 */
#define RX320_STR_CAL { 2, { \
		{      0, -60 }, \
		{  10000,  20 }, \
	} }

/*
 * rx320 receiver capabilities.
 *
 * protocol is documented at 
 *		http://www.tentec.com/rx320prg.zip
 *
 * TODO:
 */
const struct rig_caps rx320_caps = {
.rig_model =  RIG_MODEL_RX320,
.model_name = "RX-320",
.mfg_name =  "Ten-Tec",
.version =  "0.2",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_PCRECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  1200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  RIG_FUNC_NONE,
.has_get_level =  RX320_LEVELS,
.has_set_level =  RIG_LEVEL_SET(RX320_LEVELS),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 99999 } },
	[LVL_AF] = { .step = { .f = 1.0/64 } },
},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
			RIG_CHAN_END,
		},

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
	{kHz(100),MHz(30),RX320_MODES,-1,-1,RX320_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  { RIG_FRNG_END, },
.tuning_steps =  {
	 {RX320_MODES,10},	/* FIXME: add other ts */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.5)},
		{RIG_MODE_AM, kHz(6)},
		RIG_FLT_END,
	},
.str_cal = RX320_STR_CAL,

.rig_init =  tentec_init,
.rig_cleanup =  tentec_cleanup,
.set_freq =  tentec_set_freq,
.get_freq =  tentec_get_freq,
.set_mode =  tentec_set_mode,
.get_mode =  tentec_get_mode,
.set_level =  tentec_set_level,
.get_level =  tentec_get_level,
.get_info =  tentec_get_info,

};

/*
 * Function definitions below
 */

