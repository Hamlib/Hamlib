/*
 *  Hamlib Kenwood backend - TS-790 description
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: ts790.c,v 1.8 2002-09-04 17:53:51 pa4tu Exp $
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


#define TS790_ALL_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define TS790_HI_MODES (RIG_MODE_CW|RIG_MODE_FM)
#define TS790_LO_MODES (RIG_MODE_SSB)

/* func and levels to be checked */
#define TS790_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_LOCK|RIG_FUNC_MUTE)

#define TS790_LEVEL_ALL (RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER)

#define TS790_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS790_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

static const struct kenwood_priv_caps  ts790_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/*
 * ts790 rig capabilities.
 *
 * TODO: antenna caps
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts790_caps = {
.rig_model =  RIG_MODEL_TS790,
.model_name = "TS-790",
.mfg_name =  "Kenwood",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  9600,	/* TBC */
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  TS790_FUNC_ALL,
.has_set_func =  TS790_FUNC_ALL,
.has_get_level =  TS790_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS790_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.vfo_ops =  TS790_VFO_OP,
.ctcss_list =  kenwood38_ctcss_list,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { 12, RIG_DBLST_END, },
.max_rit =  kHz(9.9),	/* this is for FM, SSB/CW is 1.9kHz */
.max_xit =  0,
.max_ifshift =  900,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

	/* FIXME: split memories, call channel, etc. */
.chan_list =  { {  1, 59, RIG_MTYPE_MEM,  0 },
			 RIG_CHAN_END,
		   },

.rx_range_list1 =  {
	{MHz(144),MHz(146),TS790_ALL_MODES,-1,-1,TS790_VFO},
	{MHz(430),MHz(440),TS790_ALL_MODES,-1,-1,TS790_VFO},
#if 0
	/* 23 cm option */
	{MHz(1240),MHz(1300),TS790_ALL_MODES,-1,-1,TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  {
    {MHz(144),MHz(146),TS790_LO_MODES,W(5),W(35),TS790_VFO},
    {MHz(144),MHz(146),TS790_HI_MODES,W(5),W(45),TS790_VFO},
    {MHz(430),MHz(440),TS790_LO_MODES,W(5),W(30),TS790_VFO},
    {MHz(430),MHz(440),TS790_HI_MODES,W(5),W(40),TS790_VFO},
#if 0
	{MHz(1240),MHz(1300),TS790_ALL_MODES,W(5),W(10),TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* tx range */

.rx_range_list2 =  {
	{MHz(144),MHz(148),TS790_ALL_MODES,-1,-1,TS790_VFO},
	{MHz(430),MHz(450),TS790_ALL_MODES,-1,-1,TS790_VFO},
#if 0
	{MHz(1240),MHz(1300),TS790_ALL_MODES,-1,-1,TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(144),MHz(148),TS790_LO_MODES,W(5),W(35),TS790_VFO},
    {MHz(144),MHz(148),TS790_HI_MODES,W(5),W(45),TS790_VFO},
    {MHz(430),MHz(450),TS790_LO_MODES,W(5),W(30),TS790_VFO},
    {MHz(430),MHz(450),TS790_HI_MODES,W(5),W(40),TS790_VFO},
#if 0
	{MHz(1240),MHz(1300),TS790_ALL_MODES,W(5),W(10),TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* tx range */


.tuning_steps =  {
	 {TS790_ALL_MODES,50},
	 {TS790_ALL_MODES,100},
	 {TS790_ALL_MODES,kHz(1)},
	 {TS790_ALL_MODES,kHz(5)},
	 {TS790_ALL_MODES,kHz(9)},
	 {TS790_ALL_MODES,kHz(10)},
	 {TS790_ALL_MODES,12500},
	 {TS790_ALL_MODES,kHz(20)},
	 {TS790_ALL_MODES,kHz(25)},
	 {TS790_ALL_MODES,kHz(100)},
	 {TS790_ALL_MODES,MHz(1)},
	 {TS790_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.1)},
		{RIG_MODE_CW, Hz(500)},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts790_priv_caps,

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
.get_info =  kenwood_get_info,
.reset =  kenwood_reset,

};

/*
 * Function definitions below
 */

