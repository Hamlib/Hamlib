/*
 *  Hamlib TenTenc backend - TT-565 description
 *  Copyright (c) 2004-2005 by Stephane Fillod
 *
 *	$Id: orion.c,v 1.3 2005-01-25 00:21:29 fillods Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include <hamlib/rig.h>
#include "bandplan.h"
#include "serial.h"
#include "misc.h"

#include "tentec.h"


/*
 * Mem caps, to be checked..
 */
#define TT565_MEM_CAP {        \
	.freq = 1,      \
	.mode = 1,      \
	.width = 1,     \
	.tx_freq = 1,	\
	.tx_mode = 1,	\
	.tx_width = 1,	\
	.split = 1,	\
}

static int tt565_init(RIG *rig);
static int tt565_cleanup(RIG *rig);
static int tt565_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt565_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt565_set_vfo(RIG *rig, vfo_t vfo);
static int tt565_get_vfo(RIG *rig, vfo_t *vfo);
static int tt565_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int tt565_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt565_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int tt565_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
static int tt565_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int tt565_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int tt565_reset(RIG *rig, reset_t reset);
static int tt565_set_mem(RIG * rig, vfo_t vfo, int ch);
static int tt565_get_mem(RIG * rig, vfo_t vfo, int *ch);
static int tt565_vfo_op(RIG * rig, vfo_t vfo, vfo_op_t op);
static int tt565_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
static int tt565_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
static int tt565_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
static int tt565_get_rit(RIG * rig, vfo_t vfo, shortfreq_t *rit);
static int tt565_set_xit(RIG * rig, vfo_t vfo, shortfreq_t xit);
static int tt565_get_xit(RIG * rig, vfo_t vfo, shortfreq_t *xit);
static int tt565_set_level(RIG * rig, vfo_t vfo, setting_t level, value_t val);
static int tt565_get_level(RIG * rig, vfo_t vfo, setting_t level, value_t *val);
static const char* tt565_get_info(RIG *rig);

struct tt565_priv_data {
	int ch;		/* mem */
	vfo_t vfo_curr;
};


#define TT565_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
			RIG_MODE_RTTY|RIG_MODE_AM)
#define TT565_RXMODES (TT565_MODES)

#define TT565_FUNCS (RIG_FUNC_LOCK|RIG_FUNC_TUNER)

#define TT565_LEVELS (RIG_LEVEL_RAWSTR|/*RIG_LEVEL_NB|*/ \
				RIG_LEVEL_SQL|RIG_LEVEL_IF| \
				RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD| \
				RIG_LEVEL_RF|RIG_LEVEL_NR| \
				/*RIG_LEVEL_ANF|*/RIG_LEVEL_MICGAIN| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOX| \
				RIG_LEVEL_COMP|RIG_LEVEL_PREAMP| \
				RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT565_ANTS (RIG_ANT_1|RIG_ANT_2) 
#define TT565_RXANTS (TT565_ANTS|RIG_ANT_3) 

#define TT565_PARMS (RIG_PARM_NONE)

#define TT565_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT565_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|\
		RIG_OP_TO_VFO|RIG_OP_FROM_VFO| \
		RIG_OP_TUNE)


/*
 * tt565 transceiver capabilities.
 *
 * Protocol is documented at 
 *		http://www.rfsquared.com/
 */
const struct rig_caps tt565_caps = {
.rig_model =  RIG_MODEL_TT565,
.model_name = "TT-565 Orion",
.mfg_name =  "Ten-Tec",
.version =  "0.2",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  57600,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  400,
.retry =  3,

.has_get_func =  TT565_FUNCS,
.has_set_func =  TT565_FUNCS,
.has_get_level =  TT565_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT565_LEVELS),
.has_get_parm =  TT565_PARMS,
.has_set_parm =  TT565_PARMS,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 20, RIG_DBLST_END },	/* TBC */
.attenuator =   { 6, 12, 18, RIG_DBLST_END },
.max_rit =  kHz(8),
.max_xit =  kHz(8),
.max_ifshift =  kHz(8),
.targetable_vfo =  RIG_TARGETABLE_ALL,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
		{   0, 199, RIG_MTYPE_MEM, TT565_MEM_CAP },
		},

