/*
*  Hamlib Kenwood backend - TS850 description
*  Copyright (c) 2000-2004 by Stephane Fillod
*
*	$Id: ts850.c,v 1.24 2007-08-18 20:04:56 aa6e Exp $
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
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <hamlib/rig.h>
#include <cal.h>
#include "kenwood.h"


#define TS850_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS850_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS850_AM_TX_MODES (RIG_MODE_AM)

#define TS850_FUNC_ALL (RIG_FUNC_AIP|RIG_FUNC_LOCK)

#define TS850_LEVEL_GET (RIG_LEVEL_SWR|RIG_LEVEL_COMP|RIG_LEVEL_ALC|RIG_LEVEL_CWPITCH|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_SLOPE_LOW|RIG_LEVEL_SLOPE_HIGH)
#define TS850_LEVEL_SET (RIG_LEVEL_CWPITCH|RIG_LEVEL_SLOPE_LOW|RIG_LEVEL_SLOPE_HIGH)

#define TS850_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS850_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)

#define TS850_CHANNEL_CAPS \
.freq=1,\
.mode=1,\
.tx_freq=1,\
.tx_mode=1,\
.split=1,\
.ctcss_tone=1

#define TS850_STR_CAL { 4, \
	{ \
		{   0, -54 }, \
		{  15, 0 }, \
		{  22,30 }, \
		{  30, 66}, \
	} }


static const struct kenwood_priv_caps  ts850_priv_caps  = {
	.cmdtrm =  EOM_KEN,
};

/* forward definitions */
static int ts850_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
static int ts850_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit);
static int ts850_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ts850_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ts850_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int ts850_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ts850_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int ts850_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ts850_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int ts850_get_mem(RIG *rig, vfo_t vfo, int *ch);
static int ts850_get_channel (RIG * rig, channel_t * chan);
static int ts850_set_channel (RIG * rig, const channel_t * chan);

