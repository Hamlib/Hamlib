/*
 *  Hamlib CI-V backend - description of IC-725 and variations
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: ic725.c,v 1.1 2002-12-23 14:20:42 fillods Exp $
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
#include <bandplan.h>
#include "icom.h"


#define IC725_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

/* 
 * 100W in all modes but AM (40W)
 */ 
#define IC725_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define IC725_AM_TX_MODES (RIG_MODE_AM)

#define IC725_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

#define IC725_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_CPY|RIG_OP_MCL)

#define IC725_SCAN_OPS (RIG_SCAN_VFO|RIG_SCAN_MEM)	/* TBC */

#define IC725_ANTS RIG_ANT_1

#define IC725_STR_CAL { 0, { } }

/*
 */
static const struct icom_priv_caps ic725_priv_caps = { 
	0x28,	/* default address */
	0,		/* 731 mode */
	ic737_ts_sc_list,
	IC725_STR_CAL
};

const struct rig_caps ic725_caps = {
.rig_model =  RIG_MODEL_IC725,
.model_name = "IC-725", 
.mfg_name =  "Icom", 
.version =  "0.2", 
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
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
.vfo_ops =  IC725_VFO_OPS,
.scan_ops =  IC725_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
	{   1,  26, RIG_MTYPE_MEM },
	RIG_CHAN_END,
	},

.rx_range_list1 =   {
	{kHz(30),MHz(33),IC725_ALL_RX_MODES,-1,-1,IC725_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =  {
	FRQ_RNG_HF(2,IC725_OTHER_TX_MODES, W(10),W(100),IC725_VFO_ALL,IC725_ANTS),
	FRQ_RNG_HF(2,IC725_AM_TX_MODES, W(10),W(40),IC725_VFO_ALL,IC725_ANTS),  /* AM class */
	RIG_FRNG_END, },

.rx_range_list2 =   {
	{kHz(30),MHz(33),IC725_ALL_RX_MODES,-1,-1,IC725_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC725_OTHER_TX_MODES, W(10),W(100),IC725_VFO_ALL,IC725_ANTS),
	FRQ_RNG_HF(2,IC725_AM_TX_MODES, W(10),W(40),IC725_VFO_ALL,IC725_ANTS),  /* AM class */
	RIG_FRNG_END, },

.tuning_steps = 	{
	{IC725_ALL_RX_MODES,10},
	{IC725_ALL_RX_MODES,20},
	{IC725_ALL_RX_MODES,50},
	{IC725_ALL_RX_MODES,kHz(1)},
	{IC725_ALL_RX_MODES,MHz(1)},
	RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
	{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.4)},
	{RIG_MODE_AM, kHz(8)},	/* optional, TBC */
	{RIG_MODE_FM, kHz(15)},	/* optional, TBC */
	RIG_FLT_END,
	},

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic725_priv_caps,
.rig_init =	icom_init,
.rig_cleanup =	icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,
.set_split =  icom_set_split,

.scan =  icom_scan,

.decode_event =  icom_decode_event,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,

};