.rx_range_list1 =  {
	FRQ_RNG_HF(1,TT565_RXMODES, -1,-1,RIG_VFO_N(0),TT565_RXANTS),
	{kHz(500),MHz(30),TT565_RXMODES,-1,-1,RIG_VFO_N(1),TT565_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS),
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	FRQ_RNG_HF(2,TT565_RXMODES, -1,-1,RIG_VFO_N(0),TT565_RXANTS),
	{MHz(5.25),MHz(5.40),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(500),MHz(30),TT565_RXMODES,-1,-1,RIG_VFO_N(1),TT565_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS),
	{MHz(5.25),MHz(5.40),TT565_MODES,W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT565_RXMODES,1},
	 {TT565_RXMODES,10},
	 {TT565_RXMODES,100},
	 {TT565_RXMODES,kHz(1)},
	 {TT565_RXMODES,kHz(10)},
	 {TT565_RXMODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
	/* 9MHz IF filters: 15kHz, 6kHz, 2.4kHz, 1.0kHz */
	/* opt: 1.8kHz, 500Hz, 250Hz */
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, 100},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, kHz(6)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, 0}, /* 590 filters */
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_AM, kHz(4)},
		{RIG_MODE_FM, kHz(15)},
		RIG_FLT_END,
	},
.priv =  (void*)NULL,

.rig_init =  tt565_init,
.rig_cleanup =  tt565_cleanup,

.set_freq =  tt565_set_freq,
.get_freq =  tt565_get_freq,
.set_vfo =  tt565_set_vfo,
.get_vfo =  tt565_get_vfo,
.set_mode =  tt565_set_mode,
.get_mode =  tt565_get_mode,
.set_split_vfo =  tt565_set_split_vfo,
.get_split_vfo =  tt565_get_split_vfo,
.set_level =  tt565_set_level,
.get_level =  tt565_get_level,
.set_mem =  tt565_set_mem,
.get_mem =  tt565_get_mem,
.set_ptt =  tt565_set_ptt,
.get_ptt =  tt565_get_ptt,
.vfo_op =  tt565_vfo_op,
.set_ts =  tt565_set_ts,
.get_ts =  tt565_get_ts,
.set_rit =  tt565_set_rit,
.get_rit =  tt565_get_rit,
.set_xit =  tt565_set_xit,
.get_xit =  tt565_get_xit,
.reset =  tt565_reset,
.get_info =  tt565_get_info,

};

/*
 * Function definitions below
 */


#define EOM "\015"	/* CR */

#define TT565_USB '0'
#define TT565_LSB '1'
#define TT565_CW  '2'
#define TT565_CWR '3'
#define TT565_AM  '4'
#define TT565_FM  '5'
#define TT565_RTTY '6'



/*************************************************************************************
 *
 * Specs from http://www.rfsquared.com, Rev 1, firmware 1.340
 *
 * 	[sg]et_func
 * 	[sg]et_ant
 *
 * 	XCHG
 */


/*
 * tt565_init:
 * Basically, it just sets up *priv
 */
int tt565_init(RIG *rig)
{
	struct tt565_priv_data *priv;

	priv = (struct tt565_priv_data*)malloc(sizeof(struct tt565_priv_data));

	if (!priv) {
				/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	memset(priv, 0, sizeof(struct tt565_priv_data));

	/*
	 * set arbitrary initial status
	 */
	priv->ch = 0;

	rig->state.priv = (rig_ptr_t)priv;

	return RIG_OK;
}

/*
 * tt565_cleanup routine
 * the serial port is closed by the frontend
 */
int tt565_cleanup(RIG *rig)
{
	if (rig->state.priv)
		free(rig->state.priv);

	rig->state.priv = NULL;

	return RIG_OK;
}

static char which_receiver(const RIG *rig, vfo_t vfo)
{
	struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

	if (vfo == RIG_VFO_CURR)
		vfo = priv->vfo_curr;

	switch (vfo) {
	case RIG_VFO_A:
	case RIG_VFO_B:
	case RIG_VFO_MAIN: return 'M';
	case RIG_VFO_SUB: return 'S';
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported Receiver %s\n",
				__FUNCTION__, rig_strvfo(vfo));
		return -RIG_EINVAL;
	}
}

static char which_vfo(const RIG *rig, vfo_t vfo)
{
	struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

	if (vfo == RIG_VFO_CURR)
		vfo = priv->vfo_curr;

	switch (vfo) {
	case RIG_VFO_A: return 'A';
	case RIG_VFO_B: return 'B';
	case RIG_VFO_NONE: return 'N';
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, rig_strvfo(vfo));
		return -RIG_EINVAL;
	}
}


