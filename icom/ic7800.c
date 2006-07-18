/*
 *  Hamlib CI-V backend - description of IC-7800 and variations
 *  Copyright (c) 2004 by Stephane Fillod
 *
 *	$Id: ic7800.c,v 1.3 2006-07-18 22:51:42 n0nb Exp $
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
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "bandplan.h"

/*
 * TODO: PSK and PSKR
 */
#define IC7800_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC7800_1HZ_TS_MODES IC7800_ALL_RX_MODES
#define IC7800_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC7800_AM_TX_MODES (RIG_MODE_AM)

#define IC7800_FUNCS (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_RF|RIG_FUNC_ANF|RIG_FUNC_VSC|RIG_FUNC_LOCK)

#define IC7800_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_BALANCE|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_APF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC)

#define IC7800_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define IC7800_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT)

#define IC7800_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC7800_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_DELTA|RIG_SCAN_PRIO)

#define IC7800_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3|RIG_ANT_4)

/*
 * FIXME: real measures!
 */
#define IC7800_STR_CAL { 2, \
	{ \
		{   0, -60 }, \
		{ 255, 60 } \
	} }


/*
 * IC-7800 rig capabilities.
 *
 * TODO: complete command set (esp. the $1A bunch!) and testing..
 */
static const struct icom_priv_caps ic7800_priv_caps = { 
		0x6a,	/* default address */
		0,		/* 731 mode */
		ic756pro_ts_sc_list
};


const struct rig_caps ic7800_caps = {
.rig_model =  RIG_MODEL_IC7800,
.model_name = "IC-7800", 
.mfg_name =  "Icom", 
.version =  BACKEND_VER, 
.copyright =  "LGPL",
.status =  RIG_STATUS_NEW,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
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
.has_get_func =  IC7800_FUNCS,
.has_set_func =  IC7800_FUNCS, 
.has_get_level =  IC7800_LEVELS,
.has_set_level =  RIG_LEVEL_SET(IC7800_LEVELS),
.has_get_parm =  IC7800_PARMS,
.has_set_parm =  RIG_PARM_SET(IC7800_PARMS),	/* FIXME: parms */
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
},
.parm_gran =  {},
.ctcss_list =  common_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 10, 20, RIG_DBLST_END, },	/* FIXME: TBC */
.attenuator =   { 3, 6, 9, 12, 15, 18, 21, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(9999),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC7800_VFO_OPS,
.scan_ops =  IC7800_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
	   {   1,  99, RIG_MTYPE_MEM  },
	   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
	   RIG_CHAN_END,
	},

.rx_range_list1 =   { {kHz(30),MHz(60),IC7800_ALL_RX_MODES,-1,-1,IC7800_VFOS},
	RIG_FRNG_END, },
.tx_range_list1 =   {
	FRQ_RNG_HF(1,IC7800_OTHER_TX_MODES, W(5),W(200),IC7800_VFOS,IC7800_ANTS),
	FRQ_RNG_6m(1,IC7800_OTHER_TX_MODES, W(5),W(200),IC7800_VFOS,IC7800_ANTS),
	FRQ_RNG_HF(1,IC7800_AM_TX_MODES, W(5),W(50),IC7800_VFOS,IC7800_ANTS),   /* AM class */
	FRQ_RNG_6m(1,IC7800_AM_TX_MODES, W(5),W(50),IC7800_VFOS,IC7800_ANTS),   /* AM class */
    	RIG_FRNG_END, },

.rx_range_list2 =   { {kHz(30),MHz(60),IC7800_ALL_RX_MODES,-1,-1,IC7800_VFOS},
	RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC7800_OTHER_TX_MODES, W(5),W(200),IC7800_VFOS,IC7800_ANTS),
	FRQ_RNG_6m(2,IC7800_OTHER_TX_MODES, W(5),W(200),IC7800_VFOS,IC7800_ANTS),
	FRQ_RNG_HF(2,IC7800_AM_TX_MODES, W(5),W(50),IC7800_VFOS,IC7800_ANTS),   /* AM class */
	FRQ_RNG_6m(2,IC7800_AM_TX_MODES, W(5),W(50),IC7800_VFOS,IC7800_ANTS),   /* AM class */
    	RIG_FRNG_END, },

.tuning_steps = 	{
	 {IC7800_1HZ_TS_MODES,1},
	 {IC7800_ALL_RX_MODES,Hz(100)},
	 {IC7800_ALL_RX_MODES,kHz(1)},
	 {IC7800_ALL_RX_MODES,kHz(5)},
	 {IC7800_ALL_RX_MODES,kHz(9)},
	 {IC7800_ALL_RX_MODES,kHz(10)},
	 {IC7800_ALL_RX_MODES,kHz(12.5)},
	 {IC7800_ALL_RX_MODES,kHz(20)},
	 {IC7800_ALL_RX_MODES,kHz(25)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
	{RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2.4)},
	{RIG_MODE_CW|RIG_MODE_CWR, Hz(500)},
	{RIG_MODE_AM, kHz(6)},
	{RIG_MODE_AM, kHz(2.4)},
	{RIG_MODE_FM, kHz(15)},
	{RIG_MODE_FM, kHz(8)},
	RIG_FLT_END,
	},
.str_cal = IC7800_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic7800_priv_caps,
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

.set_rit =  icom_set_rit,

.decode_event =  icom_decode_event,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_parm =  icom_set_parm,
.get_parm =  icom_get_parm,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.set_ptt =  icom_set_ptt,
.get_ptt =  icom_get_ptt,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_ctcss_tone =  icom_set_ctcss_tone,
.get_ctcss_tone =  icom_get_ctcss_tone,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  icom_get_split_vfo,

};

