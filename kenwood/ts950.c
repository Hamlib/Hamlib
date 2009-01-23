/*
 *  Hamlib Kenwood backend - TS950 description
 *  Copyright (c) 2002-2004 by Stephane Fillod
 *
 *	$Id: ts950.c,v 1.15 2009-01-23 03:24:42 n0nb Exp $
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


#define TS950_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS950_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS950_AM_TX_MODES RIG_MODE_AM

#define TS950_FUNC_ALL (RIG_FUNC_TSQL)

#define TS950_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN)

#define TS950_VFO (RIG_VFO_A|RIG_VFO_B)

#define cmd_trm(rig) ((struct kenwood_priv_caps *)(rig)->caps->priv)->cmdtrm
const struct kenwood_priv_caps  ts950_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};


/*
 * ts950sdx rig capabilities.
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts950sdx_caps = {
.rig_model =  RIG_MODEL_TS950SDX,
.model_name = "TS-950SDX",
.mfg_name =  "Kenwood",
.version =  BACKEND_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  4800,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  TS950_FUNC_ALL,
.has_set_func =  TS950_FUNC_ALL,
.has_get_level =  TS950_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS950_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },	/* FIXME: preamp list */
.attenuator =   { 6, 12, 18, RIG_DBLST_END, },
.max_rit =  kHz(9.99),
.max_xit =  kHz(9.99),
.max_ifshift =  Hz(0),
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
			{  0, 89, RIG_MTYPE_MEM },	/* TBC */
			{ 90, 99, RIG_MTYPE_EDGE },
			RIG_CHAN_END,
		   },

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
	{kHz(100),MHz(30),TS950_ALL_MODES,-1,-1,TS950_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
    {kHz(1800),MHz(2)-1,TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},	/* 100W class */
    {kHz(1800),MHz(2)-1,TS950_AM_TX_MODES,2000,W(40),TS950_VFO},		/* 25W class */
    {kHz(3500),MHz(4)-1,TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {kHz(3500),MHz(4)-1,TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
    {MHz(7),kHz(7300),TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {MHz(7),kHz(7300),TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
    {kHz(10100),kHz(10150),TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {kHz(10100),kHz(10150),TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
    {MHz(14),kHz(14350),TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {MHz(14),kHz(14350),TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
    {kHz(18068),kHz(18168),TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {kHz(18068),kHz(18168),TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
    {MHz(21),kHz(21450),TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {MHz(21),kHz(21450),TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
    {kHz(24890),kHz(24990),TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {kHz(24890),kHz(24990),TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
    {MHz(28),kHz(29700),TS950_OTHER_TX_MODES,5000,W(150),TS950_VFO},
    {MHz(28),kHz(29700),TS950_AM_TX_MODES,2000,W(40),TS950_VFO},
	RIG_FRNG_END,
  }, /* tx range */
.tuning_steps =  {
	 {TS950_ALL_MODES,50},
	 {TS950_ALL_MODES,100},
	 {TS950_ALL_MODES,kHz(1)},
	 {TS950_ALL_MODES,kHz(5)},
	 {TS950_ALL_MODES,kHz(9)},
	 {TS950_ALL_MODES,kHz(10)},
	 {TS950_ALL_MODES,12500},
	 {TS950_ALL_MODES,kHz(20)},
	 {TS950_ALL_MODES,kHz(25)},
	 {TS950_ALL_MODES,kHz(100)},
	 {TS950_ALL_MODES,MHz(1)},
	 {TS950_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB, kHz(2.4)},
		{RIG_MODE_CW, Hz(200)},
		{RIG_MODE_RTTY, Hz(500)},
		{RIG_MODE_AM, kHz(9)},
		{RIG_MODE_FM, kHz(14)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts950_priv_caps,

.rig_init = kenwood_init,
.rig_cleanup = kenwood_cleanup,
.set_freq =  kenwood_set_freq,
.get_freq =  kenwood_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_xit =  kenwood_set_xit,
.get_xit =  kenwood_get_xit,
.set_mode =  kenwood_set_mode,
.get_mode =  kenwood_get_mode,
.set_vfo =  kenwood_set_vfo,
.get_vfo =  kenwood_get_vfo_if,
.set_ctcss_tone =  kenwood_set_ctcss_tone,
.get_ctcss_tone =  kenwood_get_ctcss_tone,
.get_ptt =  kenwood_get_ptt,
.set_ptt =  kenwood_set_ptt,
.get_dcd =  kenwood_get_dcd,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.set_level =  kenwood_set_level,
.get_level =  kenwood_get_level,
.send_morse =  kenwood_send_morse,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  kenwood_get_mem,
.set_trn =  kenwood_set_trn,
.get_trn =  kenwood_get_trn,
.set_powerstat =  kenwood_set_powerstat,
.get_powerstat =  kenwood_get_powerstat,
.reset =  kenwood_reset,

};