/*
 * tt565_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */
int tt565_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	int cmd_len, retval;
	unsigned char cmdbuf[16];

	cmd_len = sprintf (cmdbuf, "*%cF%"PRIll EOM, 
			which_vfo(rig, vfo),
			(long long)freq);

	retval = tentec_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

/*
 * tt565_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt565_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];

	cmd_len = sprintf(cmdbuf, "?%cF" EOM,
				which_vfo(rig, vfo));

	retval = tentec_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[2] != 'F' || resp_len <= 3) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	*freq = (freq_t) atof(respbuf+3);

	return RIG_OK;
}

/*
 * tt565_set_vfo
 * Assumes rig!=NULL
 */
int tt565_set_vfo(RIG *rig, vfo_t vfo)
{
	struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;
	int vfo_len;
	unsigned char vfobuf[16];

	if (vfo == RIG_VFO_CURR)
		return RIG_OK;

	if (vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB) {
		vfo_len = sprintf (vfobuf, "*K%c" EOM, 
				vfo == RIG_VFO_SUB ? 'S' : 'M');

		return tentec_transaction (rig, vfobuf, vfo_len, NULL, NULL);
	}

	priv->vfo_curr = vfo;

	return RIG_OK;
}

/*
 * tt565_get_vfo
 * Assumes rig!=NULL
 */
int tt565_get_vfo(RIG *rig, vfo_t *vfo)
{
	struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

	*vfo = priv->vfo_curr;

	return RIG_OK;
}



/*
 * tt565_set_split_vfo
 * Assumes rig!=NULL
 */
int tt565_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
	int cmd_len, retval;
	unsigned char cmdbuf[16];

	cmd_len = sprintf (cmdbuf, "*KV%c%c%c" EOM, 
			which_vfo(rig, vfo),
			'N',			/* FIXME */
			which_vfo(rig, tx_vfo));

	retval = tentec_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

static vfo_t tt2vfo(char c)
{
	switch(c) {
	case 'A': return RIG_VFO_A;
	case 'B': return RIG_VFO_B;
	case 'N': return RIG_VFO_NONE;
	}
	return RIG_VFO_NONE;
}

/*
 * tt565_get_split_vfo
 * Assumes rig!=NULL
 */
int tt565_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];
	char ttreceiver;

	cmd_len = sprintf(cmdbuf, "?KV" EOM);
	retval = tentec_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[2] != 'V' || resp_len < 5) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	ttreceiver = vfo == RIG_VFO_SUB ? respbuf[3] : respbuf[4];

	*tx_vfo = tt2vfo(respbuf[5]);

	*split = ttreceiver == respbuf[5] ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

	return RIG_OK;
}


/*
 * tt565_set_mode
 * Assumes rig!=NULL
 */
int tt565_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	struct rig_state *rs = &rig->state;
	unsigned char ttmode, ttreceiver;
	int mdbuf_len, retval;
	unsigned char mdbuf[32];

	switch (mode) {
	case RIG_MODE_USB:	ttmode = TT565_USB; break;
	case RIG_MODE_LSB:	ttmode = TT565_LSB; break;
	case RIG_MODE_CW:	ttmode = TT565_CW; break;
	case RIG_MODE_CWR:	ttmode = TT565_CWR; break;
	case RIG_MODE_AM:	ttmode = TT565_AM; break;
	case RIG_MODE_FM:	ttmode = TT565_FM; break;
	case RIG_MODE_RTTY:	ttmode = TT565_RTTY; break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %d\n",
					__FUNCTION__, mode);
		return -RIG_EINVAL;
	}

	if (width == RIG_PASSBAND_NORMAL)
		width = rig_passband_normal(rig, mode);

	ttreceiver = which_receiver(rig, vfo);

	mdbuf_len = sprintf(mdbuf, "*R%cM%c" EOM "*R%cF%d" EOM,
						ttreceiver,
						ttmode,
						ttreceiver,
						(int)width
						);

	retval = write_block(&rs->rigport, mdbuf, mdbuf_len);

	return retval;
}



