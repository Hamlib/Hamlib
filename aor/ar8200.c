/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ar8200.c - Copyright (C) 2000,2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a AOR-AR8200 handheld scanner radio
 * using the serial interface.
 *
 *
 * $Id: ar8200.c,v 1.11 2001-06-15 07:08:37 f4cfe Exp $  
 *
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

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
#include "aor.h"


#define AR8200_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM)

#define AR8200_FUNC_ALL (RIG_FUNC_TSQL)

#define AR8200_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

/*
 * ar8200 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/8200.htm
 */
const struct rig_caps ar8200_caps = {
rig_model: RIG_MODEL_AR8200,
model_name:"AR8200",
mfg_name: "AOR",
version: "0.1",
copyright: "GPL",
status: RIG_STATUS_UNTESTED,
rig_type: RIG_TYPE_SCANNER,
ptt_type: RIG_PTT_NONE,
dcd_type: RIG_DCD_NONE,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 4800,
serial_rate_max: 19200,
serial_data_bits: 8,
serial_stop_bits: 2,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_XONXOFF,
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3,
has_get_func: RIG_FUNC_NONE,
has_set_func: AR8200_FUNC_ALL,
has_get_level: AR8200_LEVEL,
has_set_level: RIG_LEVEL_SET(AR8200_LEVEL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,    /* FIXME: parms */
level_gran: {},                 /* FIXME: granularity */
parm_gran: {},
ctcss_list: NULL,				/* FIXME: CTCSS/DCS list */
dcs_list: NULL,
preamp:  { RIG_DBLST_END, },
attenuator:  { RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: 0,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: { RIG_CHAN_END, },	/* FIXME: memory channel list: 1000 memories */

rx_range_list1: { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
tx_range_list1: { RIG_FRNG_END, },
rx_range_list2: {
	{kHz(100),MHz(2040),AR8200_MODES,-1,-1,RIG_VFO_A},
	RIG_FRNG_END,
  }, /* rx range */
tx_range_list2: { RIG_FRNG_END, },	/* no tx range, this is a scanner! */

tuning_steps: {
	 {AR8200_MODES,50},
	 {AR8200_MODES,100},
	 {AR8200_MODES,kHz(1)},
	 {AR8200_MODES,kHz(5)},
	 {AR8200_MODES,kHz(9)},
	 {AR8200_MODES,kHz(10)},
	 {AR8200_MODES,12500},
	 {AR8200_MODES,kHz(20)},
	 {AR8200_MODES,kHz(25)},
	 {AR8200_MODES,kHz(100)},
	 {AR8200_MODES,MHz(1)},
#if 0 
	 {AR8200_MODES,0},	/* any tuning step */
#endif
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
filters: {
        /* mode/filter list, remember: order matters! */
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(3)}, 
		{RIG_MODE_AM, kHz(9)},
		{RIG_MODE_AM, kHz(3)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(12)}, 
		{RIG_MODE_FM, kHz(9)}, 
		{RIG_MODE_WFM, kHz(230)}, /* 50kHz at -3dB, 380kHz at -20dB */
		RIG_FLT_END,
	},

priv: NULL,
rig_init: NULL,
rig_cleanup: NULL,
rig_open: NULL,
rig_close: aor_close,

set_freq: aor_set_freq,
get_freq: aor_get_freq,
set_mode: aor_set_mode,
get_mode: aor_get_mode,

set_ts: aor_set_ts,
set_powerstat: aor_set_powerstat,
};

/*
 * Function definitions below
 */


