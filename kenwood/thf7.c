/*
 *  Hamlib Kenwood backend - TH-F7 description
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: thf7.c,v 1.1 2001-11-09 15:44:38 f4cfe Exp $
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
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "kenwood.h"
#include "th.h"


#define THF7_MODES_TX (RIG_MODE_FM|RIG_MODE_AM)
#define THF7_MODES	(THF7_MODES_TX|RIG_MODE_WFM|RIG_MODE_SSB|RIG_MODE_CW)

#define THF7_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_AIP|RIG_FUNC_SQL)

#define THF7_LEVEL_ALL (RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN)

#define THF7_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

/*
 * TODO: Band A & B
 */
#define THF7_VFO (RIG_VFO_A|RIG_VFO_C)

/*
 * th-f7e rig capabilities.
 */
const struct rig_caps thf7e_caps = {
rig_model: RIG_MODEL_THF7E,
model_name:"TH-F7E",
mfg_name: "Kenwood",
version: "0.1",
copyright: "LGPL",
status: RIG_STATUS_NEW,
rig_type: RIG_TYPE_HANDHELD,
ptt_type: RIG_PTT_RIG,
dcd_type: RIG_DCD_RIG,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 9600,
serial_rate_max: 9600,
serial_data_bits: 8,
serial_stop_bits: 1,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE,
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3,

has_get_func: THF7_FUNC_ALL,
has_set_func: THF7_FUNC_ALL,
has_get_level: THF7_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(THF7_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,    /* FIXME: parms */
level_gran: {},                 /* FIXME: granularity */
parm_gran: {},
ctcss_list: kenwood38_ctcss_list,
dcs_list: NULL,	/* FIXME */
preamp:  { RIG_DBLST_END, },
attenuator:  { 20, RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
vfo_ops: THF7_VFO_OP,
targetable_vfo: 0,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,


chan_list: { {  1,  435, RIG_MTYPE_MEM, 0 },
			 RIG_CHAN_END,
		   },	/* FIXME: memory channel list: 435? memories */

rx_range_list1: {
    {kHz(100),GHz(1.3),THF7_MODES,-1,-1,THF7_VFO},
	RIG_FRNG_END,
  }, /* rx range */
tx_range_list1: {
    {MHz(144),MHz(146),THF7_MODES_TX,W(0.05),W(5),THF7_VFO},
    {MHz(430),MHz(440),THF7_MODES_TX,W(0.05),W(5),THF7_VFO},
	RIG_FRNG_END,
  }, /* tx range */
rx_range_list2: { RIG_FRNG_END, },    /* FIXME: enter region 2 setting */
tx_range_list2: { RIG_FRNG_END, },
tuning_steps: {
	 {THF7_MODES,kHz(5)},
	 {THF7_MODES,kHz(6.25)},
	 {THF7_MODES,kHz(10)},
	 {THF7_MODES,kHz(12.5)},
	 {THF7_MODES,kHz(15)},
	 {THF7_MODES,kHz(20)},
	 {THF7_MODES,kHz(25)},
	 {THF7_MODES,kHz(30)},
	 {THF7_MODES,kHz(50)},
	 {THF7_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
filters: {
		{RIG_MODE_AM|RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
priv: NULL,

set_freq: th_set_freq,
get_freq: th_get_freq,
set_mode: th_set_mode,
get_mode: th_get_mode,
set_vfo: th_set_vfo,
set_ctcss_tone: th_set_ctcss_tone,
get_ctcss_tone: th_get_ctcss_tone,
set_ptt: kenwood_set_ptt,
get_dcd: kenwood_get_dcd,
vfo_op: kenwood_vfo_op,
set_mem: kenwood_set_mem,
get_mem: kenwood_get_mem,

};

