/*
 *  Hamlib TenTenc backend - TT-565 description
 *  Copyright (c) 2004-2005 by Stephane Fillod & Martin Ewing
 *
 *	$Id: orion.c,v 1.15 2005-04-15 15:30:09 aa6e Exp $
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

/* Edits by Martin Ewing AA6E, 23 Mar 2005 --> ?? 
 * Added valid length settings before tentec_transaction calls.
 * Added vfo_curr initialization to VFO A
 * Fixed up VSWR & S-meter, set ATT, set AGC, add rough STR_CAL func.
 * Use local tt565_transaction due to quirky serial interface
 * Variable-length transaction read ok.
 * Calibrated S-meter response with signal generator.
 * Read re-tries implemented.
 * Added RIG_LEVEL_CWPITCH, RIG_LEVEL_KEYSPD, send_morse()
 * Added RIG_FUNC_TUNER, RIG_FUNC_LOCK and RIG_FUNC_VOX, fixed MEM_CAP.
 * Added VFO_OPS
 * Support LEVEL_VOX, VOXGAIN, ANTIVOX
 * Support LEVEL_NR as Orion NB setting (firmware bug), FUNC_NB -> NB=0,4
 * Add get_, set_ant (ignores rx only ant)
 */

/* Known issues & to-do list:
 * Memory channels - emulate a more complete memory system?
 * Send_Morse() - needs to buffer more than 20 chars?
 * Figure out "granularities".
 * XCHG or other "fancy" VFO & MEM operations?
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <sys/time.h>

#include <hamlib/rig.h>
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "idx_builtin.h"
#include "tentec.h"

#define TT565_BUFSIZE 16

/*
 * Orion's own  memory channel holds a freq, mode, and bandwidth.
 * May be captured from VFO A or B and applied to VFO A or B.
 * It cannot directly be read or written from the computer! 
 */
 
#define TT565_MEM_CAP {        \
	.freq = 1,      \
	.mode = 1,      \
	.width = 1,     \
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
static int tt565_send_morse(RIG *rig, vfo_t vfo, const char *msg);
static int tt565_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int tt565_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int tt565_set_ant(RIG * rig, vfo_t vfo, ant_t ant);
static int tt565_get_ant(RIG *rig, vfo_t vfo, ant_t *ant);

struct tt565_priv_data {
	int ch;		/* mem */
	vfo_t vfo_curr;
};

#define TT565_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
			RIG_MODE_RTTY|RIG_MODE_AM)
#define TT565_RXMODES (TT565_MODES)

#define TT565_FUNCS (RIG_FUNC_LOCK|RIG_FUNC_TUNER|RIG_FUNC_VOX|RIG_FUNC_NB)

#define TT565_LEVELS (RIG_LEVEL_RAWSTR| \
				RIG_LEVEL_CWPITCH| \
				RIG_LEVEL_SQL|RIG_LEVEL_IF| \
				RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD| \
				RIG_LEVEL_RF|RIG_LEVEL_NR| \
				RIG_LEVEL_MICGAIN| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOX|RIG_LEVEL_ANTIVOX| \
				RIG_LEVEL_COMP|RIG_LEVEL_PREAMP| \
				RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT565_ANTS (RIG_ANT_1|RIG_ANT_2) 
#define TT565_RXANTS (TT565_ANTS|RIG_ANT_3) 

#define TT565_PARMS (RIG_PARM_NONE)

#define TT565_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT565_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|\
		RIG_OP_TO_VFO|RIG_OP_FROM_VFO| \
		RIG_OP_TUNE)

/* Checked on one physical unit 3/29/05 - aa6e */
#define TT565_STR_CAL { 15,  { \
		{  10, -45 }, /* S 1.5 min meter indication */ \
                {  13, -42 }, \
                {  18, -36 }, \
                {  22, -30 }, \
                {  27, -24 }, \
                {  30, -18 }, \
                {  34, -12 }, \
                {  38,  -6 }, \
                {  43,   0 }, /* S9 */ \
                {  48,  10 }, \
                {  55,  20 }, \
                {  62,  30 }, \
                {  70,  40 }, \
                {  78,  48 }, /* severe dsp quantization error */ \
                { 101,  65 }, /* at high end of scale */ \
        } }

