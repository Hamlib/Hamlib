/*
 *  Hamlib AOR backend - AR3030 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ar3030.c,v 1.2 2004-07-03 15:01:55 t_mills Exp $
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
#include <string.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "idx_builtin.h"
#include "misc.h"
#include "aor.h"


static int ar3030_set_vfo(RIG *rig, vfo_t vfo);
static int ar3030_get_vfo(RIG *rig, vfo_t *vfo);
static int ar3030_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ar3030_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ar3030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ar3030_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ar3030_set_mem(RIG *rig, vfo_t vfo, int ch);
static int ar3030_get_mem(RIG *rig, vfo_t vfo, int *ch);
static int ar3030_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int ar3030_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ar3030_get_channel(RIG *rig, channel_t *chan);
static int ar3030_init(RIG *rig);
static int ar3030_cleanup(RIG *rig);
static int ar3030_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

struct ar3030_priv_data {
	int curr_ch;
	int curr_vfo;
};

/* 
 * TODO:
 *	set_channel(emulated?),rig_vfo_op
 *	rig_reset(RIG_RESET_MCALL)
 *	quit the remote control mode on close?
 *
 *	Modes: FAX
 */
#define AR3030_MODES (RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

#define AR3030_FUNC_ALL (RIG_FUNC_NONE)

#define AR3030_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR)

#define AR3030_PARM (RIG_PARM_NONE)

#define AR3030_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_MCL)

#define AR3030_VFO (RIG_VFO_A|RIG_VFO_MEM)

/*
 * FIXME:
 */
#define AR3030_STR_CAL { 2, \
	{ \
		{ 0x00, -60 }, \
		{ 0x3f, 60 }  \
	} }

#define AR3030_MEM_CAP {        \
	.freq = 1,      \
	.mode = 1,      \
	.width = 1,     \
	.levels = RIG_LEVEL_SET(AR3030_LEVEL),     \
	.flags = 1,	\
}

/*
 * Data was obtained from AR3030 pdf on http://www.aoruk.com
 *
 * ar3030 rig capabilities.
 */
