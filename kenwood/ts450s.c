/*
 *  Hamlib Kenwood backend - TS450S description
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: ts450s.c,v 1.9 2002-09-04 17:47:25 pa4tu Exp $
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
#include "kenwood.h"


#define TS450S_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS450S_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS450S_AM_TX_MODES RIG_MODE_AM

#define TS450S_FUNC_ALL (RIG_FUNC_TSQL)

#define TS450S_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN)

#define TS450S_VFO (RIG_VFO_A|RIG_VFO_B)

static const struct kenwood_priv_caps  ts450_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/*
 * ts450s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * part of infos comes from .http = //www.qsl.net/sm7vhs/radio/kenwood/ts450/specs.htm
 */
const struct rig_caps ts450s_caps = {
.rig_model =  RIG_MODEL_TS450S,
.model_name = "TS-450S",
.mfg_name =  "Kenwood",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  4800,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  TS450S_FUNC_ALL,
.has_set_func =  TS450S_FUNC_ALL,
.has_get_level =  TS450S_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS450S_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },	/* FIXME: preamp list */
.attenuator =   { 6, 12, 18, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  { RIG_CHAN_END, },	/* FIXME: memory channel list: 1000 memories */

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
	{kHz(500),MHz(30),TS450S_ALL_MODES,-1,-1,TS450S_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {kHz(1800),MHz(2)-1,TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},	/* 100W class */
    {kHz(1800),MHz(2)-1,TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},		/* 40W class */
    {kHz(3500),MHz(4)-1,TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(3500),MHz(4)-1,TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(7),kHz(7300),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(7),kHz(7300),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {kHz(10100),kHz(10150),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(10100),kHz(10150),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(14),kHz(14350),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(14),kHz(14350),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {kHz(18068),kHz(18168),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(18068),kHz(18168),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(21),kHz(21450),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(21),kHz(21450),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {kHz(24890),kHz(24990),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(24890),kHz(24990),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(28),kHz(29700),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(28),kHz(29700),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
	RIG_FRNG_END,
  }, /* tx range */
.tuning_steps =  {
	 {TS450S_ALL_MODES,50},
	 {TS450S_ALL_MODES,100},
	 {TS450S_ALL_MODES,kHz(1)},
	 {TS450S_ALL_MODES,kHz(5)},
	 {TS450S_ALL_MODES,kHz(9)},
	 {TS450S_ALL_MODES,kHz(10)},
	 {TS450S_ALL_MODES,12500},
	 {TS450S_ALL_MODES,kHz(20)},
	 {TS450S_ALL_MODES,kHz(25)},
	 {TS450S_ALL_MODES,kHz(100)},
	 {TS450S_ALL_MODES,MHz(1)},
	 {TS450S_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB, kHz(2.2)},
		{RIG_MODE_CW, Hz(200)},
		{RIG_MODE_RTTY, Hz(500)},
		{RIG_MODE_AM, kHz(9)},
		{RIG_MODE_FM, kHz(14)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts450_priv_caps,

.set_freq =  kenwood_set_freq,
.get_freq =  kenwood_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_xit =  kenwood_set_xit,
.get_xit =  kenwood_get_xit,
.set_mode =  kenwood_set_mode,
.get_mode =  kenwood_get_mode,
.set_vfo =  kenwood_set_vfo,
.get_vfo =  kenwood_get_vfo,
.set_ctcss_tone =  kenwood_set_ctcss_tone,
.get_ctcss_tone =  kenwood_get_ctcss_tone,
.get_ptt =  kenwood_get_ptt,
.set_ptt =  kenwood_set_ptt,
.get_dcd =  kenwood_get_dcd,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.set_level =  kenwood_set_level,
.get_level =  kenwood_get_level,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  kenwood_get_mem,
.set_trn =  kenwood_set_trn,
.get_trn =  kenwood_get_trn,
.set_powerstat =  kenwood_set_powerstat,
.get_powerstat =  kenwood_get_powerstat,
.reset =  kenwood_reset,

};

/*
 * Function definitions below
 */