#undef TT565_TIME		/* Define to enable time checks */

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
.version =  "0.3",
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
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
.post_write_delay =  10,	/* Needed for CW send + ?? */
.timeout =  400,
.retry =  3,

.has_get_func =  TT565_FUNCS,
.has_set_func =  TT565_FUNCS,
.has_get_level =  TT565_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT565_LEVELS),
.has_get_parm =  TT565_PARMS,
.has_set_parm =  TT565_PARMS,

.level_gran = {}, 
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 20, RIG_DBLST_END },
.attenuator =   { 6, 12, 18, RIG_DBLST_END },
.max_rit =  kHz(8),
.max_xit =  kHz(8),
.max_ifshift =  kHz(8),
.vfo_ops = TT565_VFO_OPS,
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
.send_morse = tt565_send_morse,
.get_func = tt565_get_func,
.set_func = tt565_set_func,
.get_ant = tt565_get_ant,
.set_ant = tt565_set_ant,

.str_cal = TT565_STR_CAL,
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

#ifdef TT565_TIME
double tt565_timenow()	/* returns current time in secs+microsecs */
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + ((double)tv.tv_usec)/1.0e+6;
}
#endif

/*
 * tt565_transaction, adapted from tentec_transaction (tentec.c)
 * read variable number of bytes, up to buffer size, if data & data_len != NULL.
 * We assume that rig!=NULL, rig->state!= NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 */
int tt565_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
        int retval, data_len_init, itry;
        struct rig_state *rs;
#ifdef TT565_TIME
 	double ft1, ft2;	
#endif
	/* Capture buffer length for possible read re-try. */
	data_len_init = (data && data_len) ? *data_len : 0;
	/* Allow transaction re-tries according to capabilities. */
	for (itry=1; itry < rig->caps->retry; itry++) {	
          rs = &rig->state;

          serial_flush(&rs->rigport);

          retval = write_block(&rs->rigport, cmd, cmd_len);
          if (retval != RIG_OK)
                        return retval;

          /* no data expected, TODO: flush input? */
          if (!data || !data_len)
                        return 0;	/* normal exit if no read */
#ifdef TT565_TIME
	  ft1 = tt565_timenow();
#endif
	  *data_len = data_len_init;	/* restore orig. buffer length */
          *data_len = read_string(&rs->rigport, data, *data_len, 
		EOM, strlen(EOM));
	  if (*data_len > 0) return RIG_OK; /* normal exit if reading */
#ifdef TT565_TIME
	  ft2 = tt565_timenow();
          if (*data_len == -RIG_ETIMEOUT)
	    rig_debug(RIG_DEBUG_ERR,"Timeout %d: Elapsed = %f secs.\n", itry, ft2-ft1);
	  else
	    rig_debug(RIG_DEBUG_ERR,"Other Error #%d, itry=%d: Elapsed = %f secs.\n", 
		*data_len, itry, ft2-ft1);
#endif
	}
        return -RIG_ETIMEOUT;
}

