/*
 *  Hamlib TenTenc backend - TT-550 PC-Radio description
 *  Copyright (c) 2002-2003 by Stephane Fillod
 *
 *	$Id: pegasus.c,v 1.4 2003-11-16 17:14:44 fillods Exp $
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


#define TT550_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TT550_RXMODES (TT550_MODES|RIG_MODE_AM)

#define TT550_FUNCS (RIG_FUNC_FAGC|RIG_FUNC_VOX|RIG_FUNC_ANF| \
				RIG_FUNC_NR|RIG_FUNC_NB|RIG_FUNC_COMP)

#define TT550_LEVELS (RIG_LEVEL_AGC|RIG_LEVEL_AF|RIG_LEVEL_RAWSTR| \
				RIG_LEVEL_RF|RIG_LEVEL_COMP|RIG_LEVEL_VOX|RIG_LEVEL_SQL| \
				RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD| \
				RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT550_VFO (RIG_VFO_A)

/* 
 * a bit coarse, but I don't have a TT550, and the manual is not
 * verbose on the subject. Please test it and report! --SF
 */
#define TT550_STR_CAL { 2, { \
		{      0, -60 }, \
		{  10000,  20 }, \
	} }

/*
 * tt550 receiver capabilities.
 *
 * protocol is documented at 
 *		http://www.tentec.com/550/550prg2.pdf
 *
 * TODO:
 */
const struct rig_caps tt550_caps = {
.rig_model =  RIG_MODEL_TT550,
.model_name = "TT-550",
.mfg_name =  "Ten-Tec",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_COMPUTER,
.ptt_type =  RIG_PTT_NONE,
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
.timeout =  200,
.retry =  3,

.has_get_func =  TT550_FUNCS,
.has_set_func =  TT550_FUNCS,
.has_get_level =  TT550_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT550_LEVELS),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran = {
	[LVL_RAWSTR].min.i = 0, [LVL_RAWSTR].max.i = 99999,
},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { 20, RIG_DBLST_END },
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
	{kHz(100),MHz(30),TT550_RXMODES,-1,-1,TT550_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
    {kHz(1800),MHz(2)-1,TT550_MODES,5000,100000,TT550_VFO},
    {kHz(3500),MHz(4)-1,TT550_MODES,5000,100000,TT550_VFO},
    {MHz(7),kHz(7300),TT550_MODES,5000,100000,TT550_VFO},
    {kHz(10100),kHz(10150),TT550_MODES,5000,100000,TT550_VFO},
    {MHz(14),kHz(14350),TT550_MODES,5000,100000,TT550_VFO},
    {kHz(18068),kHz(18168),TT550_MODES,5000,100000,TT550_VFO},
    {MHz(21),kHz(21450),TT550_MODES,5000,100000,TT550_VFO},
    {kHz(24890),kHz(24990),TT550_MODES,5000,100000,TT550_VFO},
    {MHz(28),kHz(29700),TT550_MODES,5000,100000,TT550_VFO},
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT550_RXMODES,1},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
				 /* FIXME: add increments -> 34 filters? */
		{RIG_MODE_SSB|RIG_MODE_RTTY, kHz(3)},
		{RIG_MODE_CW, 300},
		{RIG_MODE_AM|RIG_MODE_FM, kHz(8)},
		RIG_FLT_END,
	},
.str_cal = TT550_STR_CAL,

.rig_init =  tentec_init,
.rig_cleanup =  tentec_cleanup,
.rig_open =  tentec_trx_open,
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


