/*
 *  Hamlib Kenwood backend - TS2000 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ts2000.c,v 1.16 2004-09-26 08:35:04 fillods Exp $
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


#define TS2000_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS2000_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS2000_AM_TX_MODES RIG_MODE_AM

#define TS2000_FUNC_ALL (RIG_FUNC_TSQL)

#define TS2000_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN)

#define TS2000_MAINVFO (RIG_VFO_A|RIG_VFO_B)
#define TS2000_SUBVFO (RIG_VFO_C)

#define TS2000_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

/*
 * 103 available DCS codes
 */
static const tone_t ts2000_dcs_list[] = {
       23,  25,  26,  31,   32,  36,  43,  47,       51,  53,
  54,  65,  71,  72,  73,   74, 114, 115, 116, 122, 125, 131,
  132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205,
  212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261,
  263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343,
  346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
  445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516,
  523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654,
  662, 664, 703, 712, 723, 731, 732, 734, 743, 754,
  0,
};

const struct kenwood_priv_caps  ts2000_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/*
 * ts2000 rig capabilities.
 *
 * TODO: antenna caps
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts2000_caps = {
.rig_model =  RIG_MODEL_TS2000,
.model_name = "TS-2000",
.mfg_name =  "Kenwood",
.version =  "0.2",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  TS2000_FUNC_ALL,
.has_set_func =  TS2000_FUNC_ALL,
.has_get_level =  TS2000_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS2000_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.vfo_ops =  TS2000_VFO_OP,
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  ts2000_dcs_list,
.preamp =   { 20, RIG_DBLST_END, },	/* FIXME: real preamp? */
.attenuator =   { 20, RIG_DBLST_END, },
.max_rit =  kHz(20),
.max_xit =  kHz(20),
.max_ifshift =  kHz(1),
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  { RIG_CHAN_END, },	/* FIXME: memory channel list: 1000 memories */

.rx_range_list1 =  {
	{kHz(300),MHz(60),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(144),MHz(146),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(430),MHz(440),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(144),MHz(146),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	{MHz(430),MHz(440),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  {
    {kHz(1830),kHz(1850),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(1830),kHz(1850),TS2000_AM_TX_MODES,2000,25000,TS2000_MAINVFO},
    {kHz(3500),kHz(3800),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(3500),kHz(3800),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(7),kHz(7100),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(7),kHz(7100),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(10.1),MHz(10.15),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(10.1),MHz(10.15),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(14),kHz(14350),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(14),kHz(14350),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {kHz(18068),kHz(18168),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(18068),kHz(18168),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(21),kHz(21450),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(21),kHz(21450),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {kHz(24890),kHz(24990),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(24890),kHz(24990),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(28),kHz(29700),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(28),kHz(29700),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(50),MHz(50.2),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(50),MHz(50.2),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(144),MHz(146),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(144),MHz(146),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(430),MHz(440),TS2000_OTHER_TX_MODES,W(5),W(50),TS2000_MAINVFO},
    {MHz(430),MHz(440),TS2000_AM_TX_MODES,W(5),W(12.5),TS2000_MAINVFO},
	RIG_FRNG_END,
  }, /* tx range */

.rx_range_list2 =  {
	{kHz(300),MHz(60),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(142),MHz(152),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(420),MHz(450),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(118),MHz(174),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	{MHz(220),MHz(512),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {kHz(1800),MHz(2),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(1800),MHz(2),TS2000_AM_TX_MODES,2000,25000,TS2000_MAINVFO},
    {kHz(3500),MHz(4),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(3500),MHz(4),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(7),kHz(7300),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(7),kHz(7300),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(10.1),MHz(10.15),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(10.1),MHz(10.15),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(14),kHz(14350),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(14),kHz(14350),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {kHz(18068),kHz(18168),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(18068),kHz(18168),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(21),kHz(21450),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(21),kHz(21450),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {kHz(24890),kHz(24990),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {kHz(24890),kHz(24990),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(28),kHz(29700),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(28),kHz(29700),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(50),MHz(54),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(50),MHz(54),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(144),MHz(148),TS2000_OTHER_TX_MODES,W(5),W(100),TS2000_MAINVFO},
    {MHz(144),MHz(148),TS2000_AM_TX_MODES,W(5),W(25),TS2000_MAINVFO},
    {MHz(430),MHz(450),TS2000_OTHER_TX_MODES,W(5),W(50),TS2000_MAINVFO},
    {MHz(430),MHz(450),TS2000_AM_TX_MODES,W(5),W(12.5),TS2000_MAINVFO},
	RIG_FRNG_END,
  }, /* tx range */
.tuning_steps =  {
	 {TS2000_ALL_MODES,50},
	 {TS2000_ALL_MODES,100},
	 {TS2000_ALL_MODES,kHz(1)},
	 {TS2000_ALL_MODES,kHz(5)},
	 {TS2000_ALL_MODES,kHz(9)},
	 {TS2000_ALL_MODES,kHz(10)},
	 {TS2000_ALL_MODES,12500},
	 {TS2000_ALL_MODES,kHz(20)},
	 {TS2000_ALL_MODES,kHz(25)},
	 {TS2000_ALL_MODES,kHz(100)},
	 {TS2000_ALL_MODES,MHz(1)},
	 {TS2000_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB, kHz(2.2)},
		{RIG_MODE_CW, Hz(600)},
		{RIG_MODE_RTTY, Hz(1500)},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts2000_priv_caps,

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
.send_morse =  kenwood_send_morse,
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