/*************************************************************************************
 * Specs from http://www.rfsquared.com, Rev 1, firmware 1.340
 * supplemented and corrected with real hardware, firmware 1.372
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
	priv->vfo_curr = RIG_VFO_A;		
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
	unsigned char cmdbuf[TT565_BUFSIZE];

	cmd_len = sprintf (cmdbuf, "*%cF%"PRIll EOM, 
			which_vfo(rig, vfo),
			(long long)freq);

	retval = tt565_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

/*
 * tt565_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt565_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];

	cmd_len = sprintf(cmdbuf, "?%cF" EOM,
				which_vfo(rig, vfo));

	resp_len = sizeof(respbuf);	
	retval = tt565_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

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
	unsigned char vfobuf[TT565_BUFSIZE];

	if (vfo == RIG_VFO_CURR)
		return RIG_OK;

	if (vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB) {
		/* Select Sub or Main RX */
		vfo_len = sprintf (vfobuf, "*K%c" EOM, 
				vfo == RIG_VFO_SUB ? 'S' : 'M');

		return tt565_transaction (rig, vfobuf, vfo_len, NULL, NULL);
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
	unsigned char cmdbuf[TT565_BUFSIZE];

	cmd_len = sprintf (cmdbuf, "*KV%c%c%c" EOM, 
			which_vfo(rig, vfo),
			'N',			/* FIXME */
			which_vfo(rig, tx_vfo));

	retval = tt565_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

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
	unsigned char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];
	char ttreceiver;

	cmd_len = sprintf(cmdbuf, "?KV" EOM);
	resp_len = sizeof(respbuf);	
	retval = tt565_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

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
	unsigned char mdbuf[TT565_BUFSIZE];

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
	unsigned char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];
	char ttmode, ttreceiver;

	ttreceiver = which_receiver(rig, vfo);

	/* Query mode */
	cmd_len = sprintf(cmdbuf, "?R%cM" EOM, ttreceiver);
	resp_len = sizeof(respbuf);  
	retval = tt565_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

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
	resp_len = sizeof(respbuf); 
	retval = tt565_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

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
	unsigned char cmdbuf[TT565_BUFSIZE];

	cmd_len = sprintf(cmdbuf, "*R%cI%d" EOM,
				which_receiver(rig, vfo),
				(int)ts);

	retval = tt565_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

int tt565_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];

	cmd_len = sprintf(cmdbuf, "?R%cI" EOM,
				which_receiver(rig, vfo));

	resp_len = sizeof(respbuf);	
	retval = tt565_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

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
	unsigned char cmdbuf[TT565_BUFSIZE];

	cmd_len = sprintf(cmdbuf, "*R%cR%d" EOM,
				which_receiver(rig, vfo),
				(int)rit);

	retval = tt565_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

int tt565_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];

	cmd_len = sprintf(cmdbuf, "?R%cR" EOM,
				which_receiver(rig, vfo));

	resp_len = sizeof(respbuf);	
	retval = tt565_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

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
	unsigned char cmdbuf[TT565_BUFSIZE];

	/* Sub receiver does not contain an XIT setting */

	cmd_len = sprintf(cmdbuf, "*R%cX%d" EOM,
				'M',
				(int)xit);

	retval = tt565_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

	return retval;
}

int tt565_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];

	cmd_len = sprintf(cmdbuf, "?R%cX" EOM,
				'M');

	resp_len = sizeof(respbuf);	
	retval = tt565_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

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
	unsigned char respbuf[TT565_BUFSIZE];

	resp_len = sizeof(respbuf);	
	retval = tt565_transaction (rig, "?S" EOM, 3, respbuf, &resp_len);

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
	char reset_buf[TT565_BUFSIZE];

	reset_len = sizeof(reset_buf);	
	retval = tt565_transaction (rig, "X" EOM, 2, reset_buf, &reset_len);
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
 */