const struct rig_caps ar3030_caps = {
.rig_model =  RIG_MODEL_AR3030,
.model_name = "AR3030",
.mfg_name =  "AOR",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  1,	/* ms */
.timeout =  500,
.retry =  0,
.has_get_func =  AR3030_FUNC_ALL,
.has_set_func =  AR3030_FUNC_ALL,
.has_get_level =  AR3030_LEVEL,
.has_set_level =  RIG_LEVEL_SET(AR3030_LEVEL),
.has_get_parm =  AR3030_PARM,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { 10, 20, RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,
.vfo_ops =  AR3030_VFO_OPS,
.str_cal = AR3030_STR_CAL,

.chan_list =  { 
	{   0,  99, RIG_MTYPE_MEM, AR3030_MEM_CAP },
	RIG_CHAN_END, },

.rx_range_list1 =  {
	{kHz(30),MHz(30),AR3030_MODES,-1,-1,AR3030_VFO},
	RIG_FRNG_END,
	},
.tx_range_list1 =  { RIG_FRNG_END, },

.rx_range_list2 =  {
	{kHz(30),MHz(30),AR3030_MODES,-1,-1,AR3030_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  { RIG_FRNG_END, },	/* no tx range, this is a receiver! */

.tuning_steps =  {
	 {AR3030_MODES,10},
	 {AR3030_MODES,100},
	 {AR3030_MODES,kHz(1)},
	 {AR3030_MODES,MHz(1)},
	 RIG_TS_END,
	},
        /* mode/filter list, .remember =  order matters! */
.filters =  {
		{RIG_MODE_AM, kHz(6)}, 
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_AM, kHz(2.4)}, 
		{RIG_MODE_CW, 500},
		{RIG_MODE_FM, kHz(15)},
		RIG_FLT_END,
	},

.rig_init = ar3030_init,
.rig_cleanup = ar3030_cleanup,

.set_freq =  ar3030_set_freq,
.get_freq =  ar3030_get_freq,
.set_mode =  ar3030_set_mode,
.get_mode =  ar3030_get_mode,
.set_vfo =  ar3030_set_vfo,
.get_vfo =  ar3030_get_vfo,

.set_level =  ar3030_set_level,
.get_level =  ar3030_get_level,

.set_mem =  ar3030_set_mem,
.get_mem =  ar3030_get_mem,

.get_channel = ar3030_get_channel,

.vfo_op = ar3030_vfo_op,

};

/*
 * Function definitions below
 */




/* is LF really needed? */
#define EOM "\x0a\x0d"

#define BUFSZ 64

/*
 * ar3030_transaction
 * We assume that rig!=NULL, rig->state!= NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling
 */
int ar3030_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int retval;
	struct rig_state *rs;

	rs = &rig->state;

	serial_flush(&rs->rigport);

	retval = write_block(&rs->rigport, cmd, cmd_len);
	if (retval != RIG_OK)
		return retval;

	/* will flush data on next transaction */
	if (!data || !data_len)
		return RIG_OK;

	*data_len = read_string(&rs->rigport, data, BUFSZ, EOM, strlen(EOM));

	return RIG_OK;
}


int ar3030_init(RIG *rig)
{
	struct ar3030_priv_data *priv;

	priv = malloc(sizeof(struct ar3030_priv_data));

	if (!priv)
		return -RIG_ENOMEM;

	priv->curr_ch = 99;	/* huh! FIXME: get_mem in open() ? */
	priv->curr_vfo = RIG_VFO_A;

	rig->state.priv = priv;

	return RIG_OK;
}

int ar3030_cleanup(RIG *rig)
{
	struct ar3030_priv_data *priv = rig->state.priv;

	free(priv);

	return RIG_OK;
}


int ar3030_set_vfo(RIG *rig, vfo_t vfo)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	unsigned char *cmd = "";
	int retval;

	switch(vfo) {
	case RIG_VFO_CURR:
		return RIG_OK;
	case RIG_VFO_VFO:
	case RIG_VFO_A: cmd = "D" EOM; break;
	case RIG_VFO_MEM: cmd = "M" EOM; break;
	default: return -RIG_EINVAL;
	}

	retval = ar3030_transaction (rig, cmd, strlen(cmd), NULL, NULL);
	if (retval == RIG_OK)
		priv->curr_vfo = vfo;

	return retval;
}

int ar3030_get_vfo(RIG *rig, vfo_t *vfo)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;

	*vfo = priv->curr_vfo;

	return RIG_OK;
}


/*
 * ar3030_set_freq
 * Assumes rig!=NULL
 */
int ar3030_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	unsigned char freqbuf[BUFSZ];
	int freq_len, retval;

	freq_len = sprintf(freqbuf,"%03.4f" EOM, ((double)freq)/MHz(1));

	retval = ar3030_transaction (rig, freqbuf, freq_len, NULL, NULL);
	if (retval != RIG_OK)
		return retval;

	priv->curr_vfo = RIG_VFO_A;

	return RIG_OK;
}

/*
 * ar3030_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int ar3030_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	char *rfp;
	int freq_len, retval;
	unsigned char freqbuf[BUFSZ];

	/*
	 * DRnGnBnTnFnnnnnnnnC
	 */
	retval = ar3030_transaction (rig, "D" EOM, 3, freqbuf, &freq_len);
	if (retval != RIG_OK)
		return retval;

	priv->curr_vfo = RIG_VFO_A;
	rfp = strchr(freqbuf, 'F');
	if (!rfp)
		return -RIG_EPROTO;
	sscanf(rfp+1,"%"FREQFMT, freq);
	*freq *= 10;

	return RIG_OK;
}

/*
 * ar3030_set_mode
 * Assumes rig!=NULL
 */
