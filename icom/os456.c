/*
 *  Hamlib CI-V backend - description of the OptoScan456
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: os456.c,v 1.2 2002-08-16 17:43:01 fillods Exp $
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
#include "tones.h"


#define OS456_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define OS456_VFO_ALL (RIG_VFO_A)

#define OS456_LEVELS (RIG_LEVEL_STRENGTH)

/*
 * The signal strength data is in the form of two bytes, each consisting 
 * of two BCD digits. The signal strength is reported in units of absolute 
 * dBm as measured at the antenna connector. The reported signal strength 
 * ranges from a maximum signal of 0 dBm to a minimum signal of - 125 dBm. 
 * A minus sign is implied.
 */

#define OS456_STR_CAL { 2, { \
		{ 125, -60 }, \
		{ 0, 60 }, \
		} }		/* TBC */

/*
 * The OptoScan is not actually a rig. This is an add-in circuit board
 * for the Realistic PRO-2006 and PRO-2005 Scanning VHF-UHF Receivers.
 * http://www.optoelectronics.com/tech/pdf/os535/os535_ci5_spec_v10.pdf
 *
 * TODO: srch_dcs, srch_ctcss, rcv_dtmf, and make icom_probe opto aware
 */

static const struct icom_priv_caps os456_priv_caps = { 
		0x80,	/* default address */
		0,		/* 731 mode */
		NULL,
		OS456_STR_CAL
};

const struct rig_caps os456_caps = {
.rig_model =  RIG_MODEL_OS456,
.model_name = "OptoScan456", 
.mfg_name =  "Optoelectronics", 
.version =  "0.2", 
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_SCANNER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  38400,
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
.ctcss_list =  full_ctcss_list,
.dcs_list =  full_dcs_list,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  RIG_OP_NONE,
.scan_ops =  RIG_SCAN_NONE,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  { RIG_CHAN_END, },

.rx_range_list1 =   {
	{ MHz(25),MHz(520),OS456_MODES,-1,-1,OS456_VFO_ALL},
	{ MHz(760),MHz(1300),OS456_MODES,-1,-1,OS456_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =  { RIG_FRNG_END, },		/* this is a scanner */

.rx_range_list2 =   {
	{ MHz(25),MHz(520),OS456_MODES,-1,-1,OS456_VFO_ALL},
	{ MHz(760),MHz(823.995),OS456_MODES,-1,-1,OS456_VFO_ALL},
	{ MHz(849),MHz(868.995),OS456_MODES,-1,-1,OS456_VFO_ALL},
	{ MHz(894),MHz(1300),OS456_MODES,-1,-1,OS456_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list2 =  { RIG_FRNG_END, },		/* this is a scanner */

.tuning_steps = 	{
	 {OS456_MODES,kHz(5)},
	 {OS456_MODES,kHz(12.5)},
	 {OS456_MODES,kHz(50)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
		{RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM, kHz(15)},	/* TBC */
		RIG_FLT_END,
	},

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&os456_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  optoscan_open,
.rig_close =  optoscan_close,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,

.get_level =  icom_get_level,
.get_dcd =  icom_get_dcd,

.decode_event =  icom_decode_event,

.get_info =  optoscan_get_info,

};

