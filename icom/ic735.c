/*
 *  Hamlib CI-V backend - description of IC-735 and variations
 *  Copyright (c) 2000,2001 by Stephane Fillod
 *
 *		$Id: ic735.c,v 1.1 2001-09-17 05:45:06 f4cfe Exp $
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


#define IC735_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC735_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM)
#define IC735_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)

/* 
 * 100W in all modes but AM (40W)
 */ 
#define IC735_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC735_AM_TX_MODES (RIG_MODE_AM)

#define IC735_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

#define IC735_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO)

#define IC735_STR_CAL { 0, { } }

/*
 */
static const struct icom_priv_caps ic735_priv_caps = { 
		0x04,	/* default address */
		1,		/* 731 mode */
		ic737_ts_sc_list,
		IC735_STR_CAL
};

const struct rig_caps ic735_caps = {
rig_model: RIG_MODEL_IC735,
model_name:"IC-735", 
mfg_name: "Icom", 
version: "0.2", 
copyright: "LGPL",
status: RIG_STATUS_UNTESTED,
rig_type: RIG_TYPE_TRANSCEIVER,
ptt_type: RIG_PTT_NONE,
dcd_type: RIG_DCD_NONE,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 1200,
serial_rate_max: 1200,
serial_data_bits: 8,
serial_stop_bits: 1,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE, 
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3, 
has_get_func: RIG_FUNC_NONE,
has_set_func: RIG_FUNC_NONE, 
has_get_level: RIG_LEVEL_NONE,
has_set_level: RIG_LEVEL_NONE,
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,
level_gran: {},
parm_gran: {},
ctcss_list: NULL,
dcs_list: NULL,
preamp:  { RIG_DBLST_END, },
attenuator:  { RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: 0,
vfo_ops: IC735_VFO_OPS,
scan_ops: RIG_SCAN_NONE,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: {
				   {   1,  10, RIG_MTYPE_MEM,   0 },
				   {  11,  12, RIG_MTYPE_EDGE,  0 },
				   RIG_CHAN_END,
		},

rx_range_list1:  { RIG_FRNG_END, },	/* FIXME: enter region 1 setting */
tx_range_list1:  { RIG_FRNG_END, },

rx_range_list2:  { {kHz(30),MHz(30),IC735_ALL_RX_MODES,-1,-1,IC735_VFO_ALL},
	RIG_FRNG_END, },
tx_range_list2: { {kHz(1800),MHz(2)-1,IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},	/* 100W class */
    {kHz(1800),MHz(2)-1,IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},	/* 40W class */
    {kHz(3500),MHz(4)-1,IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {kHz(3500),MHz(4)-1,IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
	{MHz(7),kHz(7300),IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {MHz(7),kHz(7300),IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
    {kHz(10100),kHz(10150),IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {kHz(10100),kHz(10150),IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
    {MHz(14),kHz(14350),IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {MHz(14),kHz(14350),IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
    {kHz(18068),kHz(18168),IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {kHz(18068),kHz(18168),IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
    {MHz(21),kHz(21450),IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {MHz(21),kHz(21450),IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
    {kHz(24890),kHz(24990),IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {kHz(24890),kHz(24990),IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
    {MHz(28),kHz(29700),IC735_OTHER_TX_MODES,5000,100000,IC735_VFO_ALL},
    {MHz(28),kHz(29700),IC735_AM_TX_MODES,2000,40000,IC735_VFO_ALL},
	RIG_FRNG_END, },

tuning_steps:	{
	 {IC735_1HZ_TS_MODES,1},
	 {IC735_ALL_RX_MODES,10},
	 {IC735_ALL_RX_MODES,100},
	 {IC735_ALL_RX_MODES,kHz(1)},
	 {IC735_ALL_RX_MODES,kHz(5)},
	 {IC735_ALL_RX_MODES,kHz(9)},
	 {IC735_ALL_RX_MODES,kHz(10)},
	 {IC735_ALL_RX_MODES,12500},
	 {IC735_ALL_RX_MODES,kHz(20)},
	 {IC735_ALL_RX_MODES,kHz(25)},
	 {IC735_ALL_RX_MODES,kHz(100)},
	 {IC735_1MHZ_TS_MODES,MHz(1)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
filters:	{
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.4)},	/* bultin FL-272 */
		{RIG_MODE_AM, kHz(8)},		/* mid w/ bultin FL-94 */
		{RIG_MODE_AM, kHz(2.4)},	/* narrow w/ bultin FL-272 */
		{RIG_MODE_FM, kHz(15)},		/* ?? TBC, mid w/ bultin FL-23+SFPC455E */
		{RIG_MODE_FM, kHz(8)},		/* narrow w/ bultin FL-94 */
		RIG_FLT_END,
	},

cfgparams: icom_cfg_params,
set_conf: icom_set_conf,
get_conf: icom_get_conf,

priv: (void*)&ic735_priv_caps,
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
set_mem: icom_set_mem,
vfo_op: icom_vfo_op,

};

