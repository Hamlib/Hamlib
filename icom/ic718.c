/*
 *  Hamlib CI-V backend - description of IC-718 caps
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *  Caps submitted by Chuck Gilkes WD0FCL/4
 *
 *		$Id: ic718.c,v 1.1 2002-03-05 23:26:56 fillods Exp $
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
#include "icom.h"


#define IC718_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define IC718_1MHZ_TS_MODES (RIG_MODE_AM)
#define IC718_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)

/* tx doesn't have WFM.
 * 100W in all modes but AM (40W)
 */ 
#define IC718_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define IC718_AM_TX_MODES (RIG_MODE_AM)

#define IC718_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)

#define IC718_LEVEL_ALL (RIG_LEVEL_MICGAIN|RIG_LEVEL_NR|RIG_LEVEL_CWPITCH|RIG_LEVEL_KEYSPD|RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

#define IC718_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

#define IC718_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC718_SCAN_OPS (RIG_SCAN_MEM)

#define IC718_STR_CAL { 0, { } }



static const struct icom_priv_caps IC718_priv_caps = { 
		0x5e,	/* default address */
		0,		/* 731 mode */
		ic718_ts_sc_list,
		IC718_STR_CAL
};

const struct rig_caps ic718_caps = {
rig_model: RIG_MODEL_IC718,
model_name:"IC-718", 
mfg_name: "Icom", 
version: "0.1", 
copyright: "LGPL",
status: RIG_STATUS_ALPHA,
rig_type:  RIG_TYPE_TRANSCEIVER,
ptt_type: RIG_PTT_NONE,
dcd_type: RIG_DCD_NONE,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 300,
serial_rate_max: 19200,
serial_data_bits: 8,
serial_stop_bits: 1,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE, 
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3, 
has_get_func: IC718_FUNC_ALL,
has_set_func: IC718_FUNC_ALL, 
has_get_level: IC718_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(IC718_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,	/* FIXME: parms */
level_gran: {}, 		/* granularity */
parm_gran: {},
ctcss_list: common_ctcss_list,
dcs_list: NULL,
preamp:  { 10, RIG_DBLST_END, },
attenuator:  { 20, RIG_DBLST_END, },
max_rit: Hz(1200),
max_xit: Hz(0),
max_ifshift: Hz(1200),
targetable_vfo: 0,
vfo_ops: IC718_VFO_OPS,
scan_ops: IC718_SCAN_OPS,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: {
				   {   1,  99, RIG_MTYPE_MEM, 0 },
				   { 100, 101, RIG_MTYPE_EDGE, 0 },    /* two by two */
				     RIG_CHAN_END,
		},

rx_range_list1:  { RIG_FRNG_END, },	/* FIXME: enter region 1 setting */
tx_range_list1:  { RIG_FRNG_END, },

rx_range_list2:  { {kHz(30),MHz(200)-1,IC718_ALL_RX_MODES,-1,-1,IC718_VFO_ALL},	/* this trx also has UHF */
 		 {MHz(400),MHz(470),IC718_ALL_RX_MODES,-1,-1,IC718_VFO_ALL},
		 RIG_FRNG_END, },
tx_range_list2: { {kHz(1800),MHz(2)-1,IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},	/* 100W class */
    		{kHz(1800),MHz(2)-1,IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},	/* 40W class */
    		{kHz(3500),MHz(4)-1,IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
    		{kHz(3500),MHz(4)-1,IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
		{MHz(7),kHz(7300),IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
   		{MHz(7),kHz(7300),IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
    		{kHz(10100),kHz(10150),IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
    		{kHz(10100),kHz(10150),IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
    		{MHz(14),kHz(14350),IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
    		{MHz(14),kHz(14350),IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
   		{kHz(18068),kHz(18168),IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
   		{kHz(18068),kHz(18168),IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
    		{MHz(21),kHz(21450),IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
    		{MHz(21),kHz(21450),IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
    		{kHz(24890),kHz(24990),IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
    		{kHz(24890),kHz(24990),IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
    		{MHz(28),kHz(29700),IC718_OTHER_TX_MODES,5000,100000,IC718_VFO_ALL},
    		{MHz(28),kHz(29700),IC718_AM_TX_MODES,2000,40000,IC718_VFO_ALL},
		RIG_FRNG_END, },

tuning_steps:	{
	 {IC718_1HZ_TS_MODES,1},
	 {IC718_ALL_RX_MODES,10},
	 {IC718_ALL_RX_MODES,100},
	 {IC718_ALL_RX_MODES,kHz(1)},
	 {IC718_ALL_RX_MODES,kHz(5)},
	 {IC718_ALL_RX_MODES,kHz(9)},
	 {IC718_ALL_RX_MODES,kHz(10)},
	 {IC718_ALL_RX_MODES,kHz(100)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
filters:	{
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.1)},	/* bultin */
		{RIG_MODE_CW|RIG_MODE_RTTY, Hz(500)},			/* FL-52A */
		{RIG_MODE_CW|RIG_MODE_RTTY, Hz(250)},			/* FL-53A */
		{RIG_MODE_SSB, kHz(2.8)},				/* FL-96  */
		{RIG_MODE_SSB, kHz(1.8)},				/* FL-222 */
		{RIG_MODE_AM, kHz(6)},					/* mid w/ bultin FL-94 */
		{RIG_MODE_AM, kHz(2.4)},				/* narrow w/ bultin FL-272 */
		RIG_FLT_END,		
	},

cfgparams: icom_cfg_params,
set_conf: icom_set_conf,
get_conf: icom_get_conf,

priv: (void*)&IC718_priv_caps,
rig_init:  icom_init,
rig_cleanup:  icom_cleanup,
rig_open: NULL,
rig_close: NULL,

set_freq: icom_set_freq,
get_freq: icom_get_freq,
set_mode: icom_set_mode,
get_mode: icom_get_mode,
set_vfo: icom_set_vfo,

decode_event: icom_decode_event,
set_level: icom_set_level,
get_level: icom_get_level,
set_func: icom_set_func,
get_func: icom_get_func,
set_mem: icom_set_mem,
vfo_op: icom_vfo_op,
scan: icom_scan,
set_ptt: icom_set_ptt,
get_ptt: icom_get_ptt,
get_dcd: icom_get_dcd,
set_ts: icom_set_ts,
get_ts: icom_get_ts,
set_rptr_shift: icom_set_rptr_shift,
get_rptr_shift: icom_get_rptr_shift,
set_rptr_offs: icom_set_rptr_offs,
get_rptr_offs: icom_get_rptr_offs,
set_split_freq: icom_set_split_freq,
get_split_freq: icom_get_split_freq,
set_split_mode: icom_set_split_mode,
get_split_mode: icom_get_split_mode,
set_split: icom_set_split,
get_split: icom_get_split,

};