const char *tt565_get_info(RIG *rig)
{
	static char buf[100];	/* FIXME: reentrancy */
	int firmware_len, retval;

	/*
	 * protocol version
	 */
	firmware_len = sizeof(buf);	
	retval = tt565_transaction (rig, "?V" EOM, 3, buf, &firmware_len);

	/* "Version 1.372" */				
	if (retval != RIG_OK || firmware_len < 8) {	
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
	int retval, cmd_len=0, ii;
	unsigned char cmdbuf[TT565_BUFSIZE], cc;

	switch (level) {
	case RIG_LEVEL_RFPOWER:
		cmd_len = sprintf(cmdbuf, "*TP%d" EOM, 
				(int)(val.f*100));
		break;

	case RIG_LEVEL_AGC:
		switch(val.i) {
		case RIG_AGC_OFF:    cc = 'O'; break;
		case RIG_AGC_FAST:   cc = 'F'; break;
		case RIG_AGC_MEDIUM: cc = 'M'; break;
		case RIG_AGC_SLOW:   cc = 'S'; break;
		case RIG_AGC_USER:   cc = 'P'; break;
		default: cc = 'M';
		}
		cmd_len = sprintf(cmdbuf, "*R%cA%c" EOM,
			which_receiver(rig, vfo),
			cc);
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
		ii = -1;	/* Request 0-5 dB -> 0, 6-11 dB -> 6, etc. */
		while ( rig->caps->attenuator[++ii] != RIG_DBLST_END ) {
			if (rig->caps->attenuator[ii] > val.i) break;
		}
		
		cmd_len = sprintf(cmdbuf, "*R%cT%d" EOM,
				which_receiver(rig, vfo),
				ii);
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

	case RIG_LEVEL_CWPITCH:
		/* "CWPITCH" is the "Tone" button on the Orion.
		   Manual menu adjustment works down to 100 Hz, but not via
		   computer.
		*/
		if (val.i > 1200) val.i = 1200;
		else if (val.i < 300) val.i = 300;	/* Range 300-1200 Hz works */
							/* Limits should be in caps? */
		cmd_len = sprintf(cmdbuf, "*CT%d" EOM,
				val.i);
		break;

	case RIG_LEVEL_KEYSPD:
		/* Keyer speed setting does not imply Keyer = "on".  That is a
		   command which should be a hamlib function, but is not.
		   Keyer speed determines the rate of computer sent CW also.
		*/
		if (val.i > 60) val.i = 60;
		else if (val.i < 10) val.i = 10;	/* Range 10-60 wpm */	
		cmd_len = sprintf(cmdbuf, "*CS%d" EOM,
				val.i);
		break;

	case RIG_LEVEL_NR:
		/* For some reason NB setting is supported in 1.372, but
		   NR, NOTCH, and AN are not. 
		   FOR NOW -- RIG_LEVEL_NR controls the Orion NB setting 
		*/
		cmd_len = sprintf(cmdbuf, "*R%cNB%d" EOM,
				which_receiver(rig, vfo),
				(int)(val.f*9));
		break;

	case RIG_LEVEL_VOX:	/* =VOXDELAY, tenths of seconds */
		cmd_len = sprintf(cmdbuf, "*TH%4.2f" EOM, 0.1*val.f);
		break;

	case RIG_LEVEL_VOXGAIN:	/* Float, 0.0 - 1.0 */
		cmd_len = sprintf(cmdbuf, "*TG%d" EOM, (int)(100.0*val.f));
		break;

	case RIG_LEVEL_ANTIVOX:	/* Float, 0.0 - 1.0 */
		cmd_len = sprintf(cmdbuf, "*TA%d" EOM, (int)(100.0*val.f));
		break;

	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported level %d\n",
				__FUNCTION__, level);
		return -RIG_EINVAL;
	}

	retval = tt565_transaction (rig, cmdbuf, cmd_len, NULL,NULL);

	return retval;
}