int ar3030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[BUFSZ];
	int mdbuf_len, aormode, retval;

	switch (mode) {
	case RIG_MODE_AM:       aormode = 'A'; break;
	case RIG_MODE_CW:       aormode = 'C'; break;
	case RIG_MODE_USB:      aormode = 'U'; break;
	case RIG_MODE_LSB:      aormode = 'L'; break;
	case RIG_MODE_FM:	      aormode = 'N'; break;
	case RIG_MODE_AMS:      aormode = 'S'; break;
	/*case RIG_MODE_FAX:	aormode = 'X'; break;*/
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
					__FUNCTION__,mode);
		return -RIG_EINVAL;
	}

	mdbuf_len = sprintf(mdbuf, "%dB%c" EOM,
				width < rig_passband_normal(rig,mode) ? 1 : 0,
	 			aormode);
	retval = ar3030_transaction (rig, mdbuf, mdbuf_len, NULL, NULL);

	return retval;
}

/*
 * ar3030_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int ar3030_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	int buf_len, retval;
	unsigned char buf[BUFSZ];

	/*
	 * DRnGnBnTnFnnnnnnnnC
	 */
	retval = ar3030_transaction (rig, "D" EOM, 3, buf, &buf_len);
	if (retval != RIG_OK)
		return retval;

	priv->curr_vfo = RIG_VFO_A;

	switch (buf[25]) {
	case 'A':	*mode = RIG_MODE_AM; break;
	case 'L':	*mode = RIG_MODE_LSB; break;
	case 'U':	*mode = RIG_MODE_USB; break;
	case 'C':	*mode = RIG_MODE_CW; break;
	case 'S':	*mode = RIG_MODE_AMS; break;
	case 'N':	*mode = RIG_MODE_FM; break;
	/*case 'X':	*mode = RIG_MODE_FAX; break;*/
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
						__FUNCTION__,buf[25]);
		return -RIG_EPROTO;
	}

	*width = buf[6] == '1' ? rig_passband_narrow(rig, *mode) : 
					rig_passband_normal(rig, *mode);

	return RIG_OK;
}


int ar3030_set_mem(RIG *rig, vfo_t vfo, int ch)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	unsigned char cmdbuf[BUFSZ];
	int cmd_len, retval=RIG_OK;

	if (priv->curr_vfo == RIG_VFO_MEM) {
		cmd_len = sprintf(cmdbuf, "%02dM" EOM, ch);
		retval = ar3030_transaction (rig, cmdbuf, cmd_len, NULL, NULL);
	}

	if (retval == RIG_OK) {
		priv->curr_ch = ch;
	}

	return retval;
}

int ar3030_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	unsigned char infobuf[BUFSZ];
	int info_len, retval;

	if (priv->curr_vfo != RIG_VFO_MEM) {
		*ch = priv->curr_ch;
	}

	retval = ar3030_transaction (rig, "M" EOM, 3, infobuf, &info_len);
	if (retval != RIG_OK)
		return retval;

	/*
 	 * MnnPnRnGnBnTnFnnnnnnnnC
 	 */
	if (infobuf[0] != 'M') {
		return -RIG_EPROTO;
	}

	/*
	 * Is it a blank mem channel ?
	 */
	if (infobuf[1] == '-' && infobuf[2] == '-') {
		*ch = -1;	/* FIXME: return error instead? */
		return RIG_OK;
	}

	*ch = priv->curr_ch = atoi(infobuf+1);

	return RIG_OK;
}


int ar3030_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	unsigned char *cmd;
	int retval;

	switch(level) {
	case RIG_LEVEL_AGC:
		/* SLOW otherwise */
		cmd = val.i == RIG_AGC_FAST ? "1G" EOM : "0G" EOM;
		break;
	case RIG_LEVEL_ATT:
		cmd = val.i == 0 ? "0R" EOM : 
			(val.i == 1 ? "1R" EOM : "2R" EOM);
		break;
	default:
		return -RIG_EINVAL;
	}

	retval = ar3030_transaction (rig, cmd, strlen(cmd), NULL, NULL);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}


