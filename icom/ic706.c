/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ic706.c - Copyright (C) 2000,2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an IC-706,IC-706MKII,IC706-MKIIG
 * using the "CI-V" interface.
 *
 *
 * $Id: ic706.c,v 1.23 2001-06-26 20:55:29 f4cfe Exp $  
 *
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


#define IC706_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC706_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM)
#define IC706_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY)

/* tx doesn't have WFM.
 * 100W in all modes but AM (40W)
 */ 
#define IC706_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC706_AM_TX_MODES (RIG_MODE_AM)

#define IC706_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)

#define IC706_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

#define IC706_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

#define IC706_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC706_SCAN_OPS (RIG_SCAN_MEM)

#define IC706IIG_STR_CAL { 16, \
	{ \
		{ 100, -18 }, \
		{ 104, -16 }, \
		{ 108, -14 }, \
		{ 111, -12 }, \
		{ 114, -10 }, \
		{ 118, -8 }, \
		{ 121, -6 }, \
		{ 125, -4 }, \
		{ 129, -2 }, \
		{ 133, 0 }, \
		{ 137, 2 }, \
		{ 142, 4 }, \
		{ 146, 6 }, \
		{ 151, 8 }, \
		{ 156, 10 }, \
		{ 161, 12 } \
	} }


/*
 * ic706 rigs capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
static const struct icom_priv_caps ic706_priv_caps = { 
		0x48,	/* default address */
		0,		/* 731 mode */
		ic706_ts_sc_list,
		IC706IIG_STR_CAL
};

