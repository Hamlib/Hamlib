/*
 *  Hamlib CI-V backend - description of IC-756 and variations
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ic756.c,v 1.13 2006-07-18 22:51:42 n0nb Exp $
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
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "token.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "bandplan.h"


#define IC756_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC756_1HZ_TS_MODES IC756_ALL_RX_MODES

/* 
 * 100W in all modes but AM (40W)
 */ 
#define IC756_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC756_AM_TX_MODES (RIG_MODE_AM)

#define IC756PRO_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_RF)

#define IC756PRO_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_BALANCE|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF|RIG_LEVEL_RAWSTR)

#define IC756_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

#define IC756_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC756_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_DELTA|RIG_SCAN_PRIO)

#define IC756_ANTS (RIG_ANT_1|RIG_ANT_2)

#define IC756PRO_STR_CAL { 16, \
	{ \
		{   0, -18 }, \
		{  10, -16 }, \
		{  27, -14 }, \
		{  45, -12 }, \
		{  60, -10 }, \
		{  76, -8 }, \
		{  89, -6 }, \
		{ 100, -4 }, \
		{ 110, -2 }, \
		{ 120, 0 }, \
		{ 125, 2 }, \
		{ 129, 4 }, \
		{ 133, 6 }, \
		{ 138, 8 }, \
		{ 142, 10 }, \
		{ 146, 12 } \
	} }

/*
 * ic756 rig capabilities.
 */
static const struct icom_priv_caps ic756_priv_caps = { 
		0x50,	/* default address */
		0,		/* 731 mode */
		ic756pro_ts_sc_list
};

const struct rig_caps ic756_caps = {
.rig_model =  RIG_MODEL_IC756,
.model_name = "IC-756", 
.mfg_name =  "Icom", 
.version =  BACKEND_VER, 
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE, 
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3, 
.has_get_func =  IC756PRO_FUNC_ALL,
.has_set_func =  IC756PRO_FUNC_ALL, 
.has_get_level =  IC756PRO_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(IC756PRO_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,	/* FIXME: parms */
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	},
.parm_gran =  {},
.ctcss_list =  common_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 10, RIG_DBLST_END, },	/* FIXME: TBC */
.attenuator =   { 20, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC756_VFO_OPS,
.scan_ops =  IC756_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
				   {   1,  99, RIG_MTYPE_MEM  },
				   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
				   RIG_CHAN_END,
		},