int ar3030_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	int info_len, retval;
	unsigned char infobuf[BUFSZ], *p;

	switch(level) {
	case RIG_LEVEL_ATT:
		/*
	 	 * DRnGnBnTnFnnnnnnnnC
	 	 */
		retval = ar3030_transaction (rig, "D" EOM, 3, infobuf, &info_len);
		if (retval != RIG_OK)
			return retval;

		priv->curr_vfo = RIG_VFO_A;
		p = strchr(infobuf, 'R');
		if (!p)
			return -RIG_EPROTO;
		val->i = p[1] == '0' ? 0 : rig->caps->attenuator[p[1] - '1'];
		return RIG_OK;

	case RIG_LEVEL_AGC:
		/*
	 	 * DRnGnBnTnFnnnnnnnnC
	 	 */
		retval = ar3030_transaction (rig, "D" EOM, 3, infobuf, &info_len);
		if (retval != RIG_OK)
			return retval;

		priv->curr_vfo = RIG_VFO_A;
		p = strchr(infobuf, 'G');
		if (!p)
			return -RIG_EPROTO;
		val->i = p[1] == '0' ? RIG_AGC_SLOW : RIG_AGC_FAST;
		return RIG_OK;

	case RIG_LEVEL_RAWSTR:
		retval = ar3030_transaction (rig, "Y" EOM, 3, infobuf, &info_len);
		if (retval != RIG_OK)
			return retval;

		infobuf[3] = '\0';
		val->i = strtol(infobuf, (char **)NULL, 16);
		return RIG_OK;

	default:
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

int ar3030_get_channel(RIG *rig, channel_t *chan)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	unsigned char cmdbuf[BUFSZ], infobuf[BUFSZ];
	int info_len, cmd_len, retval;


	cmd_len = sprintf(cmdbuf, "%02dM" EOM, chan->channel_num);
	retval = ar3030_transaction (rig, cmdbuf, cmd_len, infobuf, &info_len);
	if (retval != RIG_OK)
		return retval;

	priv->curr_vfo = RIG_VFO_A;

	/*
 	 * MnnPnRnGnBnTnFnnnnnnnnC
 	 */
	if (infobuf[0] != 'M') {
		return -RIG_EPROTO;
	}

	/*
	 * Is it a blank mem channel ?
	 */
	if (infobuf[1] == '-' && infobuf[2] == '-') {
		chan->freq = RIG_FREQ_NONE;
		return RIG_OK;
	}

	sscanf(infobuf+14,"%"FREQFMT, &chan->freq);
	chan->freq *= 10;

	switch (infobuf[22]) {
	case 'A':	chan->mode = RIG_MODE_AM; break;
	case 'L':	chan->mode = RIG_MODE_LSB; break;
	case 'U':	chan->mode = RIG_MODE_USB; break;
	case 'C':	chan->mode = RIG_MODE_CW; break;
	case 'S':	chan->mode = RIG_MODE_AMS; break;
	case 'N':	chan->mode = RIG_MODE_FM; break;
	/*case 'X':	chan->mode = RIG_MODE_FAX; break;*/
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
						__FUNCTION__,infobuf[22]);
		return -RIG_EPROTO;
	}

	chan->width = infobuf[10] == '1' ?
				rig_passband_narrow(rig, chan->mode) : 
				rig_passband_normal(rig, chan->mode);


	chan->levels[LVL_ATT].i = infobuf[6] == '0' ? 0 : rig->caps->attenuator[infobuf[4] - '1'];

	chan->levels[LVL_AGC].i = infobuf[8] == '0' ? RIG_AGC_SLOW : RIG_AGC_FAST;
	chan->flags = infobuf[4] == '1' ? RIG_CHFLAG_SKIP : RIG_CHFLAG_NONE;

	return RIG_OK;
}

int ar3030_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
	struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
	unsigned char buf[16];
	int len, retval;

	switch(op) {
	case RIG_OP_MCL:
		len = sprintf(buf,"%02d%%" EOM, priv->curr_ch);
		break;
	case RIG_OP_FROM_VFO:
		len = sprintf(buf,"%02dW" EOM, priv->curr_ch);
		priv->curr_vfo = RIG_VFO_MEM;
		break;
	default:
		return -RIG_EINVAL;
	}

	retval = ar3030_transaction(rig, buf, len, NULL, NULL);

	return retval;
}