/*
* ts850 rig capabilities.
* Notice that some rigs share the same functions.
* Also this struct is READONLY!
*/
const struct rig_caps ts850_caps = {
	.rig_model =  RIG_MODEL_TS850,
	.model_name = "TS-850",
	.mfg_name =  "Kenwood",
	.version =  BACKEND_VER ".0",
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
	.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
	.write_delay =  0,
	.post_write_delay =  100,
	.timeout =  480000,  // When you tune a Kenwood, the reply is delayed until you stop.
	.retry =  0,
	
	.has_get_func =  TS850_FUNC_ALL,
	.has_set_func =  TS850_FUNC_ALL,
	.has_get_level =  TS850_LEVEL_GET,
	.has_set_level =  TS850_LEVEL_SET,
	.has_get_parm =  RIG_PARM_NONE,
	.has_set_parm =  RIG_PARM_NONE,
	.level_gran =  {},            
	.parm_gran =  {},
	.ctcss_list =  kenwood38_ctcss_list,
	.dcs_list =  NULL,
	.preamp =   { RIG_DBLST_END, },
	.attenuator =   { RIG_DBLST_END, },
	.max_rit =  kHz(1.27),
	.max_xit =  kHz(1.27),
	.max_ifshift =  Hz(0),
	.vfo_ops=TS850_VFO_OPS,
	.targetable_vfo =  RIG_TARGETABLE_FREQ,
	.transceive =  RIG_TRN_RIG,
	.bank_qty =   0,
	.chan_desc_sz =  3,
	.chan_list =  {
		{  0, 89, RIG_MTYPE_MEM ,{TS850_CHANNEL_CAPS}},
		{ 90, 99, RIG_MTYPE_EDGE ,{TS850_CHANNEL_CAPS}},
		RIG_CHAN_END,
	},
	
	.rx_range_list1 =  { 
		{kHz(100),MHz(30),TS850_ALL_MODES,-1,-1,TS850_VFO},
		RIG_FRNG_END,
	}, /* rx range */
	.tx_range_list1 =  { 
		{kHz(1810),kHz(1850),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},	/* 100W class */
		{kHz(1810),kHz(1850),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},		/* 40W class */
		{kHz(3500),kHz(3800),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(3500),kHz(3800),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(7),kHz(7100),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(7),kHz(7100),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{kHz(10100),kHz(10150),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(10100),kHz(10150),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(14),kHz(14350),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(14),kHz(14350),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{kHz(18068),kHz(18168),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(18068),kHz(18168),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(21),kHz(21450),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(21),kHz(21450),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{kHz(24890),kHz(24990),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(24890),kHz(24990),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(28),kHz(29700),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(28),kHz(29700),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		RIG_FRNG_END,
	},
	
	.rx_range_list2 =  {
		{kHz(100),MHz(30),TS850_ALL_MODES,-1,-1,TS850_VFO},
		RIG_FRNG_END,
	}, /* rx range */
	.tx_range_list2 =  {
		{kHz(1800),MHz(2)-1,TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},	/* 100W class */
		{kHz(1800),MHz(2)-1,TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},		/* 40W class */
		{kHz(3500),MHz(4)-1,TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(3500),MHz(4)-1,TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(7),kHz(7300),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(7),kHz(7300),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{kHz(10100),kHz(10150),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(10100),kHz(10150),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(14),kHz(14350),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(14),kHz(14350),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{kHz(18068),kHz(18168),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(18068),kHz(18168),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(21),kHz(21450),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(21),kHz(21450),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{kHz(24890),kHz(24990),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{kHz(24890),kHz(24990),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		{MHz(28),kHz(29700),TS850_OTHER_TX_MODES,W(5),W(100),TS850_VFO},
		{MHz(28),kHz(29700),TS850_AM_TX_MODES,W(2),W(40),TS850_VFO},
		RIG_FRNG_END,
	}, /* tx range */
	.tuning_steps =  {
		{TS850_ALL_MODES,0},	/* any tuning step */
		RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
	.filters =  {
		{TS850_ALL_MODES, kHz(12)},
		{TS850_ALL_MODES, kHz(6)},
		{TS850_ALL_MODES, kHz(2.7)},
		{TS850_ALL_MODES, Hz(500)},
		{TS850_ALL_MODES, Hz(250)},
		RIG_FLT_END,
	},
	.str_cal = TS850_STR_CAL,
	.priv =  (void *)&ts850_priv_caps,
	
	.set_freq =  kenwood_set_freq,
	.get_freq =  kenwood_get_freq,
	.set_rit =  ts850_set_rit,
	.get_rit =  kenwood_get_rit,
	.set_xit =  ts850_set_xit,
	.get_xit =  kenwood_get_xit,
	.set_mode =  ts850_set_mode,
	.get_mode =  ts850_get_mode,
	.set_vfo =  kenwood_set_vfo,
	.get_vfo =  kenwood_get_vfo,
	.set_split_vfo =  kenwood_set_split_vfo,
	.set_ctcss_tone =  ts850_set_ctcss_tone,
	.get_ctcss_tone =  kenwood_get_ctcss_tone,
	.get_ptt =  kenwood_get_ptt,
	.set_ptt =  kenwood_set_ptt,
	.set_func =  ts850_set_func,
	.get_func =  ts850_get_func,
	.set_level =  ts850_set_level,
	.get_level =  ts850_get_level,
	.vfo_op =  kenwood_vfo_op,
	.set_mem =  kenwood_set_mem,
	.get_mem =  ts850_get_mem,
	.get_channel = ts850_get_channel,
	.set_channel = ts850_set_channel,
	.set_trn =  kenwood_set_trn
};

/*
* Function definitions below
*/

int ts850_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
	char buf[50], infobuf[50];
	unsigned char c;
	int retval, len, i;
	size_t info_len;
	
	info_len = 0;
	if (rit == 0)
		kenwood_transaction(rig, "RT0;", 4, infobuf, &info_len);
	else
		kenwood_transaction(rig, "RT1;", 4, infobuf, &info_len);
	
	if (rit > 0)
		c = 'U';
	else
		c = 'D';
	len = sprintf(buf, "R%c;", c);
	
	info_len = 0;
	retval = kenwood_transaction(rig, "RC;", 3, infobuf, &info_len);
	for (i = 0; i < abs(rint(rit/20)); i++)
	{
		info_len = 0;
		retval = kenwood_transaction(rig, buf, len, infobuf, &info_len);
	}
	
	return RIG_OK;
}

int ts850_set_xit(RIG * rig, vfo_t vfo, shortfreq_t xit)
{
	char buf[50], infobuf[50];
	unsigned char c;
	int retval, len, i;
	size_t info_len;
	
	info_len = 0;
	if (xit == 0)
		kenwood_transaction(rig, "XT0;", 4, infobuf, &info_len);
	else
		kenwood_transaction(rig, "XT1;", 4, infobuf, &info_len);
	
	info_len = 0;
	retval = kenwood_transaction(rig, "RC;", 3, infobuf, &info_len);
	if (xit > 0)
		c = 'U';
	else
		c = 'D';
	len = sprintf(buf, "R%c;", c);
	
	for (i = 0; i < abs(rint(xit/20)); i++)
	{
		info_len = 0;
		retval = kenwood_transaction(rig, buf, len, infobuf, &info_len);
	}
	
	return RIG_OK;
}

/*
* modes in use by the "MD" command
*/
#define MD_NONE '0'
#define MD_LSB  '1'
#define MD_USB  '2'
#define MD_CW   '3'
#define MD_FM   '4'
#define MD_AM   '5'
#define MD_FSK  '6'
#define MD_CWR  '7'
#define MD_FSKR '9'


int ts850_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	char infobuf[50];
	size_t info_len;
	int f,f1,f2,retval;
	
	info_len = 50;
	retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len)
		;
	if (retval != RIG_OK)
		return retval;
	
	if (info_len != 38 || infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"ts850_get_mode: wrong answer len=%d\n", info_len);
		return -RIG_ERJCTED;
	}
	
	switch (infobuf[29]) {
		case MD_CW:     *mode = RIG_MODE_CW; break;
		case MD_CWR:    *mode = RIG_MODE_CWR; break;
		case MD_USB:    *mode = RIG_MODE_USB; break;
		case MD_LSB:    *mode = RIG_MODE_LSB; break;
		case MD_FM:     *mode = RIG_MODE_FM; break;
		case MD_AM:     *mode = RIG_MODE_AM; break;
		case MD_FSK:    *mode = RIG_MODE_RTTY; break;
		case MD_FSKR:   *mode = RIG_MODE_RTTYR; break;
		case MD_NONE:   *mode = RIG_MODE_NONE; break;
		default:
			rig_debug(RIG_DEBUG_ERR,"ts850_get_mode: unsupported mode '%c'\n", infobuf[29]);
		return -RIG_EINVAL;
	}
	
	info_len = 50;
	retval = kenwood_transaction (rig, "FL;", 3, infobuf, &info_len)
		;
	if (retval != RIG_OK)
		return retval;
	
	if (info_len != 9 || infobuf[1] != 'L') {
		rig_debug(RIG_DEBUG_ERR,"ts850_get_mode: wrong answer len=%d\n", info_len);
		return -RIG_ERJCTED;
	}
	
	infobuf[8]='\0';
	f2=atoi(&infobuf[5]);
	infobuf[5]='\0';
	f1=atoi(&infobuf[2]);
	
	if(f2>f1) f=f2;
	else f=f1;
	
	switch(f) {
		case 2:
			*width=kHz(12);
		break;
		case 3:
		case 5:
			*width=kHz(6);
		break;
		case 7:
			*width=kHz(2.7);
		break;
		case 9:
			*width=Hz(500);
		break;
		case 10:
			*width=Hz(250);
		break;
	}
	
	return RIG_OK;
}

static char mode_to_char(rmode_t mode)
{
	switch (mode) {
		case RIG_MODE_CW:       return(MD_CW);
		case RIG_MODE_CWR:      return(MD_CWR);
		case RIG_MODE_USB:      return(MD_USB);
		case RIG_MODE_LSB:      return(MD_LSB);
		case RIG_MODE_FM:       return(MD_FM);
		case RIG_MODE_AM:       return(MD_AM);
		case RIG_MODE_RTTY:     return(MD_FSK);
		case RIG_MODE_RTTYR:    return(MD_FSKR);
		default:
			rig_debug(RIG_DEBUG_WARN,"%s: unsupported mode %d\n", __FUNCTION__,mode);
	}
	return(RIG_MODE_NONE);
}

int ts850_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	char mdbuf[16],ackbuf[16];
	int mdbuf_len, kmode, retval;
	size_t ack_len;
	
	kmode=mode_to_char(mode);
	if(kmode==RIG_MODE_NONE) {
		rig_debug(RIG_DEBUG_ERR,"ts850_set_mode: " "unsupported mode %d\n", mode);
		return -RIG_EINVAL;
	}
	
	mdbuf_len = sprintf(mdbuf, "MD%c;", kmode);
	ack_len = 0;
	retval = kenwood_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);
	
	if (retval != RIG_OK)
		return retval;
	
	ack_len = 0;
	if(width <= Hz(250)) 
		mdbuf_len = sprintf(mdbuf, "FL010009;");
	else
		if(width <= Hz(500)) 
		mdbuf_len = sprintf(mdbuf, "FL009009;");
	else
		if(width <= kHz(2.7)) 
		mdbuf_len = sprintf(mdbuf, "FL007007;");
	else
		if(width <= kHz(6)) 
		mdbuf_len = sprintf(mdbuf, "FL005005;");
	else
		mdbuf_len = sprintf(mdbuf, "FL002002;");
	
	retval = kenwood_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);
	
	return retval;
}

int ts850_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
	const struct rig_caps *caps;
	char tonebuf[16], ackbuf[16];
	int i, tone_len;
	size_t ack_len;
	
	caps = rig->caps;
	
	for (i = 0; caps->ctcss_list[i] != 0 && i<38; i++) {
		if (caps->ctcss_list[i] == tone)
			break;
	}
	if (caps->ctcss_list[i] != tone)
		return -RIG_EINVAL;
	
	tone_len = sprintf(tonebuf,"TN%03d;", i+1);
	
	ack_len = 0;
	return kenwood_transaction (rig, tonebuf, tone_len, ackbuf, &ack_len);
}

int ts850_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
	char fctbuf[16], ackbuf[16];
	int fct_len;
	size_t ack_len;
	
	if (vfo != RIG_VFO_CURR) 
		return -RIG_EINVAL;
	
	ack_len = 0;
	switch (func) {
		case RIG_FUNC_AIP:
			fct_len = sprintf(fctbuf,"MX%c;", status?'1':'0');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);
		
		case RIG_FUNC_LOCK:
			fct_len = sprintf(fctbuf,"LK%c;", status?'1':'0');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);
		
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_func %#x", func);
		return -RIG_EINVAL;
	}                                                           
	return RIG_OK;
}