/*
 * tt565_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tt565_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];
	char ttmode, ttreceiver;

	ttreceiver = which_receiver(rig, vfo);

	/* Query mode */
	cmd_len = sprintf(cmdbuf, "?R%cM" EOM, ttreceiver);
	retval = tentec_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[1] != 'R' || respbuf[3] != 'M' || resp_len <= 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	ttmode = respbuf[4];

	switch (ttmode) {
		case TT565_USB:	*mode = RIG_MODE_USB; break;
		case TT565_LSB:	*mode = RIG_MODE_LSB; break;
		case TT565_CW:	*mode = RIG_MODE_CW;  break;
		case TT565_CWR:	*mode = RIG_MODE_CWR;  break;
		case TT565_AM:	*mode = RIG_MODE_AM;  break;
		case TT565_FM:	*mode = RIG_MODE_FM;  break;
		case TT565_RTTY:	*mode = RIG_MODE_RTTY;  break;
		default:
			rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
					__FUNCTION__, ttmode);
			return -RIG_EPROTO;
	}

	/* Query passband width (filter) */
	cmd_len = sprintf(cmdbuf, "?R%cF" EOM, ttreceiver);
	retval = tentec_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[1] != 'R' || respbuf[3] != 'F' || resp_len <= 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	*width = atoi(respbuf+4);

	return RIG_OK;


}

/*
 * tt565_set_ts
 * Assumes rig!=NULL
 */
int tt565_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
	int cmd_len, retval;
	unsigned char cmdbuf[16];

	cmd_len = sprintf(cmdbuf, "*R%cI%d" EOM,
				which_receiver(rig, vfo),
				(int)ts);

	retval = tentec_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

int tt565_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];

	cmd_len = sprintf(cmdbuf, "?R%cI" EOM,
				which_receiver(rig, vfo));

	retval = tentec_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[1] != 'R' || respbuf[3] != 'I' || resp_len <= 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	*ts = atoi(respbuf+4);

	return RIG_OK;
}

/*
 * tt565_set_rit
 * Assumes rig!=NULL
 */
int tt565_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
	int cmd_len, retval;
	unsigned char cmdbuf[16];

	cmd_len = sprintf(cmdbuf, "*R%cR%d" EOM,
				which_receiver(rig, vfo),
				(int)rit);

	retval = tentec_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

int tt565_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];

	cmd_len = sprintf(cmdbuf, "?R%cR" EOM,
				which_receiver(rig, vfo));

	retval = tentec_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[1] != 'R' || respbuf[3] != 'R' || resp_len <= 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	*rit = atoi(respbuf+4);

	return RIG_OK;
}

/*
 * tt565_set_xit
 * Assumes rig!=NULL
 *
 */
int tt565_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
	int cmd_len, retval;
	unsigned char cmdbuf[16];

	/* Sub receiver does not contain an XIT setting */

	cmd_len = sprintf(cmdbuf, "*R%cX%d" EOM,
				'M',
				(int)xit);

	retval = tentec_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

int tt565_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];

	cmd_len = sprintf(cmdbuf, "?R%cX" EOM,
				'M');

	retval = tentec_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[1] != 'R' || respbuf[3] != 'X' || resp_len <= 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	*xit = atoi(respbuf+4);

	return RIG_OK;
}



/*
 * tt565_set_ptt
 * Assumes rig!=NULL
 */
int tt565_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	struct rig_state *rs = &rig->state;

	return write_block(&rs->rigport, 
			ptt==RIG_PTT_ON ? "*TK" EOM:"*TU" EOM, 4);
}

int tt565_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
	int resp_len, retval;
	unsigned char respbuf[32];

	retval = tentec_transaction (rig, "?S" EOM, 3, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[1] != 'S' || resp_len < 5) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	*ptt = respbuf[2]=='T' ? RIG_PTT_ON : RIG_PTT_OFF ;

	return RIG_OK;
}

/*
 * Software restart
 */
int tt565_reset(RIG *rig, reset_t reset)
{
	int retval, reset_len;
	char reset_buf[32];

	retval = tentec_transaction (rig, "X" EOM, 2, reset_buf, &reset_len);
	if (retval != RIG_OK)
		return retval;

	if (!strstr(reset_buf, "ORION START")) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, reset_buf);
		return -RIG_EPROTO;
	}

	return RIG_OK;
}


/*
 * tt565_get_info
 * Assumes rig!=NULL
 *
 * FIXME: what is the Orion command to get Firmware version?
 */
