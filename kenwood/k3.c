/*
 *  Hamlib Kenwood backend - Elecraft K3 description
 *  Copyright (c) 2002-2009 by Stephane Fillod
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
#include "bandplan.h"
#include "elecraft.h"

#define K3_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
		RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_PKTUSB|\
		RIG_MODE_PKTLSB)

#define K3_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_LOCK)

#define K3_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_SQL|\
	        RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD|\
		RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN)

#define K3_VFO (RIG_VFO_A|RIG_VFO_B)
#define K3_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define K3_ANTS (RIG_ANT_1|RIG_ANT_2)


static struct kenwood_priv_caps  k3_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};


/*
 * KIO3 rig capabilities.
 * This kit can recognize a large subset of TS-570/K2 commands and has many
 * extensions of its own.  Extension backend functions to standard Kenwood
 * command are defined in elecraft.c (shared with K2).
 *
 * Part of info comes from http://www.elecraft.com/K2_Manual_Download_Page.htm#K3
 * look for K3 Programmer's Reference PDF
 */
const struct rig_caps k3_caps = {
	.rig_model =  RIG_MODEL_K3,
	.model_name = "K3",
	.mfg_name =  "Elecraft",
	.version =  "20101030",
	.copyright =  "LGPL",
	.status =  RIG_STATUS_BETA,
	.rig_type =  RIG_TYPE_TRANSCEIVER,
	.ptt_type =  RIG_PTT_RIG,
	.dcd_type =  RIG_DCD_RIG,
	.port_type =  RIG_PORT_SERIAL,
	.serial_rate_min =  4800,
	.serial_rate_max =  38400,
	.serial_data_bits =  8,
	.serial_stop_bits =  2,
	.serial_parity =  RIG_PARITY_NONE,
	.serial_handshake =  RIG_HANDSHAKE_NONE,
	.write_delay =  0,
	.post_write_delay =  0,
	.timeout =  600, /* FA and FB make take up to 500 ms on band change */
	.retry =  3,

	.has_get_func =  K3_FUNC_ALL,
	.has_set_func =  K3_FUNC_ALL,
	.has_get_level =  K3_LEVEL_ALL,
	.has_set_level =  RIG_LEVEL_SET(K3_LEVEL_ALL),
	.has_get_parm =  RIG_PARM_NONE,
	.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
	.level_gran =  {},                 /* FIXME: granularity */
	.parm_gran =  {},
	.preamp =   { 14, RIG_DBLST_END, },
	.attenuator =   { 10, RIG_DBLST_END, },
	.max_rit =  Hz(9990),
	.max_xit =  Hz(9990),
	.max_ifshift =  Hz(0),
	.vfo_ops =  K3_VFO_OP,
	.targetable_vfo =  RIG_TARGETABLE_FREQ,
	.transceive =  RIG_TRN_RIG,
	.bank_qty =   0,
	.chan_desc_sz =  0,

	.chan_list =  { RIG_CHAN_END },

	.rx_range_list1 =  {
		{kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
		{ MHz(48), MHz(54), K3_MODES, -1,- 1, K3_VFO, K3_ANTS},
		RIG_FRNG_END,
	  }, /* rx range */
	.tx_range_list1 =  {
		FRQ_RNG_HF(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		FRQ_RNG_6m(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		RIG_FRNG_END,
	  }, /* tx range */

	.rx_range_list2 =  {
		{kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
		{ MHz(48), MHz(54), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
		RIG_FRNG_END,
	  }, /* rx range */
	.tx_range_list2 =  {
		FRQ_RNG_HF(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		FRQ_RNG_6m(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		RIG_FRNG_END,
	  }, /* tx range */
	.tuning_steps =  {
		 {K3_MODES, 1},
		 RIG_TS_END,
		},

	/* mode/filter list, remember: order matters! */
	.filters =  {
		{RIG_MODE_SSB, kHz(2.7)},
		{RIG_MODE_SSB, kHz(2.8)},
		{RIG_MODE_SSB, kHz(1.8)},
		{RIG_MODE_SSB, RIG_FLT_ANY},
		{RIG_MODE_CW|RIG_MODE_CWR, kHz(1)},
		{RIG_MODE_CW|RIG_MODE_CWR, kHz(2.8)},
		{RIG_MODE_CW|RIG_MODE_CWR, Hz(50)},
		{RIG_MODE_CW|RIG_MODE_CWR, RIG_FLT_ANY},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2.7)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(500)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, RIG_FLT_ANY},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, kHz(2.7)},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, kHz(2.8)},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, Hz(50)},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, RIG_FLT_ANY},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_AM, kHz(13)},
		{RIG_MODE_AM, kHz(2.7)},
		{RIG_MODE_AM, RIG_FLT_ANY},
		{RIG_MODE_FM, kHz(13)}, /* TBC */
		RIG_FLT_END,
	},
	.priv =  (void *)&k3_priv_caps,

	.rig_init =		kenwood_init,
	.rig_cleanup =	kenwood_cleanup,
	.rig_open =		elecraft_open,
	.set_freq =		kenwood_set_freq,
	.get_freq =		kenwood_get_freq,
	.set_mode =		k3_set_mode,
	.get_mode =		k3_get_mode,
	.set_vfo =		k3_set_vfo,
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
	.get_level =	kenwood_get_level,
	.vfo_op =		kenwood_vfo_op,
	.set_trn =		kenwood_set_trn,
	.get_trn =		kenwood_get_trn,
	.set_powerstat =	kenwood_set_powerstat,
	.get_powerstat =	kenwood_get_powerstat,
	.set_ant =		kenwood_set_ant,
	.get_ant =		kenwood_get_ant,
	.send_morse =	kenwood_send_morse,

};


/*
 * K3 extension function definitions in elecraft.c
 */

