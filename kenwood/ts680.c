/*
 *  Hamlib Kenwood backend - TS680 description
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: ts680.c,v 1.1 2003-11-10 15:59:36 fillods Exp $
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

#include <hamlib/rig.h>
#include "kenwood.h"


#define TS680_ALL_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB)
#define TS680_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define TS680_AM_TX_MODES RIG_MODE_AM

/* FIXME: TBC */
#define TS680_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_ANF|RIG_FUNC_LOCK)

#define TS680_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_AGC)

#define TS680_VFO (RIG_VFO_A|RIG_VFO_B)
#define TS680_ANTS (0)

static const struct kenwood_priv_caps  ts680_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/*
 * ts680 rig capabilities.
 * written from specs:
 * 	http://www.qsl.net/sm7vhs/radio/kenwood/ts680/specs.htm
 *
 * TODO: protocol to be check with manual!
 */
const struct rig_caps ts680s_caps = {
.rig_model =  RIG_MODEL_TS680S,
.model_name = "TS-680S",
.mfg_name =  "Kenwood",
.version =  "0.2",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  57600,	/* TBC */
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  TS680_FUNC_ALL,
.has_set_func =  TS680_FUNC_ALL,
.has_get_level =  TS680_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS680_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.preamp =   { RIG_DBLST_END, },	/* FIXME: preamp list */
.attenuator =   { RIG_DBLST_END, },	/* TBC */
.max_rit =  kHz(2.54),
.max_xit =  kHz(2.54),
.max_ifshift =  Hz(0),
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  {
			{  0, 89, RIG_MTYPE_MEM  },	/* TBC */
			{ 90, 99, RIG_MTYPE_EDGE },
			RIG_CHAN_END,
		},

.rx_range_list1 =  { 
	{kHz(500),MHz(30),TS680_ALL_MODES,-1,-1,TS680_VFO},
	{MHz(50),MHz(54),TS680_ALL_MODES,-1,-1,TS680_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  { 
    {kHz(1810),kHz(1850),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(1810),kHz(1850),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(3500),kHz(3800),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(3500),kHz(3800),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(7),kHz(7100),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {MHz(7),kHz(7100),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(10100),kHz(10150),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(10100),kHz(10150),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(14),kHz(14350),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {MHz(14),kHz(14350),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(18068),kHz(18168),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(18068),kHz(18168),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(21),kHz(21450),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {MHz(21),kHz(21450),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(24890),kHz(24990),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(24890),kHz(24990),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(28),kHz(29700),TS680_OTHER_TX_MODES^RIG_MODE_FM,W(5),W(95),TS680_VFO},
    {MHz(28),kHz(29700),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(28),kHz(29700),RIG_MODE_FM,W(1),W(10),TS680_VFO},
    {MHz(50),MHz(54),TS680_OTHER_TX_MODES,W(1),W(10),TS680_VFO},
    {MHz(50),kHz(54),TS680_AM_TX_MODES,W(1),W(4),TS680_VFO},
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(500),MHz(30),TS680_ALL_MODES,-1,-1,TS680_VFO},
	{MHz(50),MHz(54),TS680_ALL_MODES,-1,-1,TS680_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {kHz(1800),MHz(2)-1,TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(1800),MHz(2)-1,TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(3500),MHz(4)-1,TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(3500),MHz(4)-1,TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(7),kHz(7300),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {MHz(7),kHz(7300),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(10100),kHz(10150),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(10100),kHz(10150),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(14),kHz(14350),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {MHz(14),kHz(14350),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(18068),kHz(18168),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(18068),kHz(18168),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(21),kHz(21450),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {MHz(21),kHz(21450),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {kHz(24890),kHz(24990),TS680_OTHER_TX_MODES,W(5),W(100),TS680_VFO},
    {kHz(24890),kHz(24990),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(28),kHz(29700),TS680_OTHER_TX_MODES^RIG_MODE_FM,W(5),W(95),TS680_VFO},
    {MHz(28),kHz(29700),TS680_AM_TX_MODES,W(2),W(40),TS680_VFO},
    {MHz(28),kHz(29700),RIG_MODE_FM,W(1),W(10),TS680_VFO},
    {MHz(50),MHz(54),TS680_OTHER_TX_MODES,W(1),W(10),TS680_VFO},
    {MHz(50),kHz(54),TS680_AM_TX_MODES,W(1),W(4),TS680_VFO},
	RIG_FRNG_END,
  }, /* tx range */
.tuning_steps =  {		/* FIXME: TBC */
	 {TS680_ALL_MODES,50},
	 {TS680_ALL_MODES,100},
	 {TS680_ALL_MODES,kHz(1)},
	 {TS680_ALL_MODES,kHz(5)},
	 {TS680_ALL_MODES,kHz(9)},
	 {TS680_ALL_MODES,kHz(10)},
	 {TS680_ALL_MODES,12500},
	 {TS680_ALL_MODES,kHz(20)},
	 {TS680_ALL_MODES,kHz(25)},
	 {TS680_ALL_MODES,kHz(100)},
	 {TS680_ALL_MODES,MHz(1)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_AM, kHz(2.2)},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts680_priv_caps,

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

