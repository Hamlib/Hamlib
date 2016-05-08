/*
 *  Hamlib Kenwood backend - TS-811 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "ic10.h"


#define TS811_ALL_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

/* func and levels to be checked */
#define TS811_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_LOCK|RIG_FUNC_MUTE)

#define TS811_LEVEL_ALL (RIG_LEVEL_STRENGTH)

#define TS811_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS811_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)
#define TS811_SCAN_OP (RIG_SCAN_VFO)

static struct kenwood_priv_caps  ts811_priv_caps  = {
	.cmdtrm =  EOM_KEN,
	.if_len =  28,
};

/*
 * ts811 rig capabilities.
 *
 * specs: http://www.qsl.net/sm7vhs/radio/kenwood/ts811/specs.htm
 *
 *  TODO: protocol to be check with manual!
 */
const struct rig_caps ts811_caps = {
.rig_model =  RIG_MODEL_TS811,
.model_name = "TS-811",
.mfg_name =  "Kenwood",
.version =  BACKEND_VER "." IC10_VER,
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
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  1000,
.retry =  10,

.has_get_func =  TS811_FUNC_ALL,
.has_set_func =  TS811_FUNC_ALL,
.has_get_level =  TS811_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS811_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.vfo_ops =  TS811_VFO_OP,
.scan_ops =  TS811_SCAN_OP,
.ctcss_list =  kenwood38_ctcss_list,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  kHz(9.9),
.max_xit =  0,
.max_ifshift =  0,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

	/* FIXME: split memories, call channel, etc. */
.chan_list =  { {  1, 59, RIG_MTYPE_MEM, {IC10_CHANNEL_CAPS} },
			 RIG_CHAN_END,
		   },

.rx_range_list1 =  {
	{MHz(430),MHz(440),TS811_ALL_MODES,-1,-1,TS811_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  {
    {MHz(430),MHz(440),TS811_ALL_MODES,W(5),W(25),TS811_VFO},
	RIG_FRNG_END,
  }, /* tx range */

.rx_range_list2 =  {
	{MHz(430),MHz(450),TS811_ALL_MODES,-1,-1,TS811_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(430),MHz(450),TS811_ALL_MODES,W(5),W(25),TS811_VFO},
	RIG_FRNG_END,
  }, /* tx range */


.tuning_steps =  {
	 {TS811_ALL_MODES,50},
	 {TS811_ALL_MODES,100},
	 {TS811_ALL_MODES,kHz(1)},
	 {TS811_ALL_MODES,kHz(5)},
	 {TS811_ALL_MODES,kHz(9)},
	 {TS811_ALL_MODES,kHz(10)},
	 {TS811_ALL_MODES,12500},
	 {TS811_ALL_MODES,kHz(20)},
	 {TS811_ALL_MODES,kHz(25)},
	 {TS811_ALL_MODES,kHz(100)},
	 {TS811_ALL_MODES,MHz(1)},
	 {TS811_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.2)},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts811_priv_caps,

.rig_init = kenwood_init,
.rig_cleanup = kenwood_cleanup,
.set_freq =  kenwood_set_freq,
.get_freq =  ic10_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_xit =  kenwood_set_xit,
.get_xit =  kenwood_get_xit,
.set_mode =  kenwood_set_mode,
.get_mode =  ic10_get_mode,
.set_vfo =  ic10_set_vfo,
.get_vfo =  ic10_get_vfo,
.set_split_vfo =  ic10_set_split_vfo,
.get_split_vfo =  ic10_get_split_vfo,
.set_ptt =  kenwood_set_ptt,
.get_ptt =  ic10_get_ptt,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  ic10_get_mem,
.set_trn =  kenwood_set_trn,
.scan =  kenwood_scan,
.set_ctcss_tone =  kenwood_set_ctcss_tone,
.get_ctcss_tone =  kenwood_get_ctcss_tone,
.set_level =  kenwood_set_level,
.get_level =  kenwood_get_level,
.set_channel = ic10_set_channel,
.get_channel = ic10_get_channel,
.decode_event = ic10_decode_event,

};

/*
 * Function definitions below
 */

