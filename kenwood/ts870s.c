/*
 *  Hamlib Kenwood backend - TS870S description
 *  Copyright (c) 2000,2001 by Stephane Fillod
 *
 *		$Id: ts870s.c,v 1.16 2001-07-13 19:08:15 f4cfe Exp $
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


#define TS870S_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_AM_TX_MODES RIG_MODE_AM

#define TS870S_FUNC_ALL (RIG_FUNC_TSQL)

#define TS870S_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN)

#define TS870S_VFO (RIG_VFO_A|RIG_VFO_B)

/*
 * ts870s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * part of infos comes from http://www.kenwood.net/
 */
const struct rig_caps ts870s_caps = {
rig_model: RIG_MODEL_TS870S,
model_name:"TS-870S",
mfg_name: "Kenwood",
version: "0.1",
copyright: "LGPL",
status: RIG_STATUS_UNTESTED,
rig_type: RIG_TYPE_TRANSCEIVER,
ptt_type: RIG_PTT_RIG,
dcd_type: RIG_DCD_RIG,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 1200,
serial_rate_max: 57600,
serial_data_bits: 8,
serial_stop_bits: 1,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE,
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3,

has_get_func: TS870S_FUNC_ALL,
has_set_func: TS870S_FUNC_ALL,
has_get_level: TS870S_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(TS870S_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,    /* FIXME: parms */
level_gran: {},                 /* FIXME: granularity */
parm_gran: {},
ctcss_list: kenwood38_ctcss_list,
dcs_list: NULL,
preamp:  { RIG_DBLST_END, },	/* FIXME: preamp list */
attenuator:  { 6, 12, 18, RIG_DBLST_END, },
max_rit: Hz(9999),
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
	{kHz(100),MHz(30),TS870S_ALL_MODES,-1,-1,TS870S_VFO},
	RIG_FRNG_END,
  }, /* rx range */
tx_range_list2: {
    {kHz(1800),MHz(2)-1,TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},	/* 100W class */
    {kHz(1800),MHz(2)-1,TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},		/* 25W class */
    {kHz(3500),MHz(4)-1,TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(3500),MHz(4)-1,TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(7),kHz(7300),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(7),kHz(7300),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(10100),kHz(10150),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(10100),kHz(10150),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(14),kHz(14350),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(14),kHz(14350),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(18068),kHz(18168),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(18068),kHz(18168),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(21),kHz(21450),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(21),kHz(21450),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(24890),kHz(24990),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(24890),kHz(24990),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(28),kHz(29700),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(28),kHz(29700),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
	RIG_FRNG_END,
  }, /* tx range */
tuning_steps: {
	 {TS870S_ALL_MODES,50},
	 {TS870S_ALL_MODES,100},
	 {TS870S_ALL_MODES,kHz(1)},
	 {TS870S_ALL_MODES,kHz(5)},
	 {TS870S_ALL_MODES,kHz(9)},
	 {TS870S_ALL_MODES,kHz(10)},
	 {TS870S_ALL_MODES,12500},
	 {TS870S_ALL_MODES,kHz(20)},
	 {TS870S_ALL_MODES,kHz(25)},
	 {TS870S_ALL_MODES,kHz(100)},
	 {TS870S_ALL_MODES,MHz(1)},
	 {TS870S_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
filters: {
		{RIG_MODE_SSB, kHz(2.4)},
		{RIG_MODE_CW, Hz(200)},
		{RIG_MODE_RTTY, Hz(500)},
		{RIG_MODE_AM, kHz(9)},
		{RIG_MODE_FM, kHz(14)},
		RIG_FLT_END,
	},
priv: NULL,

set_freq: kenwood_set_freq,
get_freq: kenwood_get_freq,
set_mode: kenwood_set_mode,
get_mode: kenwood_get_mode,
set_vfo: kenwood_set_vfo,
get_vfo: kenwood_get_vfo,
set_ctcss_tone: kenwood_set_ctcss_tone,
get_ctcss_tone: kenwood_get_ctcss_tone,
set_ptt: kenwood_set_ptt,
get_dcd: kenwood_get_dcd,
get_level: kenwood_get_level,
set_powerstat: kenwood_set_powerstat,
get_powerstat: kenwood_get_powerstat,
reset: kenwood_reset,
send_morse: kenwood_send_morse,

};

/*
 * Function definitions below
 */