/*
 * tt565_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt565_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	int retval, cmd_len, lvl_len;
	unsigned char cmdbuf[TT565_BUFSIZE],lvlbuf[TT565_BUFSIZE];

	/* Optimize: sort the switch cases with the most frequent first */
	switch (level) {
	case RIG_LEVEL_SWR:
		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, "?S" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		/* in Xmit, response is @STFuuuRvvvSwww (or ...Swwww)
			uuu = 000-100 (apx) fwd watts
                        vvv = 000-100       rev watts
                        www = 256-999  256 * VSWR
		   in Rcv,  response is @SRMuuuSvvv
			uuu = 000-100 (apx) Main S meter
                        vvv = 000-100 (apx) Sub  S meter
		*/

		if (lvlbuf[1] != 'S' || lvl_len < 5 ) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		if (lvlbuf[2]=='T') {
			val->f = atof(strchr(lvlbuf+5,'S')+1)/256.0;
			if (val->f < 1.0) val->f = 9.99;	/* high VSWR */
		}
			else val->f = 0.0;	/* SWR in Receive = 0.0 */
		break;

	case RIG_LEVEL_RAWSTR:
  		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, "?S" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'S' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		if (lvlbuf[2] == 'R') {
			val->i = atoi(strchr(lvlbuf+3,
				vfo == RIG_VFO_SUB ? 'S' : 'M')+1);  /* check main/sub logic */
		}
			else val->i = 0;	/* S-meter in xmit = 0 */
		break;

	case RIG_LEVEL_RFPOWER:
		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, "?TP" EOM, 4, lvlbuf, &lvl_len);
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

		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
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

		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
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
		cmd_len = sprintf(cmdbuf, "?R%cP" EOM,	/* passband tuning */
				which_receiver(rig, vfo));

		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
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

		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
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

		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
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
		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, "?RME" EOM, 5, lvlbuf, &lvl_len);
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

		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
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
		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, "?TM" EOM, 4, lvlbuf, &lvl_len);
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
		lvl_len = sizeof(lvlbuf);	
		retval = tt565_transaction (rig, "?TS" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'T' || lvlbuf[2] != 'S' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = atof(lvlbuf+3)/9.0;
		break;

	case RIG_LEVEL_CWPITCH:
		lvl_len = sizeof(lvlbuf);
		retval = tt565_transaction (rig, "?CT" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'C' || lvlbuf[2] != 'T' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		val->i = atoi(lvlbuf+3);
		break;

	case RIG_LEVEL_KEYSPD:
		lvl_len = sizeof(lvlbuf);
		retval = tt565_transaction (rig, "?CS" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'C' || lvlbuf[2] != 'S' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		val->i = atoi(lvlbuf+3);
		break;

	case RIG_LEVEL_NR:
		/* RIG_LEVEL_NR controls Orion NB setting - TEMP */
		cmd_len = sprintf(cmdbuf, "?R%cNB" EOM,
                                which_receiver(rig, vfo));
	
		lvl_len = sizeof(lvlbuf);
		retval = tt565_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'N' || lvlbuf[4] != 'B' ||
			lvl_len < 6 ) {
				rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
				return -RIG_EPROTO;
			}
		val->f = atof(lvlbuf+5)/9.0;	/* Note 0-9 -> 0.0 - 1.0 */
		break;

	case RIG_LEVEL_VOX:	/* =VOXDELAY, tenths of secs. */
		lvl_len = sizeof(lvlbuf);
		retval = tt565_transaction (rig, "?TH" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[1] != 'T' || lvlbuf[2] != 'H' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
				__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		val->f = 10.0*atof(lvlbuf+3);
		break;

	case RIG_LEVEL_VOXGAIN:	/* Float, 0.0 - 1.0 */
		lvl_len = sizeof(lvlbuf);
		retval = tt565_transaction (rig, "?TG" EOM, 4, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[1] != 'T' || lvlbuf[2] != 'G' || lvl_len < 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
				__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		val->f = 0.01 * atof(lvlbuf+3);
	break;

	case RIG_LEVEL_ANTIVOX:	/* Float, 0.0 - 1.0 */
                lvl_len = sizeof(lvlbuf);
                retval = tt565_transaction (rig, "?TA" EOM, 4, lvlbuf, &lvl_len);
                if (retval != RIG_OK)
                        return retval;
                if (lvlbuf[1] != 'T' || lvlbuf[2] != 'A' || lvl_len < 4) {
                        rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
                                __FUNCTION__, lvlbuf);
                        return -RIG_EPROTO;
                }
                val->f = 0.01 * atof(lvlbuf+3);
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

	priv->ch = ch;	/* See RIG_OP_TO/FROM_VFO */
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
	char cmdbuf[TT565_BUFSIZE];
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

	retval = tt565_transaction (rig, cmdbuf, cmd_len, NULL, NULL);
	return retval;
}

int tt565_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
	int msg_len, retval, ic, cmdl;
	char morsecmd[8];
	static int keyer_set = 0;	/*Shouldn't be here!*/

/* Orion keyer must be on for morse, but we do not have a "keyer on" function in 
 * hamlib (yet).  So force keyer on here. 
 */
	if (!keyer_set) {
	    retval = tt565_transaction(rig, "*CK1" EOM, 5, NULL, NULL);
	    if (retval != RIG_OK)
		return retval;
	    keyer_set = 1;
	    usleep(100000);	/* 100 msec - guess */
	}
	msg_len = strlen(msg);
	if (msg_len > 20) msg_len = 20;	/* sanity limit 20 chars */

/* Orion can queue up to about 20 characters.  
 * We could batch a longer message into 20 char chunks, but there is no
 * simple way to tell if message has completed.  We could calculate a
 * duration based on keyer speed and the text that was sent, but
 * what we really need is a handshake for "message complete".
 * Without it, you can't easily use the Orion as a code practice machine.
 * For now, we let the user do the batching.
 * Note that rig panel is locked up for duration of message. 
 */
	for (ic = 0; ic < msg_len; ic++) {
		cmdl = sprintf(morsecmd,"/%c" EOM, msg[ic]);
		retval = tt565_transaction(rig,morsecmd,cmdl,NULL,NULL);
		if (retval != RIG_OK)
			return retval;
	}
	return RIG_OK;
}

int tt565_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
	unsigned char fcmdbuf[TT565_BUFSIZE];
	int retval, fcmdlen;

	if (vfo != RIG_VFO_CURR)
		return -RIG_EINVAL;

	switch (func) {
	case RIG_FUNC_TUNER:
		fcmdlen = sprintf(fcmdbuf,"*TT%c" EOM, !status ? 0:1);
		break;

	case RIG_FUNC_VOX:
		fcmdlen = sprintf(fcmdbuf,"*TV%c" EOM, !status ? 0:1);
		break;

	case RIG_FUNC_LOCK:
		fcmdlen = sprintf(fcmdbuf,"*%c%c" EOM,
			which_vfo(rig, vfo),
			!status ? 'U' : 'L' );
		break;

	case RIG_FUNC_NB:
		/* NB "on" sets Orion NB=4; "off" -> NB=0.  See also
		RIG_LEVEL_NR which maps to NB setting due to firmware
		limitation.
		*/
		fcmdlen = sprintf(fcmdbuf,"*R%cNB%c" EOM,
			which_receiver(rig, vfo),
			!status ? '0' : '4' );
		break;

	default:
                rig_debug(RIG_DEBUG_ERR,"Unsupported set_func %#x", func);
                return -RIG_EINVAL;
	}	
        retval = tt565_transaction(rig, fcmdbuf, fcmdlen, NULL, NULL);

        if (retval != RIG_OK)
                return retval;
	return RIG_OK;
}

