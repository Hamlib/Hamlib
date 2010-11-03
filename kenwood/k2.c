/*
 *  Hamlib Kenwood backend - Elecraft K2 description
 *  Copyright (c) 2002-2009 by Stephane Fillod
 *
 *	$Id: k2.c,v 1.6 2009-01-28 23:30:46 azummo Exp $
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
#include "elecraft.h"


#define K2_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)

#define K2_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_LOCK)

#define K2_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_SQL|\
		RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD)

#define K2_VFO (RIG_VFO_A|RIG_VFO_B)
#define K2_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define K2_ANTS (RIG_ANT_1|RIG_ANT_2)

static struct kenwood_priv_caps  k2_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};


/* Actual read extension levels from radio.
 * 
 * The Value stored in this variable maps to elecraft_ext_id_string_list.level
 * and is only written to by the elecraft_get_extension_level() private
 * function during elecraft_open() and thereafter shall be treated as 
 * READ ONLY!
 */
extern int k2_ext_lvl;	/* Initial K2 extension level */


/*
 * KIO2 rig capabilities.
 * This kit can recognize a large subset of TS-570 commands.
 *
 * part of infos comes from http://www.elecraft.com
 */
const struct rig_caps k2_caps = {
	.rig_model =  RIG_MODEL_K2,
	.model_name = "K2",
	.mfg_name =  "Elecraft",
	.version =  BACKEND_VER,
	.copyright =  "LGPL",
	.status =  RIG_STATUS_BETA,
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
	.timeout =  600,
	.retry =  3,

	.has_get_func =  K2_FUNC_ALL,
	.has_set_func =  K2_FUNC_ALL,
	.has_get_level =  K2_LEVEL_ALL,
	.has_set_level =  RIG_LEVEL_SET(K2_LEVEL_ALL),
	.has_get_parm =  RIG_PARM_NONE,
	.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
	.level_gran =  {},                 /* FIXME: granularity */
	.parm_gran =  {},
	.preamp =   { 14, RIG_DBLST_END, },
	.attenuator =   { 10, RIG_DBLST_END, },
	.max_rit =  Hz(9990),
	.max_xit =  Hz(9990),
	.max_ifshift =  Hz(0),
	.vfo_ops =  K2_VFO_OP,
	.targetable_vfo =  RIG_TARGETABLE_FREQ,
	.transceive =  RIG_TRN_RIG,
	.bank_qty =   0,
	.chan_desc_sz =  0,

	.chan_list =  { RIG_CHAN_END },

	.rx_range_list1 =  { 
		{kHz(500),MHz(30),K2_MODES,-1,-1,K2_VFO,K2_ANTS},
		RIG_FRNG_END,
	}, /* rx range */
	.tx_range_list1 =  {
		{kHz(1810),kHz(1850)-1,K2_MODES,10,W(15),K2_VFO,K2_ANTS},	/* 15W class */
		{kHz(3500),kHz(3800)-1,K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(7),kHz(7100),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{kHz(10100),kHz(10150),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(14),kHz(14350),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{kHz(18068),kHz(18168),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(21),kHz(21450),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{kHz(24890),kHz(24990),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(28),kHz(29700),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		RIG_FRNG_END,
	}, /* tx range */

	.rx_range_list2 =  {
		{kHz(500),MHz(30),K2_MODES,-1,-1,K2_VFO,K2_ANTS},
		RIG_FRNG_END,
	}, /* rx range */
	.tx_range_list2 =  {
		{kHz(1800),MHz(2)-1,K2_MODES,10,W(15),K2_VFO,K2_ANTS},	/* 15W class */
		{kHz(3500),MHz(4)-1,K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(7),kHz(7300),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{kHz(10100),kHz(10150),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(14),kHz(14350),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{kHz(18068),kHz(18168),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(21),kHz(21450),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{kHz(24890),kHz(24990),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		{MHz(28),kHz(29700),K2_MODES,10,W(15),K2_VFO,K2_ANTS},
		RIG_FRNG_END,
	}, /* tx range */
	.tuning_steps =  {
		{K2_MODES,10},
		RIG_TS_END,
		},
    
	/* mode/filter list, remember: order matters! */
	.filters =  {
		{RIG_MODE_SSB, kHz(2.5)},
		{RIG_MODE_CW|RIG_MODE_CWR, Hz(500)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(500)},
		RIG_FLT_END,
	},
	.priv =  (void *)&k2_priv_caps,

	.rig_init =		kenwood_init,
	.rig_cleanup =	kenwood_cleanup,
	.rig_open =		elecraft_open,
	.set_freq =		kenwood_set_freq,
	.get_freq =		kenwood_get_freq,
	.set_mode =		kenwood_set_mode,
	.get_mode =		kenwood_get_mode,
	.set_vfo =		kenwood_set_vfo,
	.get_vfo =		kenwood_get_vfo_if,
	.set_split_vfo =	kenwood_set_split_vfo,
	.get_split_vfo =	kenwood_get_split_vfo_if,
	.set_rit =		kenwood_set_rit,
	.get_rit =		kenwood_get_rit,
	.set_xit =		kenwood_set_xit,
	.get_xit =		kenwood_get_xit,
	.get_ptt =		kenwood_get_ptt,
	.set_ptt =		kenwood_set_ptt,
	.get_dcd =		kenwood_get_dcd,
	.set_func =		kenwood_set_func,
	.get_func =		kenwood_get_func,
	.set_level =	kenwood_set_level,
	.get_level = 	kenwood_get_level,
	.vfo_op =		kenwood_vfo_op,
	.set_trn =		kenwood_set_trn,
	.get_trn =		kenwood_get_trn,
	.get_powerstat =	kenwood_get_powerstat,
	.set_ant =		kenwood_set_ant,
	.get_ant =		kenwood_get_ant,
	.send_morse =	kenwood_send_morse,

};


/*
 * Function definitions in elecraft.c
 */

