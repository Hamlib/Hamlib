/*
 *  Hamlib CI-V backend - description of the TenTenc OMNI VI
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *		$Id: omni.c,v 1.1 2002-05-30 17:38:07 fillods Exp $
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



#define OMNIVIP_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

#define OMNIVIP_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define OMNIVIP_AM_TX_MODES (RIG_MODE_AM)

#define OMNIVIP_ALL_RX_MODES (OMNIVIP_OTHER_TX_MODES|OMNIVIP_AM_TX_MODES)

#define OMNIVIP_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO)
#define OMNIVIP_STR_CAL { 0, { } }


#define OMNIVIP_STR_CAL { 0, { \
		} }		/* TBC */

/*
 * Specs from http://www.tentec.com/TT564.htm
 */

static const struct icom_priv_caps omnivip_priv_caps = { 
		0x80,	/* default address */
		0,		/* 731 mode */
		NULL,
		OMNIVIP_STR_CAL
};

const struct rig_caps omnivip_caps = {
rig_model: RIG_MODEL_OMNIVIP,
model_name:"Omni VI Plus", 
mfg_name: "Ten-Tec", 
version: "0.1", 
copyright: "LGPL",
status: RIG_STATUS_UNTESTED,
rig_type: RIG_TYPE_TRANSCEIVER,
ptt_type: RIG_PTT_NONE,
dcd_type: RIG_DCD_NONE,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 1200,
serial_rate_max: 19200,
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
preamp:  { RIG_DBLST_END, },
attenuator:  { RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: 0,
vfo_ops: OMNIVIP_VFO_OPS,
scan_ops: RIG_SCAN_NONE,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: { RIG_CHAN_END, },

rx_range_list1:  {
	RIG_FRNG_END, },
tx_range_list1: { RIG_FRNG_END, },		/* this is a scanner */


rx_range_list2:  { {kHz(30),MHz(30),OMNIVIP_ALL_RX_MODES,-1,-1,OMNIVIP_VFO_ALL},
	RIG_FRNG_END, },
tx_range_list2: { {kHz(1800),MHz(2)-1,OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},	/* 100W class */
    {kHz(1800),MHz(2)-1,OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},	/* 40W class */
    {kHz(3500),MHz(4)-1,OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {kHz(3500),MHz(4)-1,OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
	{MHz(7),kHz(7300),OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {MHz(7),kHz(7300),OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
    {kHz(10100),kHz(10150),OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {kHz(10100),kHz(10150),OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
    {MHz(14),kHz(14350),OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {MHz(14),kHz(14350),OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
    {kHz(18068),kHz(18168),OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {kHz(18068),kHz(18168),OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
    {MHz(21),kHz(21450),OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {MHz(21),kHz(21450),OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
    {kHz(24890),kHz(24990),OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {kHz(24890),kHz(24990),OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
    {MHz(28),kHz(29700),OMNIVIP_OTHER_TX_MODES,5000,100000,OMNIVIP_VFO_ALL},
    {MHz(28),kHz(29700),OMNIVIP_AM_TX_MODES,2000,40000,OMNIVIP_VFO_ALL},
	RIG_FRNG_END, },

tuning_steps:	{
	 {OMNIVIP_ALL_RX_MODES,1},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
filters:	{
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.4)},
		{RIG_MODE_AM|RIG_MODE_FM, kHz(8)},
		RIG_FLT_END,
	},

cfgparams: icom_cfg_params,
set_conf: icom_set_conf,
get_conf: icom_get_conf,

priv: (void*)&omnivip_priv_caps,
rig_init:  icom_init,
rig_cleanup:  icom_cleanup,

set_freq: icom_set_freq,
get_freq: icom_get_freq,
set_mode: icom_set_mode,
get_mode: icom_get_mode,
set_vfo: icom_set_vfo,

decode_event: icom_decode_event,
set_mem: icom_set_mem,
vfo_op: icom_vfo_op,

};