const char *tt565_get_info(RIG *rig)
{
	static char buf[100];	/* FIXME: reentrancy */
	int firmware_len, retval;

	/*
	 * protocol version
	 */
	firmware_len = 7;
	retval = tentec_transaction (rig, "?V" EOM, 3, buf, &firmware_len);

	/* "VER 1010-516" */
	if (retval != RIG_OK || firmware_len != 12) {
			rig_debug(RIG_DEBUG_ERR,"%s: ack NG, len=%d\n",
					__FUNCTION__, firmware_len);
			return NULL;
	}
	buf[firmware_len] = '\0';

	return buf;
}




/*
 * tt565_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sens though)
 */
int tt565_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	int retval, cmd_len=0;
	unsigned char cmdbuf[16];

	switch (level) {
	case RIG_LEVEL_RFPOWER:
		cmd_len = sprintf(cmdbuf, "*TP%d" EOM, 
				(int)(val.f*100));
		break;

	case RIG_LEVEL_AGC:
#if 0
	case RIG_LEVEL_AGC:
		/* default to MEDIUM */
		cmd_len = sprintf(cmdbuf, "G%c" EOM,
				val.i==RIG_AGC_SLOW ? '1' : (
				val.i==RIG_AGC_FAST ? '3' : '2' ) );
		retval = write_block(&rs->rigport, cmdbuf, cmd_len);
		if (retval == RIG_OK)
			priv->agc = val.i;
		return retval;

		cmd_len = sprintf(cmdbuf, "?R%cA" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'A' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		switch(lvlbuf[4]) {
		case 'O': val->i=RIG_AGC_OFF; break;
		case 'F': val->i=RIG_AGC_FAST; break;
		case 'M': val->i=RIG_AGC_MEDIUM; break;
		case 'S': val->i=RIG_AGC_SLOW; break;
		case 'P': val->i=RIG_AGC_USER; break;
		default:
			return -RIG_EPROTO;
		}
#endif
		break;

	case RIG_LEVEL_AF:
		cmd_len = sprintf(cmdbuf, "*U%c%d" EOM,
				which_receiver(rig, vfo),
				(int)(val.f*255));
		break;

	case RIG_LEVEL_IF:
		cmd_len = sprintf(cmdbuf, "*R%cP%d" EOM,
				which_receiver(rig, vfo),
				val.i);
		break;

	case RIG_LEVEL_RF:
		cmd_len = sprintf(cmdbuf, "*R%cG%d" EOM,
				which_receiver(rig, vfo),
				(int)(val.f*100));
		break;

	case RIG_LEVEL_ATT:
#if 0
		cmd_len = sprintf(cmdbuf, "?R%cT" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'T' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		if (lvlbuf[4] == '0')
			val.i = 0;
		else
			val.i = rig->caps->attenuator[lvlbuf[4]-'1'];
#endif
		break;

	case RIG_LEVEL_PREAMP:
		/* Sub receiver does not contain a Preamp */
		if (which_receiver(rig, vfo) == 'S') {
			return -RIG_EINVAL;
		}

		cmd_len = sprintf(cmdbuf, "*RME%d" EOM,
				val.i==0 ? 0 : 1);
		break;

	case RIG_LEVEL_SQL:
		cmd_len = sprintf(cmdbuf, "*R%cS%d" EOM,
				which_receiver(rig, vfo),
				(int)((val.f*127)-127));
		break;

	case RIG_LEVEL_MICGAIN:
		cmd_len = sprintf(cmdbuf, "*TM%d" EOM, 
				(int)(val.f*100));
		break;

	case RIG_LEVEL_COMP:
		cmd_len = sprintf(cmdbuf, "*TS%d" EOM, 
				(int)(val.f*9));
		break;

	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported level %d\n",
				__FUNCTION__, level);
		return -RIG_EINVAL;
	}

	retval = tentec_transaction (rig, cmdbuf, cmd_len, NULL,NULL);

	return retval;
}


