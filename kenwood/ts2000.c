/*
 *  Hamlib Kenwood backend - TS2000 description
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *		$Id: ts2000.c,v 1.9 2002-06-30 10:20:52 dedmons Exp $
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
#include "ts2k.h"


#define TS2000_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM| \
	RIG_MODE_RTTY)
#define TS2000_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM| \
	RIG_MODE_RTTY)
#define TS2000_AM_TX_MODES (RIG_MODE_AM)

// the following might be cond. later
# define TS2000_RIGVFO (0)
# define TS2000_MAINVFO (RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM)
# define TS2000_SUBVFO (RIG_VFO_C)

#ifndef _NEW_VFO_H

// old / simple
# define TS2000_FUNC_ALL (RIG_FUNC_TONE | RIG_FUNC_NB)
# define TS2000_PARM_OP (RIG_PARM_BEEP | RIG_PARM_BACKLIGHT)
# define TS2000_LEVEL_ALL (RIG_LEVEL_PREAMP | RIG_LEVEL_VOX | RIG_LEVEL_AF)
# define TS2000_SCAN_OP (RIG_SCAN_STOP | RIG_SCAN_MEM)

#else

// new
# define TS2000_FUNC_ALL ( RIG_FUNC_ALL & \
			~(RIG_FUNC_MN | RIG_FUNC_RNF | RIG_FUNC_VSC) ) 
# define TS2000_PARM_OP (RIG_PARM_EXCLUDE(RIG_PARM_BAT | RIG_PARM_TIME))
# define TS2000_LEVEL_ALL (RIG_LEVEL_ALL & ~(RIG_LEVEL_APF))
# define TS2000_SCAN_OP (RIG_SCAN_ALL & ~(RIG_SCAN_DELTA))
// the following uses both Sub and Main for the Major mode
//# define TS2000_MAINVFO (RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM_A | RIG_VFO_CALL_A)
//# define TS2000_RIGVFO (RIG_VFO_SAT | RIG_VFO_CROSS)
//# define TS2000_SUBVFO (RIG_VFO_C | RIG_VFO_MEM_C | RIG_VFO_CALL_C)

#endif

#define TS2000_VFO_ALL (TS2000_RIGVFO | TS2000_MAINVFO | TS2000_SUBVFO)
#define TS2000_VFO_OP (RIG_OP_UP | RIG_OP_DOWN)

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

const struct ts2k_priv_caps  ts2k_priv_caps  = {
		cmdtrm: EOM_KEN,
};

/*
 * ts2000 rig capabilities.
 *
 * TODO: antenna caps
 *
 * part of infos comes from http://www.kenwood.net/
 */