.rx_range_list1 =   { {kHz(30),MHz(60),IC756_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(1,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(1,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(1,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	RIG_FRNG_END, },

.rx_range_list2 =   { {kHz(30),MHz(60),IC756_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(2,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(2,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(2,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	RIG_FRNG_END, },

.tuning_steps = 	{
	 {IC756_1HZ_TS_MODES,1},
	 {IC756_ALL_RX_MODES,kHz(1)},
	 {IC756_ALL_RX_MODES,kHz(5)},
	 {IC756_ALL_RX_MODES,kHz(9)},
	 {IC756_ALL_RX_MODES,kHz(10)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
		{RIG_MODE_SSB|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_CW, Hz(500)},
		{RIG_MODE_AM, kHz(8)},
		{RIG_MODE_AM, kHz(2.4)},
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_FM, kHz(8)},
		RIG_FLT_END,
	},
.str_cal = IC756PRO_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic756_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,

.decode_event =  icom_decode_event,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  icom_get_split_vfo,

};


/*
 * ic756pro rig capabilities.
 * TODO: check every paramters,
 * 		add antenna capabilities
 */
static const struct icom_priv_caps ic756pro_priv_caps = { 
		0x5c,	/* default address */
		0,		/* 731 mode */
		ic756_ts_sc_list
};

const struct rig_caps ic756pro_caps = {
.rig_model =  RIG_MODEL_IC756PRO,
.model_name = "IC-756PRO", 
.mfg_name =  "Icom", 
.version =  BACKEND_VER, 
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE, 
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3, 
.has_get_func =  IC756PRO_FUNC_ALL,
.has_set_func =  IC756PRO_FUNC_ALL, 
.has_get_level =  IC756PRO_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(IC756PRO_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,	/* FIXME: parms */
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	},
.parm_gran =  {},
.ctcss_list =  common_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 10, 20, RIG_DBLST_END, },	/* FIXME: TBC */
.attenuator =   { 6, 12, 18, 20, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC756_VFO_OPS,
.scan_ops =  IC756_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
				   {   1,  99, RIG_MTYPE_MEM  },
				   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
				   RIG_CHAN_END,
		},

.rx_range_list1 =   { {kHz(30),MHz(60),IC756_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(1,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(1,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(1,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	RIG_FRNG_END, },

.rx_range_list2 =   { {kHz(30),MHz(60),IC756_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(2,IC756_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(2,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(2,IC756_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	RIG_FRNG_END, },

.tuning_steps = 	{
	 {IC756_1HZ_TS_MODES,1},
	 {IC756_ALL_RX_MODES,kHz(1)},
	 {IC756_ALL_RX_MODES,kHz(5)},
	 {IC756_ALL_RX_MODES,kHz(9)},
	 {IC756_ALL_RX_MODES,kHz(10)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
		{RIG_MODE_SSB|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_CW, Hz(500)},
		{RIG_MODE_AM, kHz(8)},
		{RIG_MODE_AM, kHz(2.4)},
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_FM, kHz(8)},
		RIG_FLT_END,
	},
.str_cal = IC756PRO_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic756pro_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,

.decode_event =  icom_decode_event,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.set_ptt =  icom_set_ptt,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_ctcss_tone =  icom_set_ctcss_tone,
.get_ctcss_tone =  icom_get_ctcss_tone,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  icom_get_split_vfo,

};

/*
 * ic756proII rig capabilities.
 * TODO: check every paramters,
 * 		add antenna capabilities
 */
static const struct icom_priv_caps ic756pro2_priv_caps = { 
		0x64,	/* default address */
		0,		/* 731 mode */
		ic756_ts_sc_list
};

/*
 * These TOKEN_BACKEND's are on a different name space than conf's
 */
#define TOK_SSBBASS TOKEN_BACKEND(1)
#define TOK_MEMNAME TOKEN_BACKEND(2)
#define TOK_SQLCTRL TOKEN_BACKEND(3)
#define TOK_MYCALL  TOKEN_BACKEND(4)

static const struct confparams ic756pro2_ext_parms[] = {
	{ TOK_SSBBASS, "ssbbass", "SSB Tx Tone (Bass)", "SSB Tx Tone (Bass)",
		NULL, RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } }
	},
	{ TOK_MEMNAME, "showmem", "Show mem name", "Show memory name",
		NULL, RIG_CONF_CHECKBUTTON, { }
	},
	{ TOK_SQLCTRL, "sqlctrl", "RF/Sql control", "set RF/Squelch control",
		NULL, RIG_CONF_COMBO, { .c = {{ "Auto", "Sql", "RF+Sql", NULL }} }
	},
	{ TOK_MYCALL, "mycall", "Callsign", "My call sign",
		NULL, RIG_CONF_STRING, { }
	},
	{ RIG_CONF_END, NULL, }
};


/*
 * NUMERIC: val.f
 * COMBO: val.i, starting from 0
 * STRING: val.cs for set, val.s for get
 * CHECKBUTTON: val.i 0/1
 */

static int ic756pro2_set_ext_parm(RIG *rig, token_t token, value_t val);
static int ic756pro2_get_ext_parm(RIG *rig, token_t token, value_t *val);

#define IC756PROII_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROII_1HZ_TS_MODES IC756PROII_ALL_RX_MODES
#define IC756PROII_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROII_AM_TX_MODES (RIG_MODE_AM)


const struct rig_caps ic756pro2_caps = {
.rig_model =  RIG_MODEL_IC756PROII,
.model_name = "IC-756PROII", 
.mfg_name =  "Icom", 
.version =  BACKEND_VER, 
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE, 
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3, 
.has_get_func =  IC756PRO_FUNC_ALL,
.has_set_func =  IC756PRO_FUNC_ALL, 
.has_get_level =  IC756PRO_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(IC756PRO_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,	/* FIXME: parms */
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
},
.parm_gran =  {},
.extparms =  ic756pro2_ext_parms,
.ctcss_list =  common_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 10, 20, RIG_DBLST_END, },	/* FIXME: TBC */
.attenuator =   { 6, 12, 18, 20, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(9999),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC756_VFO_OPS,
.scan_ops =  IC756_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
	   {   1,  99, RIG_MTYPE_MEM  },
	   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
	   RIG_CHAN_END,
	},

.rx_range_list1 =   { {kHz(30),MHz(60),IC756PROII_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =   {
	FRQ_RNG_HF(1,IC756PROII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(1,IC756PROII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(1,IC756PROII_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(1,IC756PROII_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
    	RIG_FRNG_END, },

.rx_range_list2 =   { {kHz(30),MHz(60),IC756PROII_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC756PROII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(2,IC756PROII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(2,IC756PROII_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(2,IC756PROII_AM_TX_MODES, W(2),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
    	RIG_FRNG_END, },

.tuning_steps = 	{
	 {IC756PROII_1HZ_TS_MODES,1},
	 {IC756PROII_ALL_RX_MODES,kHz(1)},
	 {IC756PROII_ALL_RX_MODES,kHz(5)},
	 {IC756PROII_ALL_RX_MODES,kHz(9)},
	 {IC756PROII_ALL_RX_MODES,kHz(10)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
	{RIG_MODE_SSB|RIG_MODE_RTTY, kHz(2.4)},
	{RIG_MODE_CW, Hz(500)},
	{RIG_MODE_AM, kHz(6)},
	{RIG_MODE_AM, kHz(2.4)},
	{RIG_MODE_FM, kHz(15)},
	{RIG_MODE_FM, kHz(8)},
	RIG_FLT_END,
	},
.str_cal = IC756PRO_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic756pro2_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,

.decode_event =  icom_decode_event,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.set_ptt =  icom_set_ptt,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_ctcss_tone =  icom_set_ctcss_tone,
.get_ctcss_tone =  icom_get_ctcss_tone,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  icom_get_split_vfo,

.set_ext_parm =  ic756pro2_set_ext_parm,
.get_ext_parm =  ic756pro2_get_ext_parm,
};


/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int ic756pro2_set_ext_parm(RIG *rig, token_t token, value_t val)
{
	struct icom_priv_data *priv;
	struct rig_state *rs;
	unsigned char epbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
	int ack_len, ep_len, val_len;
	int ep_menu;             /* Menu in $1A $05 */
	int icom_val = 0;
	int retval;

	rs = &rig->state;
	priv = (struct icom_priv_data*)rs->priv;

	ep_len = 0;	/* 0 implies BCD data */
	val_len = 1;

	switch(token) {
	case TOK_SSBBASS:
		ep_menu = 0x01;
		icom_val = val.f;
		break;
	case TOK_MEMNAME:
		ep_menu = 0x14;
		icom_val = val.i ? 1 : 0;
		break;
	case TOK_SQLCTRL:
		ep_menu = 0x22;
		/* TODO: check range */
		icom_val = val.i;
		break;
	case TOK_MYCALL:	/* max 10 ASCII char */
		ep_len = strlen(val.cs);
		if (ep_len > 10)
			return -RIG_EINVAL;
		ep_menu = 0x15;
		memcpy(epbuf+1, val.cs, ep_len);
		break;
	default:
		return -RIG_EINVAL;
	}

	epbuf[0] = ep_menu;
	if (ep_len++ == 0) {
		to_bcd_be(epbuf+1, (long long)icom_val, val_len*2);
		ep_len += val_len;
	}

	retval = icom_transaction (rig, C_CTL_MEM, 0x05, epbuf, ep_len,
						ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 1 || ackbuf[0] != ACK) {
		rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), "
			"len=%d\n", __FUNCTION__, ackbuf[0], ack_len);
		return -RIG_ERJCTED;
	}
	return RIG_OK;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
static int ic756pro2_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
	struct icom_priv_data *priv;
	struct rig_state *rs;

	rs = &rig->state;
	priv = (struct icom_priv_data*)rs->priv;

	switch(token) {
	default:
		return -RIG_ENIMPL;
	}
	return RIG_OK;
}



/*
 * ic756proIII rig capabilities.
 *
 * TODO: check every paramters,
 * 		add antenna capabilities
 */
static const struct icom_priv_caps ic756pro3_priv_caps = { 
		0x6e,	/* default address */
		0,		/* 731 mode */
		ic756_ts_sc_list
};


#define IC756PROIII_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROIII_1HZ_TS_MODES IC756PROIII_ALL_RX_MODES
#define IC756PROIII_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROIII_AM_TX_MODES (RIG_MODE_AM)


const struct rig_caps ic756pro3_caps = {
.rig_model =  RIG_MODEL_IC756PROIII,
.model_name = "IC-756PROIII", 
.mfg_name =  "Icom", 
.version =  BACKEND_VER, 
.copyright =  "LGPL",
.status =  RIG_STATUS_NEW,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE, 
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3, 
.has_get_func =  IC756PRO_FUNC_ALL,
.has_set_func =  IC756PRO_FUNC_ALL, 
.has_get_level =  IC756PRO_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(IC756PRO_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,	/* FIXME: parms */
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
},
.parm_gran =  {},
.extparms =  ic756pro2_ext_parms,
.ctcss_list =  common_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 10, 20, RIG_DBLST_END, },	/* FIXME: TBC */
.attenuator =   { 6, 12, 18, 20, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(9999),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC756_VFO_OPS,
.scan_ops =  IC756_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
	   {   1,  99, RIG_MTYPE_MEM  },
	   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
	   RIG_CHAN_END,
	},

.rx_range_list1 =   { {kHz(30),MHz(60),IC756PROIII_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =   {
	FRQ_RNG_HF(1,IC756PROIII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(1,IC756PROIII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(1,IC756PROIII_AM_TX_MODES, W(5),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(1,IC756PROIII_AM_TX_MODES, W(5),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
    	RIG_FRNG_END, },

.rx_range_list2 =   { {kHz(30),MHz(60),IC756PROIII_ALL_RX_MODES,-1,-1,IC756_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC756PROIII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_6m(2,IC756PROIII_OTHER_TX_MODES, W(5),W(100),IC756_VFO_ALL,IC756_ANTS),
	FRQ_RNG_HF(2,IC756PROIII_AM_TX_MODES, W(5),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
	FRQ_RNG_6m(2,IC756PROIII_AM_TX_MODES, W(5),W(40),IC756_VFO_ALL,IC756_ANTS),   /* AM class */
    	RIG_FRNG_END, },

.tuning_steps = 	{
	 {IC756PROIII_1HZ_TS_MODES,1},
	 {IC756PROIII_ALL_RX_MODES,kHz(1)},
	 {IC756PROIII_ALL_RX_MODES,kHz(5)},
	 {IC756PROIII_ALL_RX_MODES,kHz(9)},
	 {IC756PROIII_ALL_RX_MODES,kHz(10)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
	{RIG_MODE_SSB|RIG_MODE_RTTY, kHz(2.4)},
	{RIG_MODE_CW, Hz(500)},
	{RIG_MODE_AM, kHz(6)},
	{RIG_MODE_AM, kHz(2.4)},
	{RIG_MODE_FM, kHz(15)},
	{RIG_MODE_FM, kHz(8)},
	RIG_FLT_END,
	},
.str_cal = IC756PRO_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic756pro3_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,

.decode_event =  icom_decode_event,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.set_ptt =  icom_set_ptt,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_ctcss_tone =  icom_set_ctcss_tone,
.get_ctcss_tone =  icom_get_ctcss_tone,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  icom_get_split_vfo,

.set_ext_parm =  ic756pro2_set_ext_parm,
.get_ext_parm =  ic756pro2_get_ext_parm,
};