const struct rig_caps ic706_caps = {
rig_model: RIG_MODEL_IC706,
model_name:"IC-706", 
mfg_name: "Icom", 
version: "0.2", 
copyright: "GPL",
status: RIG_STATUS_UNTESTED,
rig_type: RIG_TYPE_MOBILE,
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
has_get_func: IC706_FUNC_ALL,
has_set_func: IC706_FUNC_ALL, 
has_get_level: IC706_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(IC706_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,	/* FIXME: parms */
level_gran: {}, 		/* granularity */
parm_gran: {},
ctcss_list: NULL,
dcs_list: NULL,
preamp:  { 10, RIG_DBLST_END, },
attenuator:  { 20, RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: 0,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: { RIG_CHAN_END, },	/* FIXME: memory channel list */

rx_range_list1:  { RIG_FRNG_END, },	/* FIXME: enter region 1 setting */
tx_range_list1:  { RIG_FRNG_END, },
rx_range_list2:  { {kHz(30),199999999,IC706_ALL_RX_MODES,-1,-1,IC706_VFO_ALL},RIG_FRNG_END, }, /* rx range */
tx_range_list2:  { {kHz(1800),1999999,IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},	/* 100W class */
    {kHz(1800),1999999,IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},	/* 40W class */
    {kHz(3500),3999999,IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(3500),3999999,IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(7),kHz(7300),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(7),kHz(7300),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(10100),kHz(10150),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(10100),kHz(10150),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(14),kHz(14350),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(14),kHz(14350),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(18068),kHz(18168),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(18068),kHz(18168),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(21),kHz(21450),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(21),kHz(21450),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(24890),kHz(24990),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(24890),kHz(24990),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(28),kHz(29700),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(28),kHz(29700),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(50),MHz(54),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(50),MHz(54),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(144),MHz(148),IC706_OTHER_TX_MODES,5000,20000,IC706_VFO_ALL}, /* not sure.. */
    {MHz(144),MHz(148),IC706_AM_TX_MODES,2000,8000,IC706_VFO_ALL}, /* anyone? */
	RIG_FRNG_END, },

tuning_steps:	{{IC706_1HZ_TS_MODES,1},
	 {IC706_ALL_RX_MODES,10},
	 {IC706_ALL_RX_MODES,100},
	 {IC706_ALL_RX_MODES,kHz(1)},
	 {IC706_ALL_RX_MODES,kHz(5)},
	 {IC706_ALL_RX_MODES,kHz(9)},
	 {IC706_ALL_RX_MODES,kHz(10)},
	 {IC706_ALL_RX_MODES,12500},
	 {IC706_ALL_RX_MODES,kHz(20)},
	 {IC706_ALL_RX_MODES,kHz(25)},
	 {IC706_ALL_RX_MODES,kHz(100)},
	 {IC706_1MHZ_TS_MODES,MHz(1)},
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
priv: (void*)&ic706_priv_caps,
rig_init:  icom_init,
rig_cleanup:  icom_cleanup,
rig_open: NULL,
rig_close: NULL,

set_freq: icom_set_freq,
get_freq: icom_get_freq,
set_mode: icom_set_mode,
get_mode: icom_get_mode,
set_vfo: icom_set_vfo,

};


static const struct icom_priv_caps ic706mkii_priv_caps = { 
		0x4e,	/* default address */
		0,		/* 731 mode */
		ic706_ts_sc_list,
		IC706IIG_STR_CAL
};

const struct rig_caps ic706mkii_caps = {
rig_model: RIG_MODEL_IC706MKII,
model_name:"IC-706MkII", 
mfg_name: "Icom", 
version: "0.2", 
copyright: "GPL",
status: RIG_STATUS_UNTESTED,
rig_type: RIG_TYPE_MOBILE,
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
has_get_func: IC706_FUNC_ALL,
has_set_func: IC706_FUNC_ALL, 
has_get_level: IC706_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(IC706_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,	/* FIXME: parms */
level_gran: {}, 		/* granularity */
parm_gran: {},
ctcss_list: NULL,
dcs_list: NULL,
preamp:  { 10, RIG_DBLST_END, },
attenuator:  { 20, RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: 0,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: { RIG_CHAN_END, },	/* FIXME: memory channel list */

rx_range_list1:  { RIG_FRNG_END, },	/* FIXME: enter region 1 setting */
tx_range_list1:  { RIG_FRNG_END, },
rx_range_list2:  { {kHz(30),199999999,IC706_ALL_RX_MODES,-1,-1,IC706_VFO_ALL},
						 RIG_FRNG_END, }, /* rx range */
tx_range_list2:  { {kHz(1800),1999999,IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},	/* 100W class */
    {kHz(1800),1999999,IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},	/* 40W class */
    {kHz(3500),3999999,IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(3500),3999999,IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(7),kHz(7300),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(7),kHz(7300),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(10100),kHz(10150),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(10100),kHz(10150),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(14),kHz(14350),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(14),kHz(14350),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(18068),kHz(18168),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(18068),kHz(18168),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(21),kHz(21450),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(21),kHz(21450),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(24890),kHz(24990),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(24890),kHz(24990),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(28),kHz(29700),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(28),kHz(29700),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(50),MHz(54),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(50),MHz(54),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(144),MHz(148),IC706_OTHER_TX_MODES,5000,20000,IC706_VFO_ALL}, /* not sure.. */
    {MHz(144),MHz(148),IC706_AM_TX_MODES,2000,8000,IC706_VFO_ALL}, /* anyone? */
	RIG_FRNG_END, },

tuning_steps:	{
	 {IC706_1HZ_TS_MODES,1},
	 {IC706_ALL_RX_MODES,10},
	 {IC706_ALL_RX_MODES,100},
	 {IC706_ALL_RX_MODES,kHz(1)},
	 {IC706_ALL_RX_MODES,kHz(5)},
	 {IC706_ALL_RX_MODES,kHz(9)},
	 {IC706_ALL_RX_MODES,kHz(10)},
	 {IC706_ALL_RX_MODES,12500},
	 {IC706_ALL_RX_MODES,kHz(20)},
	 {IC706_ALL_RX_MODES,kHz(25)},
	 {IC706_ALL_RX_MODES,kHz(100)},
	 {IC706_1MHZ_TS_MODES,MHz(1)},
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

priv: (void*)&ic706mkii_priv_caps,
rig_init:  icom_init,
rig_cleanup:  icom_cleanup,
rig_open: NULL,
rig_close: NULL,

set_freq: icom_set_freq,
get_freq: icom_get_freq,
set_mode: icom_set_mode,
get_mode: icom_get_mode,
set_vfo: icom_set_vfo,
};

/*
 * Basically, the IC706MKIIG is an IC706MKII plus UHF, a DSP
 * and 50W VHF
 */
static const struct icom_priv_caps ic706mkiig_priv_caps = { 
		0x58,	/* default address */
		0,		/* 731 mode */
		ic706_ts_sc_list,
		IC706IIG_STR_CAL
};

const struct rig_caps ic706mkiig_caps = {
rig_model: RIG_MODEL_IC706MKIIG,
model_name:"IC-706MkIIG", 
mfg_name: "Icom", 
version: "0.2", 
copyright: "GPL",
status: RIG_STATUS_ALPHA,
rig_type: RIG_TYPE_MOBILE,
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
has_get_func: IC706_FUNC_ALL,
has_set_func: IC706_FUNC_ALL, 
has_get_level: IC706_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(IC706_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,	/* FIXME: parms */
level_gran: {}, 		/* granularity */
parm_gran: {},
ctcss_list: common_ctcss_list,
dcs_list: NULL,
preamp:  { 10, RIG_DBLST_END, },
attenuator:  { 20, RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: 0,
vfo_ops: IC706_VFO_OPS,
scan_ops: IC706_SCAN_OPS,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: {
				   {  01,  99, RIG_MTYPE_MEM, 0 },
				   { 100, 105, RIG_MTYPE_EDGE, 0 },    /* two by two */
				   { 106, 107, RIG_MTYPE_CALL, 0 },
				   RIG_CHAN_END,
		},

rx_range_list1:  { RIG_FRNG_END, },	/* FIXME: enter region 1 setting */
tx_range_list1:  { RIG_FRNG_END, },

rx_range_list2:  { {kHz(30),MHz(200)-1,IC706_ALL_RX_MODES,-1,-1,IC706_VFO_ALL},	/* this trx also has UHF */
 	{MHz(400),MHz(470),IC706_ALL_RX_MODES,-1,-1,IC706_VFO_ALL},
	RIG_FRNG_END, },
tx_range_list2: { {kHz(1800),MHz(2)-1,IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},	/* 100W class */
    {kHz(1800),MHz(2)-1,IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},	/* 40W class */
    {kHz(3500),MHz(4)-1,IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(3500),MHz(4)-1,IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
	{MHz(7),kHz(7300),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(7),kHz(7300),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(10100),kHz(10150),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(10100),kHz(10150),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(14),kHz(14350),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(14),kHz(14350),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(18068),kHz(18168),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(18068),kHz(18168),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(21),kHz(21450),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(21),kHz(21450),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {kHz(24890),kHz(24990),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {kHz(24890),kHz(24990),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(28),kHz(29700),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(28),kHz(29700),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(50),MHz(54),IC706_OTHER_TX_MODES,5000,100000,IC706_VFO_ALL},
    {MHz(50),MHz(54),IC706_AM_TX_MODES,2000,40000,IC706_VFO_ALL},
    {MHz(144),MHz(148),IC706_OTHER_TX_MODES,5000,50000,IC706_VFO_ALL}, /* 50W */
    {MHz(144),MHz(148),IC706_AM_TX_MODES,2000,20000,IC706_VFO_ALL}, /* AM VHF is 20W */
    {MHz(430),MHz(450),IC706_OTHER_TX_MODES,5000,20000,IC706_VFO_ALL},
    {MHz(430),MHz(450),IC706_AM_TX_MODES,2000,8000,IC706_VFO_ALL},
	RIG_FRNG_END, },

tuning_steps:	{
	 {IC706_1HZ_TS_MODES,1},
	 {IC706_ALL_RX_MODES,10},
	 {IC706_ALL_RX_MODES,100},
	 {IC706_ALL_RX_MODES,kHz(1)},
	 {IC706_ALL_RX_MODES,kHz(5)},
	 {IC706_ALL_RX_MODES,kHz(9)},
	 {IC706_ALL_RX_MODES,kHz(10)},
	 {IC706_ALL_RX_MODES,12500},
	 {IC706_ALL_RX_MODES,kHz(20)},
	 {IC706_ALL_RX_MODES,kHz(25)},
	 {IC706_ALL_RX_MODES,kHz(100)},
	 {IC706_1MHZ_TS_MODES,MHz(1)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
filters:	{
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.4)},	/* bultin FL-272 */
		{RIG_MODE_AM, kHz(8)},		/* mid w/ bultin FL-94 */
		{RIG_MODE_AM, kHz(2.4)},	/* narrow w/ bultin FL-272 */
		{RIG_MODE_FM, kHz(15)},		/* ?? TBC, mid w/ bultin FL-23+SFPC455E */
		{RIG_MODE_FM, kHz(8)},		/* narrow w/ bultin FL-94 */
		{RIG_MODE_WFM, kHz(230)},	/* WideFM, filter FL?? */
		RIG_FLT_END,
	},

priv: (void*)&ic706mkiig_priv_caps,
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
#ifdef WANT_OLD_VFO_TO_BE_REMOVED
mv_ctl: icom_mv_ctl,
#else
vfo_op: icom_vfo_op,
#endif
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