const struct rig_caps ts2000_caps = {
rig_model: RIG_MODEL_TS2000,
model_name:"TS-2000",
mfg_name: "Kenwood",
version: "0.1.2",
copyright: "LGPL",
status: RIG_STATUS_ALPHA,
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
timeout: 20,
retry: 1,

has_get_func: TS2000_FUNC_ALL,
has_set_func: TS2000_FUNC_ALL,
has_get_level: TS2000_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(TS2000_LEVEL_ALL),
has_get_parm: TS2000_PARM_OP,
has_set_parm: TS2000_PARM_OP,
level_gran: {},                 /* FIXME: granularity */
parm_gran: {},
vfo_ops: TS2000_VFO_OP,
scan_ops: TS2000_SCAN_OP,
ctcss_list: ts2k_ctcss_list,
dcs_list: ts2000_dcs_list,
preamp:  { 20, RIG_DBLST_END, },	/* FIXME: real preamp? */
attenuator:  { 20, RIG_DBLST_END, },
max_rit: kHz(20),
max_xit: kHz(20),
max_ifshift: kHz(1),
targetable_vfo: RIG_TARGETABLE_FREQ,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 8,

/* set up the memories.  See also, rig.h --D.E. kd7eni */

/* The following are suggested 'modes' and when the following may
 *  be accessed:
 *
 *	MTYPE		MSTATE		Description
 *
 *	MEM		M_MEM		main, sub
 *	EDGE		M_MEM		main, sub (vhf/uhf)
 *	MEMOPAD		M_VFO		e.g. main&&sub in vfo (both!)
 *	CALL		M_ANY		at least VFO and MEM (others?)
 *	SAT		M_SAT		only (uses both main+sub)
 *	PCT		M_PCT		when P.C.T. enabled on sub+tnc
 *	MENU		M_MOST		rig does it if it feels like it :)
 *	SETUP		M_UNKNOWN	twilight zone stuff...
 */
chan_list: {
	{ 0, 289, RIG_MTYPE_MEM, 0 },		/* regular memories */
	/* Note: each memory is receive+transmit an RX != TX is split memory. */
	{ 290, 299, RIG_MTYPE_EDGE, 0 },	/* band tune limits (not scan-only) */
	{ 0, 9, RIG_MTYPE_MEMOPAD, 0 },		/* Quick Memories, Main+sub both saved:) */
	{ 0, 1, RIG_MTYPE_CALL, 0 },		/* each TX band has one call */
	{ 0, 9, RIG_MTYPE_SAT, 0 },		/* direct operation from these */
//	{ 0, 9, RIG_MTYPE_PCT, 0 },		/* packet clusters buffered as
//						   they come in */
//	{ 0, 1, RIG_MTYPE_MENU, 0 },		/* There are two menus, A/B. I
//						   set one for HF, one for VHF/UHF*/
//	{ 0, 5, RIG_MTYPE_SETUP, 0 },		/* See: "pm;" command.  ;) */
	/* This seems to be undocumented and not accesible to the front panel.
	   When operated it seems to be an independently settable menu. Thus,
	   more than just A/B are available.  I don't know if the memopad
	   quick memories are involved but the regular MEM ones are *NOT*
	   duplicated.  The manual only says: 0=PM off, 1-5=channel 1-5.
	   Kenwood calls this "Programmable Memory".  I haven't used this
	   in some time but 0-5 seems more appropriate than 1-5.  I'll
	   investigate more after hamlib-1.1.3 (gnurig's target release). */

/*	{ 0, , RIG_MTYPE_, 0 },*/
	RIG_CHAN_END,
   },
rx_range_list1: {
	{kHz(300),MHz(60),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(144),MHz(146),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(430),MHz(440),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(144),MHz(146),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	{MHz(430),MHz(440),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	RIG_FRNG_END,
  }, /* rx range */
tx_range_list1: {
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

rx_range_list2: {
	{kHz(300),MHz(60),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(142),MHz(152),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(420),MHz(450),TS2000_ALL_MODES,-1,-1,TS2000_MAINVFO},
	{MHz(118),MHz(174),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	{MHz(220),MHz(512),TS2000_ALL_MODES,-1,-1,TS2000_SUBVFO},
	RIG_FRNG_END,
  }, /* rx range */
tx_range_list2: {
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
tuning_steps: {
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
filters: {
		{RIG_MODE_SSB, kHz(2.2)},
		{RIG_MODE_CW, Hz(600)},
		{RIG_MODE_RTTY, Hz(1500)},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(12)},
		RIG_FLT_END,
	},
priv: (void *)&ts2k_priv_caps,

/* ts2k */
//set_tone: ts2k_set_tone,
//get_tone: ts2k_get_tone,
set_ctcss_tone: ts2k_set_ctcss_tone,
get_ctcss_tone: ts2k_get_ctcss_tone,
get_dcd: ts2k_get_dcd,
set_freq: ts2k_set_freq,
get_freq: ts2k_get_freq,
get_func: ts2k_get_func,
set_func: ts2k_set_func,
get_info: ts2k_get_info,
get_level: ts2k_get_level,
set_level: ts2k_set_level,
get_mem: ts2k_get_mem,
set_mem: ts2k_set_mem,
get_mode: ts2k_get_mode,
set_mode: ts2k_set_mode,
get_powerstat: ts2k_get_powerstat,
set_powerstat: ts2k_set_powerstat,
get_ptt: ts2k_get_ptt,
set_ptt: ts2k_set_ptt,
reset: ts2k_reset,
send_morse: ts2k_send_morse,
get_trn: ts2k_get_trn,
set_trn: ts2k_set_trn,
/*
*/
set_vfo: ts2k_set_vfo,
get_vfo: ts2k_get_vfo,
vfo_op: ts2k_vfo_op,
/*
 * stuff I've written--kd7eni
 */

scan:		ts2k_scan,
get_channel:	ts2k_get_channel,
set_channel:	ts2k_set_channel,
get_dcs_code:	ts2k_get_dcs_code,
set_dcs_code:	ts2k_set_dcs_code,
get_parm:	ts2k_get_parm,
set_parm:	ts2k_set_parm,
get_rit:	ts2k_get_rit,
set_rit:	ts2k_set_rit,
get_rptr_offs:	ts2k_get_rptr_offs,
set_rptr_offs:	ts2k_set_rptr_offs,
get_rptr_shift:	ts2k_get_rptr_shift,
set_rptr_shift:	ts2k_set_rptr_shift,
get_split:	ts2k_get_split,
set_split:	ts2k_set_split,
get_split_freq:	ts2k_get_split_freq,
set_split_freq:	ts2k_set_split_freq,
get_split_mode:	ts2k_get_split_mode,
set_split_mode:	ts2k_set_split_mode,
get_ts:		ts2k_get_ts,
set_ts:		ts2k_set_ts,
get_xit:	ts2k_get_xit,
set_xit:	ts2k_set_xit,

/* comming soon... */
//get_tone_sql:	ts2k_get_tone_sql,
//set_tone_sql:	ts2k_set_tone_sql,
//decode_event:	ts2k_decode_event,	/* highest */
//get_conf:	ts2k_get_conf,
//set_conf:	ts2k_set_conf,
//get_ant:	ts2k_get_ant,
//set_ant:	ts2k_set_ant,
//recv_dtmf:	ts2k_recv_dtmf,		/* possible? */
//get_ctcss_sql:	ts2k_get_ctcss_sql,
//set_ctcss_sql:	ts2k_set_ctcss_sql,
//get_dcs_sql:	ts2k_get_dcs_sql,
//set_dcs_sql:	ts2k_set_dcs_sql,
//send_dtmf:	ts2k_send_dtmf,		/* lowest */

/* and never... */
//set_bank:				/* not needed */
/*
  end ts2k
 */
};

/*
 * Function definitions below
 */
