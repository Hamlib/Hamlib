/*
 *  Hamlib CI-V backend - description of IC-7400 and variations
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: ic7400.c,v 1.1 2003-11-05 20:40:26 fillods Exp $
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
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "token.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "bandplan.h"
#include "misc.h"


/*
 * IC-7400
 *
 * TODO:
 * 	- advanced scanning functions
 * 	- set_ant 0x12,[1|2]
 * 	- set_channel
 * 	- set_ctcss_tone/ctcss_sql
 * 	- set keyer?
 * 	- read IF filter setting?
 * 	- switch main/sub?
 * 	- test all that stuff..
 */

#define IC7400_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC7400_1HZ_TS_MODES IC7400_ALL_RX_MODES

/* 
 * 100W in all modes but AM (40W)
 */ 
#define IC7400_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC7400_AM_TX_MODES (RIG_MODE_AM)

#define IC7400_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_RNF|RIG_FUNC_ANF|RIG_FUNC_APF)

#define IC7400_LEVEL_ALL (RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_BALANCE|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF|RIG_LEVEL_APF|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH)

#define IC7400_VFO_ALL (RIG_VFO_A|RIG_VFO_B)
#define IC7400_ANTS (RIG_ANT_1|RIG_ANT_2)

#define IC7400_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC7400_SCAN_OPS (RIG_SCAN_VFO|RIG_SCAN_MEM)

/*
 * FIXME: cloned from IC746
 */
#define IC7400_STR_CAL { 16, \
	{ \
		{   0, -18 }, \
		{  10, -16 }, \
		{  27, -14 }, \
		{  45, -12 }, \
		{  60, -10 }, \
		{  76, -8 }, \
		{  89, -6 }, \
		{ 100, -4 }, \
		{ 110, -2 }, \
		{ 120, 0 }, \
		{ 125, 2 }, \
		{ 129, 4 }, \
		{ 133, 6 }, \
		{ 138, 8 }, \
		{ 142, 10 }, \
		{ 146, 12 } \
	} }

/*
 * ic7400 rig capabilities.
 */
static const struct icom_priv_caps ic7400_priv_caps = { 
		0x66,	/* default address */
		0,		/* 731 mode */
		ic756pro_ts_sc_list,
		IC7400_STR_CAL	/* FIXME */
};

const struct rig_caps ic7400_caps = {
.rig_model =  RIG_MODEL_IC7400,
.model_name = "IC-7400", 
.mfg_name =  "Icom", 
.version =  "0.2", 
.copyright =  "LGPL",
.status =  RIG_STATUS_NEW,
.rig_type =  RIG_TYPE_TRANSCEIVER,
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
.has_get_func =  IC7400_FUNC_ALL,
.has_set_func =  IC7400_FUNC_ALL, 
.has_get_level =  IC7400_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(IC7400_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,	/* FIXME: parms */
.level_gran =  {}, 		/* granularity */
.parm_gran =  {},
.ctcss_list =  common_ctcss_list,
.dcs_list =  full_dcs_list,
.preamp =   { 10, 20, RIG_DBLST_END, },	/* FIXME: TBC */
.attenuator =   { 20, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC7400_VFO_OPS,
.scan_ops =  IC7400_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
			   {   1,  99, RIG_MTYPE_MEM  },
			   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
			   { 102, 102, RIG_MTYPE_CALL },
			   RIG_CHAN_END,
		},

.rx_range_list1 =   {
		{kHz(30),MHz(60),IC7400_ALL_RX_MODES,-1,-1,IC7400_VFO_ALL,IC7400_ANTS},
		{MHz(144),MHz(146),IC7400_ALL_RX_MODES,-1,-1,IC7400_VFO_ALL,IC7400_ANTS},
		RIG_FRNG_END, },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,IC7400_OTHER_TX_MODES, W(5),W(100),IC7400_VFO_ALL,IC7400_ANTS),
	FRQ_RNG_6m(1,IC7400_OTHER_TX_MODES, W(5),W(100),IC7400_VFO_ALL,IC7400_ANTS),
	FRQ_RNG_2m(1,IC7400_OTHER_TX_MODES, W(5),W(100),IC7400_VFO_ALL,IC7400_ANTS),
	FRQ_RNG_HF(1,IC7400_AM_TX_MODES, W(5),W(40),IC7400_VFO_ALL,IC7400_ANTS),   /* AM class */
	FRQ_RNG_6m(1,IC7400_AM_TX_MODES, W(5),W(40),IC7400_VFO_ALL,IC7400_ANTS),   /* AM class */
	FRQ_RNG_2m(1,IC7400_AM_TX_MODES, W(5),W(40),IC7400_VFO_ALL,IC7400_ANTS),   /* AM class */
	RIG_FRNG_END,
	},


.rx_range_list2 =   {
		{kHz(30),MHz(60),IC7400_ALL_RX_MODES,-1,-1,IC7400_VFO_ALL,IC7400_ANTS},
		{MHz(144),MHz(148),IC7400_ALL_RX_MODES,-1,-1,IC7400_VFO_ALL,IC7400_ANTS},
		RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC7400_OTHER_TX_MODES, W(5),W(100),IC7400_VFO_ALL,IC7400_ANTS),
	FRQ_RNG_6m(2,IC7400_OTHER_TX_MODES, W(5),W(100),IC7400_VFO_ALL,IC7400_ANTS),
	FRQ_RNG_2m(2,IC7400_OTHER_TX_MODES, W(5),W(100),IC7400_VFO_ALL,IC7400_ANTS),
	FRQ_RNG_HF(2,IC7400_AM_TX_MODES, W(5),W(40),IC7400_VFO_ALL,IC7400_ANTS),   /* AM class */
	FRQ_RNG_6m(2,IC7400_AM_TX_MODES, W(5),W(40),IC7400_VFO_ALL,IC7400_ANTS),   /* AM class */
	FRQ_RNG_2m(2,IC7400_AM_TX_MODES, W(5),W(40),IC7400_VFO_ALL,IC7400_ANTS),   /* AM class */
	RIG_FRNG_END,
	},

.tuning_steps = 	{
	 {IC7400_1HZ_TS_MODES,1},
	 {IC7400_ALL_RX_MODES,100},
	 {IC7400_ALL_RX_MODES,kHz(1)},
	 {IC7400_ALL_RX_MODES,kHz(5)},
	 {IC7400_ALL_RX_MODES,kHz(9)},
	 {IC7400_ALL_RX_MODES,kHz(10)},
	 {IC7400_ALL_RX_MODES,kHz(12.5)},
	 {IC7400_ALL_RX_MODES,kHz(20)},
	 {IC7400_ALL_RX_MODES,kHz(25)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
		{RIG_MODE_SSB|RIG_MODE_RTTYR|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_CW, kHz(2.4)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(350)},
		{RIG_MODE_CW|RIG_MODE_CWR, Hz(500)},
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)},
		RIG_FLT_END,
	},

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic7400_priv_caps,
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
.get_func =  icom_get_func,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  icom_get_split_vfo,

};

