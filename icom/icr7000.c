/*
 *  Hamlib CI-V backend - IC-R7000 description
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *		$Id: icr7000.c,v 1.2 2002-03-05 00:49:30 fillods Exp $
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
#include <serial.h>
#include <misc.h>

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


#define ICR7000_MODES (RIG_MODE_AM|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_WFM)

#define ICR7000_OPS (RIG_OP_FROM_VFO|RIG_OP_MCL)

static int r7000_set_freq(RIG *rig, vfo_t vfo, freq_t freq);


static const struct icom_priv_caps icr7000_priv_caps = {
	0x08,   /* default address */
	0,      /* 731 mode */
	r7100_ts_sc_list,
	EMPTY_STR_CAL
};
/*
 * ICR7000 rigs capabilities.
 */
const struct rig_caps icr7000_caps = {
rig_model: RIG_MODEL_ICR7000,
model_name:"ICR-7000",
mfg_name: "Icom",
version: "0.2",
copyright: "LGPL",
status: RIG_STATUS_ALPHA,
rig_type: RIG_TYPE_RECEIVER,
ptt_type: RIG_PTT_NONE,
dcd_type: RIG_DCD_NONE,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 300,
serial_rate_max: 1200,
serial_data_bits: 8,
serial_stop_bits: 1,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE,
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3,

has_get_func: RIG_FUNC_NONE,
has_set_func: RIG_FUNC_NONE,
has_get_level: RIG_LEVEL_NONE,
has_set_level: RIG_LEVEL_NONE,
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE, 
level_gran: {},
parm_gran: {},
ctcss_list: NULL,
dcs_list: NULL,
preamp:  { RIG_DBLST_END, },
attenuator:  { RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: 0,
vfo_ops: ICR7000_OPS,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: { 
	{   1,  99, RIG_MTYPE_MEM,   0 },
	RIG_CHAN_END, },

rx_range_list1: {
	{MHz(25),MHz(1000),ICR7000_MODES,-1,-1, RIG_VFO_A},
    {MHz(1025),MHz(2000),ICR7000_MODES,-1,-1, RIG_VFO_A},
 	RIG_FRNG_END, },
tx_range_list1: { RIG_FRNG_END, },

rx_range_list2: {
	{MHz(25),MHz(1000),ICR7000_MODES,-1,-1, RIG_VFO_A},
    {MHz(1025),MHz(2000),ICR7000_MODES,-1,-1, RIG_VFO_A},
 	RIG_FRNG_END, },
tx_range_list2: { RIG_FRNG_END, },	/* no TX ranges, this is a receiver */

tuning_steps: {
	 {ICR7000_MODES,100},	/* resolution */
#if 0
	 {ICR7000_MODES,kHz(1)},
	 {ICR7000_MODES,kHz(5)},
	 {ICR7000_MODES,kHz(10)},
	 {ICR7000_MODES,12500},
	 {ICR7000_MODES,kHz(25)},
#endif
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
filters: {
		{RIG_MODE_SSB, kHz(2.8)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(15)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)},	/* narrow */
		{RIG_MODE_WFM, kHz(150)},
		RIG_FLT_END,
	},

cfgparams: icom_cfg_params,
set_conf: icom_set_conf,
get_conf: icom_get_conf,

priv: (void*)&icr7000_priv_caps,
rig_init:  icom_init,
rig_cleanup:  icom_cleanup,
rig_open: NULL,
rig_close: NULL,

set_freq: r7000_set_freq,
get_freq: icom_get_freq,
set_mode: icom_set_mode,
get_mode: icom_get_mode,
set_vfo: icom_set_vfo,

decode_event: icom_decode_event,
set_mem: icom_set_mem,
vfo_op: icom_vfo_op,
};


/*
 * Function definitions below
 */


/*
 * r7000_set_freq
 */
static int r7000_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		/* 
		 * The R7000 cannot set freqencies higher than 1GHz, 
		 * this is done by flipping a switch on the front panel and
		 * stripping the most significant digit.
		 * This is the only change with the common icom_set_freq
		 */
		freq %= GHz(1);

		return icom_set_freq(rig, vfo, freq);
}