/*
 * tt565_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt565_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	int retval, cmd_len, lvl_len;
	unsigned char cmdbuf[16],lvlbuf[32];


	/* Optimize:
	 *   sort the switch cases with the most frequent first
	 */
	switch (level) {
	case RIG_LEVEL_SWR:
		retval = tentec_transaction (rig, "?S" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'S' || lvl_len < 5 || lvlbuf[2]!='T') {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = atof(strchr(lvlbuf+5,'S'));
		break;

	case RIG_LEVEL_RAWSTR:
		retval = tentec_transaction (rig, "?S" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'S' || lvlbuf[2] != 'R' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->i = atoi(strchr(lvlbuf+3,
				vfo == RIG_VFO_SUB ? 'S' : 'M'));
		break;

	case RIG_LEVEL_RFPOWER:
		retval = tentec_transaction (rig, "?TP" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'T' || lvlbuf[2] != 'P' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = atof(lvlbuf+3)/100.0;
		break;

	case RIG_LEVEL_AGC:
		cmd_len = sprintf(cmdbuf, "?R%cA" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'A' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		switch(lvlbuf[4]) {
		case 'O': val->i=RIG_AGC_OFF; break;
		case 'F': val->i=RIG_AGC_FAST; break;
		case 'M': val->i=RIG_AGC_MEDIUM; break;
		case 'S': val->i=RIG_AGC_SLOW; break;
		case 'P': val->i=RIG_AGC_USER; break;
		default:
			return -RIG_EPROTO;
		}
		break;

	case RIG_LEVEL_AF:
		cmd_len = sprintf(cmdbuf, "?U%c" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'U' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = atof(lvlbuf+3)/255.0;
		break;

	case RIG_LEVEL_IF:
		cmd_len = sprintf(cmdbuf, "?R%cP" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'P' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->i = atoi(lvlbuf+4);
		break;

	case RIG_LEVEL_RF:
		cmd_len = sprintf(cmdbuf, "?R%cG" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'G' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = atof(lvlbuf+4)/100.0;
		break;

	case RIG_LEVEL_ATT:
		cmd_len = sprintf(cmdbuf, "?R%cT" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'T' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		if (lvlbuf[4] == '0')
			val->i = 0;
		else
			val->i = rig->caps->attenuator[lvlbuf[4]-'1'];
		break;

	case RIG_LEVEL_PREAMP:
		/* Sub receiver does not contain a Preamp */
		if (which_receiver(rig, vfo) == 'S') {
			val->i=0;
			break;
		}
		retval = tentec_transaction (rig, "?RME" EOM, 5, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'E' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->i = lvlbuf[4] == '0' ? 0 : rig->caps->preamp[0];
		break;

	case RIG_LEVEL_SQL:
		cmd_len = sprintf(cmdbuf, "?R%cS" EOM,
				which_receiver(rig, vfo));

		retval = tentec_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'S' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = (atof(lvlbuf+4)+127.0)/127.0;
		break;

	case RIG_LEVEL_MICGAIN:
		retval = tentec_transaction (rig, "?TM" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'T' || lvlbuf[2] != 'M' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = atof(lvlbuf+3)/100.0;
		break;

	case RIG_LEVEL_COMP:
		retval = tentec_transaction (rig, "?TS" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'T' || lvlbuf[2] != 'S' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = atof(lvlbuf+3)/9.0;
		break;


	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported level %d\n", 
				__FUNCTION__, level);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

int tt565_set_mem(RIG * rig, vfo_t vfo, int ch)
{
	struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

	priv->ch = ch;

	return RIG_OK;
}

int tt565_get_mem(RIG * rig, vfo_t vfo, int *ch)
{
	struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

	*ch = priv->ch;

	return RIG_OK;
}

int tt565_vfo_op(RIG * rig, vfo_t vfo, vfo_op_t op)
{
	struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;
	char cmdbuf[16];
	int retval;
	int cmd_len;

	switch (op) {
	case RIG_OP_TO_VFO:
	case RIG_OP_FROM_VFO:
		cmd_len = sprintf (cmdbuf, "*K%c%c%d" EOM, 
			op == RIG_OP_TO_VFO ? 'R' : 'W',
			which_vfo(rig, vfo),
			priv->ch);
		break;

	case RIG_OP_TUNE:
		strcpy(cmdbuf, "*TTT" EOM);
		cmd_len = 5;
		break;

	case RIG_OP_UP:
	case RIG_OP_DOWN:
		cmd_len = sprintf (cmdbuf, "*%cS%c1" EOM, 
			which_vfo(rig, vfo),
			op == RIG_OP_UP ? '+' : '-');
		break;

	default:
		rig_debug(RIG_DEBUG_ERR,"%s: Unsupported op %d\n", 
				__FUNCTION__, op);
		return -RIG_EINVAL;
	}

	retval = tentec_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