int tt565_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
	unsigned char fcmdbuf[TT565_BUFSIZE], frespbuf[TT565_BUFSIZE];
	int retval, fcmdlen, fresplen;

	if (vfo != RIG_VFO_CURR)
		return -RIG_EINVAL;

	switch (func) {
	case RIG_FUNC_TUNER:
		fcmdlen = sprintf(fcmdbuf, "?TT" EOM);
		break;

	case RIG_FUNC_VOX:
		fcmdlen = sprintf(fcmdbuf, "?TV" EOM);
		break;

	case RIG_FUNC_LOCK:
		fcmdlen = sprintf(fcmdbuf, "?%cU" EOM, 
			which_vfo(rig, vfo) );
		/* needs special treatment */
		fresplen = sizeof(frespbuf);
		retval = tt565_transaction(rig, fcmdbuf, fcmdlen, 
			frespbuf, &fresplen);
		if (retval != RIG_OK)
			return retval;
		/* response is @AL @AU or @BL @BU */
		*status = frespbuf[ 2 ] == 'L' ? 1:0;
		return RIG_OK;

	case RIG_FUNC_NB:
		/* Note NB should be a LEVEL for Orion. It is also
		   available through LEVEL_NR
		*/
		fcmdlen = sprintf(fcmdbuf, "?R%cNB" EOM,
			which_receiver(rig, vfo) );
		/* needs special treatment */
		fresplen = sizeof(frespbuf);
		retval = tt565_transaction(rig, fcmdbuf, fcmdlen, 
			frespbuf, &fresplen);
		if (retval != RIG_OK)
			return retval;
		/* response is @RxNBn, n=0--9. Return 0 iff receive NB=0 */
		*status = frespbuf[ 5 ] == '0' ? 0:1;
		return RIG_OK;

	default:
		rig_debug(RIG_DEBUG_ERR,"Unsupported get_func %#x", func);
		return -RIG_EINVAL;
	}
	fresplen = sizeof(frespbuf);
	retval = tt565_transaction(rig, fcmdbuf, fcmdlen, frespbuf, &fresplen);
	if (retval != RIG_OK)
		return retval;
	*status = frespbuf[ 3 ] == '1' ? 1:0;
	return RIG_OK;
}
/* Antenna selection for Orion
 * We support Ant_1 and Ant_2 for M and S receivers.
 * Note that Rx-only antenna (Ant_3?) is not supported at this time.
 * Orion command assigns MSBN to each ant, but hamlib wants to assign ant to rx/tx!
 * The efficient way would be to keep current config in rig priv area, but we will
 * ask the rig what its state is each time...
 */