int ts850_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
	char fctbuf[16], ackbuf[50];
	int retval, fct_len;
	size_t ack_len;
	
	if (vfo != RIG_VFO_CURR) 
		return -RIG_EINVAL;
	
	switch (func) {
		case RIG_FUNC_AIP:
			fct_len = sprintf(fctbuf,"MX;");
		break;
		case RIG_FUNC_LOCK:
			fct_len = sprintf(fctbuf,"LK;");
		break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_func %#x", func);
		return -RIG_EINVAL;
	}
	
	ack_len = 50;
	retval = kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);
	
	if (retval != RIG_OK)
		return retval;
	
	*status=ackbuf[2]=='1'?1:0;
	
	return RIG_OK;
}

int ts850_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	char lvlbuf[50];
	int i, retval;
	size_t lvl_len;
	
	if(vfo!=RIG_VFO_CURR)
		return -RIG_EINVAL;
	
	switch (level) {
		case RIG_LEVEL_RAWSTR:
		lvl_len = 50;
		retval = kenwood_transaction (rig, "SM;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[6]='\0';
		val->i=atoi(&lvlbuf[2]);
		break;
		
		case RIG_LEVEL_STRENGTH:
		lvl_len = 50;
		retval = kenwood_transaction (rig, "SM;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[6]='\0';
		val->i=atoi(&lvlbuf[2]);
		//val->i = (val->i * 4) - 54;  Old approximate way of doing it.
		val->i = (int)rig_raw2val(val->i,&rig->caps->str_cal);
		break;
		
		case RIG_LEVEL_SWR:
			lvl_len = 0;
		retval = kenwood_transaction (rig, "RM1;", 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvl_len = 50;
		retval = kenwood_transaction (rig, "RM;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[7]='\0';
		i=atoi(&lvlbuf[3]);
		if(i == 30) 
			val->f = 150.0; /* infinity :-) */
		else
			val->f = 60.0/(30.0-(float)i)-1.0;
		break;
		
		case RIG_LEVEL_COMP:
			lvl_len = 0;
		retval = kenwood_transaction (rig, "RM2;", 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvl_len = 50;
		retval = kenwood_transaction (rig, "RM;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[7]='\0';
		val->f=(float)atoi(&lvlbuf[3])/30.0;
		break;
		
		case RIG_LEVEL_ALC:
			lvl_len = 0;
		retval = kenwood_transaction (rig, "RM3;", 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvl_len = 50;
		retval = kenwood_transaction (rig, "RM;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[7]='\0';
		val->f=(float)atoi(&lvlbuf[3])/30.0;
		break;
		
		case RIG_LEVEL_CWPITCH:
			lvl_len = 25;
		retval = kenwood_transaction (rig, "PT;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[4]='\0';
		val->i=atoi(&lvlbuf[2]);
		val->i=(val->i-8)*50+800;
		break;
		
		case RIG_LEVEL_SLOPE_LOW:
			lvl_len = 50;
		retval = kenwood_transaction (rig, "SL;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[4]='\0';
		val->i=atoi(&lvlbuf[2]);
		break;
		
		case RIG_LEVEL_SLOPE_HIGH:
			lvl_len = 50;
		retval = kenwood_transaction (rig, "SH;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[4]='\0';
		val->i=atoi(&lvlbuf[2]);
		break;
		
		default:
			return kenwood_get_level (rig, vfo, level, val);
	}
	return retval; // Never gets here.
}

int ts850_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	char lvlbuf[16], ackbuf[16];
	int lvl_len;
	size_t ack_len;
	
	switch (level) {
		case RIG_LEVEL_CWPITCH:
			if(val.i>1000 || val.i<400)
			return -RIG_EINVAL;	
		ack_len=0;
		lvl_len = sprintf(lvlbuf,"PT%02d;",(val.i/50)-8);
		return  kenwood_transaction (rig, lvlbuf, lvl_len, ackbuf, &ack_len);
		break;
		
		case RIG_LEVEL_SLOPE_HIGH:
			if(val.i>20 || val.i < 0)
			return -RIG_EINVAL;
		ack_len=0;
		lvl_len = sprintf(lvlbuf,"SH%02d;",(val.i));
		return  kenwood_transaction (rig, lvlbuf, lvl_len, ackbuf, &ack_len);
		break;
		
		case RIG_LEVEL_SLOPE_LOW:
			if(val.i>20 || val.i < 0)
			return -RIG_EINVAL;
		ack_len=0;
		lvl_len = sprintf(lvlbuf,"SL%02d;",(val.i));
		return  kenwood_transaction (rig, lvlbuf, lvl_len, ackbuf, &ack_len);
		break;
		
		default:
			return kenwood_set_level (rig, vfo, level, val);
    }
  
  return RIG_OK;		/* never reached */
}

int ts850_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	char infobuf[50];
	size_t info_len;
	int retval;
	
	info_len = 50;
	retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len)
		;
	if (retval != RIG_OK)
		return retval;
	
	if (info_len != 38 || infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"ts850_get_mem: wrong answer len=%d\n", info_len);
		return -RIG_ERJCTED;
	}
	
	infobuf[28]='\0';
	*ch=atoi(&infobuf[26]);
	
	return RIG_OK;
}

static rmode_t char_to_mode(char c)
{
	switch (c) {
		case MD_CW:     return(RIG_MODE_CW);
		case MD_CWR:    return(RIG_MODE_CWR);
		case MD_USB:    return(RIG_MODE_USB);
		case MD_LSB:    return(RIG_MODE_LSB);
		case MD_FM:     return(RIG_MODE_FM);
		case MD_AM:     return(RIG_MODE_AM);
		case MD_FSK:    return(RIG_MODE_RTTY);
		case MD_FSKR:   return(RIG_MODE_RTTYR);
		case MD_NONE:   return(RIG_MODE_NONE);
	}
	return(RIG_MODE_NONE);
}

int ts850_get_channel (RIG * rig, channel_t * chan)
{
	char cmdbuf[16], membuf[50];
	int retval, cmd_len, num;
	size_t mem_len;
	
	num=chan->channel_num;
	
	cmd_len = sprintf(cmdbuf, "MR0 %02d;", num);
	mem_len = 50;
	retval = kenwood_transaction (rig, cmdbuf, cmd_len, membuf, &mem_len);
	if (retval != RIG_OK)
		return retval;
	
	memset(chan,0,sizeof(channel_t));
	chan->channel_num=num;
	
	
	if (membuf[19]=='0')
		chan->ctcss_tone=0;
	else {
		membuf[22]='\0';
		chan->ctcss_tone= rig->caps->ctcss_list[atoi(&membuf[20])];
	}
	
	chan->mode=char_to_mode(membuf[17]);
	
	membuf[17]='\0';
	chan->freq=atoi(&membuf[6]);
	
	cmd_len = sprintf(cmdbuf, "MR1 %02d;", num);
	mem_len = 50;
	retval = kenwood_transaction (rig, cmdbuf, cmd_len, membuf, &mem_len);
	if (retval != RIG_OK)
		return retval;
	
	chan->tx_mode=char_to_mode(membuf[17]);
	membuf[17]='\0';
	chan->tx_freq=atoi(&membuf[6]);
	
	if(chan->freq==chan->tx_freq &&
	   chan->mode==chan->tx_mode) {
		   chan->tx_freq=RIG_FREQ_NONE;
		   chan->tx_mode=RIG_MODE_NONE;
		   chan->split=RIG_SPLIT_OFF;
	   } else
		chan->split=RIG_SPLIT_ON;
	
	return RIG_OK;
}


int ts850_set_channel (RIG * rig, const channel_t * chan)
{
	char cmdbuf[30], membuf[30];
	int retval, cmd_len;
	size_t mem_len;
	int num,freq,tx_freq,tone;
	char mode,tx_mode,split,tones;
	
	num=chan->channel_num;
	freq=(int)chan->freq;
	mode=mode_to_char(chan->mode);
	if(chan->split==RIG_SPLIT_ON) {
		tx_freq=(int)chan->tx_freq;
		tx_mode=mode_to_char(chan->tx_mode);
		split='1';
	} else {
		tx_freq=0;
		tx_mode='\0';
		split='0';
	}
	
	for (tone = 1; rig->caps->ctcss_list[tone-1] != 0 && tone<39; tone++) {
		if (rig->caps->ctcss_list[tone-1] == chan->ctcss_tone)
			break;
	}
	if(chan->ctcss_tone!=0) {
		tones='1';
	} else {
		tones='0';
		tone=0;
	}
	
	cmd_len = sprintf(cmdbuf, "MW0 %02d%011d%c0%c%02d ;",
					  num,freq,mode,tones,tone);
	mem_len = 0;
	retval = kenwood_transaction (rig, cmdbuf, cmd_len, membuf, &mem_len);
	if (retval != RIG_OK)
		return retval;
	
	cmd_len = sprintf(cmdbuf, "MW1 %02d%011d%c0%c%02d ;",
					  num,tx_freq,tx_mode,tones,tone);
	mem_len = 0;
	retval = kenwood_transaction (rig, cmdbuf, cmd_len, membuf, &mem_len);
	if (retval != RIG_OK)
		return retval;
	
	return RIG_OK;
}
