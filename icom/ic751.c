/*
 *  Hamlib CI-V backend - description of IC-751 and variations
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ic751.c,v 1.2 2005-04-03 19:53:51 fillods Exp $
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
#include "bandplan.h"
#include "icom.h"


#define IC751_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)

/* 
 * 200W in all modes but AM (40W)
 */ 
#define IC751_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define IC751_AM_TX_MODES (RIG_MODE_AM)

#define IC751_VFO_ALL (RIG_VFO_A)

#define IC751_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO)

#define IC751_SCAN_OPS (RIG_SCAN_NONE)

#define IC751_ANTS RIG_ANT_1

/*
 */
static const struct icom_priv_caps ic751_priv_caps = { 
	0x1c,	/* default address */
	0,		/* 731 mode */
	ic737_ts_sc_list
};

const struct rig_caps ic751_caps = {
.rig_model =  RIG_MODEL_IC751,
.model_name = "IC-751", 
.mfg_name =  "Icom", 
.version =  BACKEND_VER, 
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  1200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE, 
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3, 
.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  RIG_FUNC_NONE, 
.has_get_level =  RIG_LEVEL_NONE,
.has_set_level =  RIG_LEVEL_NONE,
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC751_VFO_OPS,
.scan_ops =  IC751_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
	{   1,  26, RIG_MTYPE_MEM, IC_MIN_MEM_CAP },	/* TBC */
	RIG_CHAN_END,
	},

.rx_range_list1 =   {
	{kHz(100),MHz(30),IC751_ALL_RX_MODES,-1,-1,IC751_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =  {
    {MHz(1.8),MHz(1.99999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},	/* 100W class */
    {MHz(1.8),MHz(1.99999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},	/* 40W class */
    {MHz(3.45),MHz(4.09999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(3.45),MHz(4.09999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(6.95),MHz(7.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(6.95),MHz(7.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(9.95),MHz(10.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(9.95),MHz(10.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(13.95),MHz(14.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(13.95),MHz(14.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(17.95),MHz(18.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(17.95),MHz(18.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(20.95),MHz(21.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(20.95),MHz(21.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(24.45),MHz(25.09999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(24.45),MHz(25.09999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(27.95),MHz(30),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(27.95),MHz(30),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    RIG_FRNG_END, },

.rx_range_list2 =   {
	{kHz(100),MHz(30),IC751_ALL_RX_MODES,-1,-1,IC751_VFO_ALL},
	RIG_FRNG_END, },
	/* weird transmit ranges ... --sf */
.tx_range_list2 =   {
    {MHz(1.8),MHz(1.99999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},	/* 100W class */
    {MHz(1.8),MHz(1.99999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},	/* 40W class */
    {MHz(3.45),MHz(4.09999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(3.45),MHz(4.09999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(6.95),MHz(7.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(6.95),MHz(7.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(9.95),MHz(10.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(9.95),MHz(10.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(13.95),MHz(14.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(13.95),MHz(14.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(17.95),MHz(18.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(17.95),MHz(18.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(20.95),MHz(21.49999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(20.95),MHz(21.49999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(24.45),MHz(25.09999),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(24.45),MHz(25.09999),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    {MHz(27.95),MHz(30),IC751_OTHER_TX_MODES,W(10),W(200),IC751_VFO_ALL},
    {MHz(27.95),MHz(30),IC751_AM_TX_MODES,W(10),W(40),IC751_VFO_ALL},
    RIG_FRNG_END, },

.tuning_steps = 	{
	{IC751_ALL_RX_MODES,10},  /* basic resolution, there's no set_ts */
	RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
	{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.3)},
	{RIG_MODE_AM, kHz(6)},
	RIG_FLT_END,
	},

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic751_priv_caps,
.rig_init =	icom_init,
.rig_cleanup =	icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,

.decode_event =  icom_decode_event,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,

};