int tt565_set_ant(RIG * rig, vfo_t vfo, ant_t ant)
{
                unsigned char respbuf[TT565_BUFSIZE];
                int resp_len, retval;
		ant_t main_ant, sub_ant;

		/* First, find out what antenna config is now. */
		resp_len = sizeof(respbuf);
		retval = tt565_transaction (rig, "?KA" EOM, 4, respbuf, &resp_len);
                if (retval != RIG_OK)
                                return retval;
		if (resp_len != 7 || respbuf[1] != 'K' || respbuf[2] != 'A') {
			rig_debug(RIG_DEBUG_ERR,"%s; tt565_set_ant: ?KA NG %s\n", 
				__FUNCTION__, respbuf);
			return -RIG_EPROTO;
		}

		/* respbuf="@KAxxx"
		 * x='M'|'S'|'B'|'N'=main/sub/both/none for ants 1,2,3. 
		 * but hardware will not permit all combinations!
		 * respbuf [3,4] can be MS, SM, BN, NB
		 * decode to rx-centric view 
		 */
		if (respbuf[3] == 'M' || respbuf[3] == 'B') main_ant = RIG_ANT_1;
			else main_ant = RIG_ANT_2;
		if (respbuf[3] == 'S' || respbuf[3] == 'B') sub_ant = RIG_ANT_1;
			else sub_ant = RIG_ANT_2;
		switch (which_receiver(rig,vfo)) {
			case 'M':
				main_ant = ant;
				break;
			case 'S':
				sub_ant = ant;
				break;
			default: {
				/* no change? */
			}
		}
		/* re-encode ant. settings into command */
		if (main_ant == RIG_ANT_1) {
			if (sub_ant == RIG_ANT_1) {
					respbuf[3] = 'B';
					respbuf[4] = 'N';
			}
				else {
					respbuf[3] = 'M';
					respbuf[4] = 'S';
				}
		}
		else if (sub_ant == RIG_ANT_2) {
					respbuf[3] = 'N';
					respbuf[4] = 'B';
			}
				else {
					respbuf[3] = 'S';
					respbuf[4] = 'M';
				}
		respbuf[0] = '*';	/* respbuf becomes a store command */
		respbuf[5] = 'N';	/* Force no rx on Ant 3 */
		respbuf[6] = EOM[0];
		respbuf[7] = 0;
                retval = tt565_transaction (rig, respbuf, 7, NULL, NULL); 
                if (retval != RIG_OK)
                                return retval;
                return RIG_OK;
}

int tt565_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
                unsigned char respbuf[TT565_BUFSIZE];
                int resp_len, retval;

		resp_len = sizeof(respbuf);
                retval = tt565_transaction(rig, "?KA" EOM, 4, respbuf, &resp_len); 
                if (retval != RIG_OK)
                                return retval;
		if (respbuf[1] != 'K' || respbuf[2] != 'A' || resp_len != 7) {
			rig_debug(RIG_DEBUG_ERR,"%s; tt565_get_ant: NG %s\n", 
				__FUNCTION__, respbuf);
			return -RIG_EPROTO;
		}
		/* Look for first occurrence of M or S in ant 1, 2, 3 characters */
		if (respbuf[3] == which_receiver(rig,vfo) || respbuf[3] == 'B' ) {
			*ant = RIG_ANT_1;
			return RIG_OK;
		}
		if (respbuf[4] == which_receiver(rig,vfo) || respbuf[4] == 'B' ) {
			*ant = RIG_ANT_2;
			return RIG_OK;
		} 

                *ant = RIG_ANT_NONE;	/* ignore possible RIG_ANT_3 = rx only ant */
                return RIG_OK;
}
/* End of orion.c */
