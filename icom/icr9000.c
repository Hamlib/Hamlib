/*
 *  Hamlib CI-V backend - IC-R9000 descriptions
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: icr9000.c,v 1.3 2004-09-26 08:35:03 fillods Exp $
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
#include "serial.h"
#include "misc.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


#define ICR9000_MODES (RIG_MODE_AM|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_CW|RIG_MODE_WFM)

#define ICR9000_OPS (RIG_OP_FROM_VFO|RIG_OP_MCL)

#define ICR9000_FUNCS (RIG_FUNC_VSC)
#define ICR9000_LEVELS (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR)
#define ICR9000_PARMS (RIG_PARM_ANN)
#define ICR9000_SCAN_OPS (RIG_SCAN_MEM)	/* TBC */

#define ICR9000_ANTS (RIG_ANT_1|RIG_ANT_2)


/* FIXME: S-Meter measurements */
#define ICR9000_STR_CAL	UNKNOWN_IC_STR_CAL

static const struct icom_priv_caps icr9000_priv_caps = {
	0x2a,   /* default address */
	0,      /* 731 mode */
	r9000_ts_sc_list
};
/*
 * ICR9000A rig capabilities.
 */
const struct rig_caps icr9000_caps = {
.rig_model =  RIG_MODEL_ICR9000,
.model_name = "IC-R9000",
.mfg_name =  "Icom",
.version =  BACKEND_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  1200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  ICR9000_FUNCS,
.has_set_func =  ICR9000_FUNCS,
.has_get_level =  ICR9000_LEVELS,
.has_set_level =  RIG_LEVEL_SET(ICR9000_LEVELS),
.has_get_parm =  ICR9000_PARMS,
.has_set_parm =  RIG_PARM_SET(ICR9000_PARMS), 
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { 10, 20, 30, RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  ICR9000_OPS,
.scan_ops =  ICR9000_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  { 
	{     1,   99, RIG_MTYPE_MEM },		/* TBC */
	{ 0x1000, 0x1009, RIG_MTYPE_EDGE },	/* 2 by 2 */
	{ 0x1010, 0x1019, RIG_MTYPE_EDGE },	/* 2 by 2 */
	RIG_CHAN_END, },

.rx_range_list1 =  {
	{kHz(100),MHz(1999.8),ICR9000_MODES,-1,-1, RIG_VFO_A, ICR9000_ANTS},
 	RIG_FRNG_END, },
.tx_range_list1 =  { RIG_FRNG_END, },

.rx_range_list2 =  {
	{kHz(100),MHz(1999.8),ICR9000_MODES,-1,-1, RIG_VFO_A,  ICR9000_ANTS},
 	RIG_FRNG_END, },
.tx_range_list2 =  { RIG_FRNG_END, },	/* no TX ranges, this is a receiver */

.tuning_steps =  {
	 {ICR9000_MODES,10},	/* resolution */
	 {ICR9000_MODES,100},
	 {ICR9000_MODES,kHz(1)},
	 {ICR9000_MODES,kHz(5)},
	 {ICR9000_MODES,kHz(9)},
	 {ICR9000_MODES,kHz(10)},
	 {ICR9000_MODES,12500},
	 {ICR9000_MODES,kHz(20)},
	 {ICR9000_MODES,kHz(25)},
	 {ICR9000_MODES,kHz(100)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_WFM, kHz(150)},
		RIG_FLT_END,
	},
.str_cal = ICR9000_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&icr9000_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,

.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
#if 0
.set_parm =  icom_set_parm,
.get_parm =  icom_get_parm,
#endif

.decode_event =  icom_decode_event,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.get_dcd =  icom_get_dcd,
};


/*
 * Function definitions below
 */


