/*
 *  Hamlib CI-V backend - IC-R8500 description
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: icr8500.c,v 1.19 2003-12-08 08:33:58 fillods Exp $
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

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "icom.h"


#define ICR8500_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_WFM)
#define ICR8500_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define ICR8500_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_TSQL|RIG_FUNC_APF)

#define ICR8500_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_APF|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_RAWSTR)

#define ICR8500_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)

static const struct icom_priv_caps icr8500_priv_caps = {
	0x4a,   /* default address */
	0,      /* 731 mode */
	r8500_ts_sc_list
};
/*
 * ICR8500 rigs capabilities.
 */
const struct rig_caps icr8500_caps = {
.rig_model =  RIG_MODEL_ICR8500,
.model_name = "ICR-8500",
.mfg_name =  "Icom",
.version =  "0.2",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  ICR8500_FUNC_ALL,
.has_get_level =  ICR8500_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(ICR8500_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
},
.parm_gran =  {},
.ctcss_list =  NULL,	/* FIXME: CTCSS/DCS list */
.dcs_list =  NULL,
.preamp =   { 10, RIG_DBLST_END, },
.attenuator =   { 20, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  ICR8500_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   12,
.chan_desc_sz =  0,

.chan_list =  { RIG_CHAN_END, },	/* FIXME: memory channel list */

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },

.rx_range_list2 =  {
	{kHz(100),MHz(824)-10,ICR8500_MODES,-1,-1, RIG_VFO_A},
    {MHz(849)+10,MHz(869)-10,ICR8500_MODES,-1,-1, RIG_VFO_A},
    {MHz(894)+10,GHz(2)-10,ICR8500_MODES,-1,-1, RIG_VFO_A},
 	RIG_FRNG_END, },
.tx_range_list2 =  { RIG_FRNG_END, },	/* no TX ranges, this is a receiver */

.tuning_steps =  {
	 {ICR8500_MODES,10},
	 {ICR8500_MODES,50},
	 {ICR8500_MODES,100},
	 {ICR8500_MODES,kHz(1)},
	 {ICR8500_MODES,2500},
	 {ICR8500_MODES,kHz(5)},
	 {ICR8500_MODES,kHz(9)},
	 {ICR8500_MODES,kHz(10)},
	 {ICR8500_MODES,12500},
	 {ICR8500_MODES,kHz(20)},
	 {ICR8500_MODES,kHz(25)},
	 {ICR8500_MODES,kHz(100)},
	 {ICR8500_1MHZ_TS_MODES,MHz(1)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters =  {
			/* FIXME: To be confirmed! --SF */
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_AM, kHz(8)},
		{RIG_MODE_AM, kHz(2.4)},	/* narrow */
		{RIG_MODE_AM, kHz(15)},		/* wide */
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_FM, kHz(8)},	/* narrow */
		{RIG_MODE_WFM, kHz(230)},
		RIG_FLT_END,
	},

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&icr8500_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,

.decode_event =  icom_decode_event,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_func =  icom_set_func,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
};


/*
 * Function definitions below
 */


