/*
 *  Hamlib Alinco backend - DX77 description
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: dx77.c,v 1.6 2003-04-06 18:40:35 fillods Exp $
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
#include "alinco.h"


#define DX77_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define DX77_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define DX77_AM_TX_MODES RIG_MODE_AM

#define DX77_FUNC (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_TONE|RIG_FUNC_COMP)

#define DX77_LEVEL_ALL (RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD|RIG_LEVEL_BKINDL|RIG_LEVEL_CWPITCH)

#define DX77_PARM_ALL (RIG_PARM_BEEP|RIG_PARM_BACKLIGHT)

#define DX77_VFO (RIG_VFO_A|RIG_VFO_B)

/* 90 is S9 */
#define DX77_STR_CAL { 13, { \
		{   0, -60 }, \
		{  28, -48 }, \
		{  36, -42 }, \
		{  42, -36 }, \
		{  50, -30 }, \
		{  58, -24 }, \
		{  66, -18 }, \
		{  74, -12 }, \
		{  82, -6 }, \
		{  90, 0 }, \
		{ 132, 20 }, \
		{ 174, 40 }, \
		{ 216, 60 }, \
	} }

static const struct alinco_priv_caps dx77_priv_caps = {
		DX77_STR_CAL
};

/*
 * dx77 rig capabilities.
 *
 * protocol is documented at 
 * 		http://www.alinco.com/pdf.files/DX77-77_SOFTWARE_MANUAL.pdf
 *
 * This backend was a pleasure to develop. Documentation is clear,
 * and the protocol logical. I'm wondering is the rig's good too. --SF
 *
 * TODO:
 *  - get_parm/set_parm and some LEVELs left (Set Data "2W" command).
 * 	- tuner
 * 	- up/down
 * 	- scan
 */
const struct rig_caps dx77_caps = {
.rig_model =  RIG_MODEL_DX77,
.model_name = "DX-77",
.mfg_name =  "Alinco",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  DX77_FUNC,
.has_set_func =  DX77_FUNC|RIG_FUNC_MON,
.has_get_level =  DX77_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(DX77_LEVEL_ALL),
.has_get_parm =  DX77_PARM_ALL,
.has_set_parm =  RIG_PARM_SET(DX77_PARM_ALL),
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  common_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 10, RIG_DBLST_END },
.attenuator =   { 10, 20, RIG_DBLST_END },
.max_rit =  kHz(1),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
			{ 0, 99, RIG_MTYPE_MEM },
			RIG_CHAN_END,
		},

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
	{kHz(500),MHz(30),DX77_ALL_MODES,-1,-1,DX77_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
    {kHz(1800),MHz(2)-100,DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {kHz(1800),MHz(2)-100,DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {kHz(3500),MHz(4)-100,DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {kHz(3500),MHz(4)-100,DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {MHz(7),kHz(7300),DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {MHz(7),kHz(7300),DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {kHz(10100),kHz(10150),DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {kHz(10100),kHz(10150),DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {MHz(14),kHz(14350),DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {MHz(14),kHz(14350),DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {kHz(18068),kHz(18168),DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {kHz(18068),kHz(18168),DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {MHz(21),kHz(21450),DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {MHz(21),kHz(21450),DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {kHz(24890),kHz(24990),DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {kHz(24890),kHz(24990),DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
    {MHz(28),kHz(29700),DX77_OTHER_TX_MODES,W(10),W(100),DX77_VFO},
    {MHz(28),kHz(29700),DX77_AM_TX_MODES,W(4),W(40),DX77_VFO},
	RIG_FRNG_END,
  },
.tuning_steps =  {
	 {DX77_ALL_MODES,10},	/* FIXME: add other ts */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.7)},
		{RIG_MODE_AM|RIG_MODE_FM, kHz(8)},
		{RIG_MODE_AM, kHz(2.7)},
		RIG_FLT_END,
	},
.priv =  (void*)&dx77_priv_caps,

.set_freq =  alinco_set_freq,
.get_freq =  alinco_get_freq,
.set_mode =  alinco_set_mode,
.get_mode =  alinco_get_mode,
.set_vfo =  alinco_set_vfo,
.get_vfo =  alinco_get_vfo,
.set_split_vfo =  alinco_set_split_vfo,
.get_split_vfo =  alinco_get_split_vfo,
.set_split_freq =  alinco_set_split_freq,
.get_split_freq =  alinco_get_split_freq,
.set_ctcss_tone =  alinco_set_ctcss_tone,
.get_rit =  alinco_get_rit,
.get_ptt =  alinco_get_ptt,
.get_dcd =  alinco_get_dcd,
.set_func =  alinco_set_func,
.get_func =  alinco_get_func,
.set_level =  alinco_set_level,
.get_level =  alinco_get_level,
.set_mem =  alinco_set_mem,
.get_mem =  alinco_get_mem,

};

/*
 * Function definitions below
 */

