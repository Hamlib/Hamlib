/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icall.c - Copyright (C) 2000,2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a virtual do-it-all Icom (for debug purpose)
 * using the "CI-V" interface.
 *
 *
 * 		$Id: icall.c,v 1.5 2001-06-03 19:54:05 f4cfe Exp $  
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "icom.h"


#define ICALL_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define ICALL_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM)
#define ICALL_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY)

/* tx doesn't have WFM.
 * 100W in all modes but AM (40W)
 */ 
#define ICALL_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define ICALL_AM_TX_MODES (RIG_MODE_AM)

#define ICALL_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)

#define ICALL_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

#define ICALL_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)

static const struct icom_priv_caps icall_priv_caps = {
	0x58,   /* whatever! */
	0,      /* 731 mode */
	ic706_ts_sc_list,
	EMPTY_STR_CAL
};

/*
 */
const struct rig_caps icall_caps = {
rig_model: RIG_MODEL_ICALL,
model_name:"IC-DoItAll",
mfg_name: "Icom",
version: "0.2",
copyright: "GPL",
status: RIG_STATUS_ALPHA,
rig_type: RIG_TYPE_TRANSCEIVER,
ptt_type: RIG_PTT_RIG,
dcd_type: RIG_DCD_RIG,
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

has_get_func: ICALL_FUNC_ALL,
has_set_func: ICALL_FUNC_ALL,
has_get_level: ICALL_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(ICALL_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,    /* FIXME: parms */
level_gran: {},                 /* granularity */
parm_gran: {},
ctcss_list: common_ctcss_list,
dcs_list: NULL,
preamp:  { 10, RIG_DBLST_END, },
attenuator:  { 20, RIG_DBLST_END, },
max_rit: Hz(9999),
max_xit: Hz(0),
max_ifshift: Hz(2.1),
targetable_vfo: 0,
vfo_ops: ICALL_OPS,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

  /* memory channel list */
chan_list: {
	{  01,  99, RIG_MTYPE_MEM, 0 },
    { 100, 105, RIG_MTYPE_EDGE, 0 },	/* two by two */
    { 106, 107, RIG_MTYPE_CALL, 0 },
    RIG_CHAN_END,
  },

rx_range_list1: { RIG_FRNG_END, },	/* FIXME: enter region 1 setting */
tx_range_list1: { RIG_FRNG_END, },
rx_range_list2: {
	{kHz(30),MHz(200)-1,ICALL_ALL_RX_MODES,-1,-1},	/* this trx also has UHF */
 	{MHz(400),MHz(470),ICALL_ALL_RX_MODES,-1,-1},
	RIG_FRNG_END, },
tx_range_list2: {
	{kHz(1800),MHz(2)-1,ICALL_OTHER_TX_MODES,5000,100000},	/* 100W class */
    {kHz(1800),MHz(2)-1,ICALL_AM_TX_MODES,2000,40000},	/* 40W class */
    {kHz(3500),MHz(4)-1,ICALL_OTHER_TX_MODES,5000,100000},
    {kHz(3500),MHz(4)-1,ICALL_AM_TX_MODES,2000,40000},
	{MHz(7),kHz(7300),ICALL_OTHER_TX_MODES,5000,100000},
    {MHz(7),kHz(7300),ICALL_AM_TX_MODES,2000,40000},
    {kHz(10100),kHz(10150),ICALL_OTHER_TX_MODES,5000,100000},
    {kHz(10100),kHz(10150),ICALL_AM_TX_MODES,2000,40000},
    {MHz(14),kHz(14350),ICALL_OTHER_TX_MODES,5000,100000},
    {MHz(14),kHz(14350),ICALL_AM_TX_MODES,2000,40000},
    {kHz(18068),kHz(18168),ICALL_OTHER_TX_MODES,5000,100000},
    {kHz(18068),kHz(18168),ICALL_AM_TX_MODES,2000,40000},
    {MHz(21),kHz(21450),ICALL_OTHER_TX_MODES,5000,100000},
    {MHz(21),kHz(21450),ICALL_AM_TX_MODES,2000,40000},
    {kHz(24890),kHz(24990),ICALL_OTHER_TX_MODES,5000,100000},
    {kHz(24890),kHz(24990),ICALL_AM_TX_MODES,2000,40000},
    {MHz(28),kHz(29700),ICALL_OTHER_TX_MODES,5000,100000},
    {MHz(28),kHz(29700),ICALL_AM_TX_MODES,2000,40000},
    {MHz(50),MHz(54),ICALL_OTHER_TX_MODES,5000,100000},
    {MHz(50),MHz(54),ICALL_AM_TX_MODES,2000,40000},
    {MHz(144),MHz(148),ICALL_OTHER_TX_MODES,5000,50000}, /* 50W */
    {MHz(144),MHz(148),ICALL_AM_TX_MODES,2000,20000}, /* AM VHF is 20W */
    {MHz(430),MHz(450),ICALL_OTHER_TX_MODES,5000,20000},
    {MHz(430),MHz(450),ICALL_AM_TX_MODES,2000,8000},
	RIG_FRNG_END, },
tuning_steps: {
	 {ICALL_1HZ_TS_MODES,1},
	 {ICALL_ALL_RX_MODES,10},
	 {ICALL_ALL_RX_MODES,100},
	 {ICALL_ALL_RX_MODES,kHz(1)},
	 {ICALL_ALL_RX_MODES,kHz(5)},
	 {ICALL_ALL_RX_MODES,kHz(9)},
	 {ICALL_ALL_RX_MODES,kHz(10)},
	 {ICALL_ALL_RX_MODES,12500},
	 {ICALL_ALL_RX_MODES,kHz(20)},
	 {ICALL_ALL_RX_MODES,kHz(25)},
	 {ICALL_ALL_RX_MODES,kHz(100)},
	 {ICALL_1MHZ_TS_MODES,MHz(1)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
filters: {
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.4)},	/* bultin FL-272 */
		{RIG_MODE_AM, kHz(8)},		/* mid w/ bultin FL-94 */
		{RIG_MODE_AM, kHz(2.4)},	/* narrow w/ bultin FL-272 */
		{RIG_MODE_FM, kHz(15)},		/* ?? TBC, mid w/ bultin FL-23+SFPC455E */
		{RIG_MODE_FM, kHz(8)},		/* narrow w/ bultin FL-94 */
		{RIG_MODE_WFM, kHz(230)},	/* WideFM, filter FL?? */
		RIG_FLT_END,
	},
priv: (void*)&icall_priv_caps,
rig_init: icom_init,
rig_cleanup: icom_cleanup,
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
set_channel: icom_set_channel,
get_channel: icom_get_channel,
set_mem: icom_set_mem,
#ifdef WANT_OLD_VFO_TO_BE_REMOVED
mv_ctl: icom_mv_ctl,
#else
vfo_op: icom_vfo_op,
#endif
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
set_ctcss: icom_set_ctcss,
get_ctcss: icom_get_ctcss,
set_ctcss_sql: icom_set_ctcss_sql,
get_ctcss_sql: icom_get_ctcss_sql,
};


/*
 * Function definitions below
 */
