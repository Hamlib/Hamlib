
/*
 *  Hamlib TenTenc backend - TT-538 description
 *  Copyright (c) 2003-2004 by Stephane Fillod
 *
 *	$Id: jupiter.c,v 1.2 2004-11-08 21:52:30 fillods Exp $
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
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "tentec2.h"
#include "tentec.h"
#include "bandplan.h"

struct tt538_priv_data {
	int ch;     /* mem */
	vfo_t vfo_curr;
};


#define TT538_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM)
#define TT538_RXMODES (TT538_MODES)

#define TT538_FUNCS (RIG_FUNC_NR|RIG_FUNC_ANF)

#define TT538_LEVELS_OLD (RIG_LEVEL_RAWSTR|/*RIG_LEVEL_NB|*/ \
				RIG_LEVEL_RF|RIG_LEVEL_IF| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_SQL|RIG_LEVEL_ATT)

#define TT538_LEVELS (RIG_LEVEL_RAWSTR|/*RIG_LEVEL_NB|*/ \
				RIG_LEVEL_SQL|/*RIG_LEVEL_IF|*/ \
				RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD| \
				RIG_LEVEL_RF|RIG_LEVEL_NR| \
				/*RIG_LEVEL_ANF|*/RIG_LEVEL_MICGAIN| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOX| \
				RIG_LEVEL_COMP|/*RIG_LEVEL_PREAMP|*/ \
				RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT538_ANTS (RIG_ANT_1) 

#define TT538_PARMS (RIG_PARM_NONE)

#define TT538_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT538_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define TT538_AM  '0'
#define TT538_USB '1'
#define TT538_LSB '2'
#define TT538_CW  '3'
#define TT538_FM  '4'
#define EOM "\015"      /* CR */

static int tt538_init(RIG *rig);
static int tt538_reset(RIG *rig, reset_t reset);
static int tt538_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt538_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt538_set_vfo(RIG *rig, vfo_t vfo);
static int tt538_get_vfo(RIG *rig, vfo_t *vfo);
static int tt538_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt538_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static char which_vfo(const RIG *rig, vfo_t vfo);
static int tt538_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

/*
 * tt538 transceiver capabilities.
 *
 * Protocol is documented at 
 *		http://www.rfsquared.com/
 *
 * Only set_freq is supposed to work.
 * This is a skelton.
 */
const struct rig_caps tt538_caps = {
.rig_model =  RIG_MODEL_TT538,
.model_name = "TT-538 Jupiter",
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

.has_get_func =  TT538_FUNCS,
.has_set_func =  TT538_FUNCS,
.has_get_level =  TT538_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT538_LEVELS),
.has_get_parm =  TT538_PARMS,
.has_set_parm =  TT538_PARMS,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { 15, RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  kHz(2),
.targetable_vfo =  RIG_TARGETABLE_FREQ|RIG_TARGETABLE_MODE,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
		{   0, 127, RIG_MTYPE_MEM, TT_MEM_CAP },
		},

.rx_range_list1 =  {
	{kHz(100),MHz(30),TT538_RXMODES,-1,-1,TT538_VFO,TT538_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,TT538_MODES, W(5),W(100),TT538_VFO,TT538_ANTS),
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(100),MHz(30),TT538_RXMODES,-1,-1,TT538_VFO,TT538_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TT538_MODES, W(5),W(100),TT538_VFO,TT538_ANTS),
	{MHz(5.25),MHz(5.40),TT538_MODES,W(5),W(100),TT538_VFO,TT538_ANTS},
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT538_RXMODES,1},
	 {TT538_RXMODES,10},
	 {TT538_RXMODES,100},
	 {TT538_RXMODES,kHz(1)},
	 {TT538_RXMODES,kHz(10)},
	 {TT538_RXMODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM, kHz(2.4)},
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM, 300},
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM, kHz(8)},
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM, 0}, /* 34 filters */
		{RIG_MODE_FM, kHz(15)},	/* TBC */
		RIG_FLT_END,
	},
.priv =  (void*) NULL,

.rig_init =  tt538_init,
.set_freq =  tt538_set_freq,
.get_freq =  tt538_get_freq,
.set_vfo =  tt538_set_vfo,
.get_vfo =  tt538_get_vfo,
.set_mode =  tt538_set_mode,
.get_mode =  tt538_get_mode,
.get_level =  tt538_get_level,
.set_split_vfo =  tentec2_set_split_vfo,
.get_split_vfo =  tentec2_get_split_vfo,
.set_ptt =  tentec2_set_ptt,
.reset =  tt538_reset,
.get_info =  tentec2_get_info,

};

/* Filter table for 538 reciver support. */
static int tt538_rxFilter[] = {
  8000, 6000, 5700, 5400, 5100, 4800, 4500, 4200, 3900, 3600, 3300,
  3000, 2850, 2700, 2550, 2400, 2250, 2100, 1950, 1800, 1650, 1500,
  1350, 1200, 1050, 900, 750, 675, 600, 525, 450, 375, 330, 300
};

/*
 * Function definitions below
 */

/* I frequently see the Jupiter and my laptop get out of sync.  A
   response from the 538 isn't seen by the laptop.  A few "XX"s
   sometimes get things going again, hence this hack, er, function. */

int tt538_transaction (RIG *rig, const char *cmd, int cmd_len,
	char *data, int *data_len)
{
	char	reset_buf[32];
	int	i, reset_len, retval;

	retval = tentec_transaction (rig, cmd, cmd_len, data, data_len);
	if (retval == RIG_OK)
		return retval;

	/* Try a few times to do a DSP reset to resync things. */
	for (i = 0; i < 3; i++) {
		reset_len = 32;
		retval = tentec_transaction (rig, "XX" EOM, 3, reset_buf, &reset_len);
		if (retval != RIG_OK)
			continue; /* Try again.  This 1 didn't work. */
		if (strstr(reset_buf, "RADIO START"))
			break; /* DSP reset successful! */
	}

	/* Try real command one last time... */
	return tentec_transaction (rig, cmd, cmd_len, data, data_len);
}

/*
 * tt538_init:
 * Basically, it just sets up *priv
 */
int tt538_init(RIG *rig)
{
	struct tt538_priv_data *priv;

	priv = (struct tt538_priv_data *) malloc(sizeof(struct tt538_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	memset(priv, 0, sizeof(struct tt538_priv_data));

	/*
	 * set arbitrary initial status
	 */
	priv->ch = 0;
	priv->vfo_curr = RIG_VFO_A;
	rig->state.priv = (rig_ptr_t)priv;

	return RIG_OK;
}

static char which_vfo(const RIG *rig, vfo_t vfo)
{
	struct tt538_priv_data *priv = (struct tt538_priv_data *)rig->state.priv;

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

int tt538_get_vfo(RIG *rig, vfo_t *vfo) {

	struct tt538_priv_data *priv = (struct tt538_priv_data *) rig->state.priv;
	*vfo = priv->vfo_curr;
	return RIG_OK;
}

/*
 * tt538_set_vfo
 * Assumes rig!=NULL
 */
int tt538_set_vfo(RIG *rig, vfo_t vfo)
{
	struct tt538_priv_data *priv = (struct tt538_priv_data *)rig->state.priv;

	if (vfo == RIG_VFO_CURR)
		return RIG_OK;

	priv->vfo_curr = vfo;
	return RIG_OK;
}

/*
 * Software restart
 */
int tt538_reset(RIG *rig, reset_t reset) {
	int retval, reset_len;
	char reset_buf[32];

	reset_len = 32;
	retval = tt538_transaction (rig, "XX" EOM, 3, reset_buf, &reset_len);
	if (retval != RIG_OK)
		return retval;

	if (!strstr(reset_buf, "RADIO START")) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
					__FUNCTION__, reset_buf);
		return -RIG_EPROTO;
	}

	return RIG_OK;
}

/*
 * tt538_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt538_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {

	char	curVfo;
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];

	cmd_len = sprintf(cmdbuf, "?%c" EOM, which_vfo(rig, vfo));
	resp_len = 32;
	retval = tt538_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK) 
		return retval;

	curVfo = which_vfo(rig, vfo);
	if (respbuf[0] != curVfo) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
			__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}
	if (resp_len != 6) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected length '%d'\n",
			__FUNCTION__, resp_len);
		return -RIG_EPROTO;
	}

	*freq = (respbuf[1] << 24)
		+ (respbuf[2] << 16)
		+ (respbuf[3] << 8)
		+ respbuf[4];

	return RIG_OK;
}

/*
 * tt538_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */

int tt538_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	char	bytes[4];
	int cmd_len;
	unsigned char cmdbuf[16];

	/* Freq is 4 bytes long, MSB sent first. */
	bytes[3] = ((int) freq >> 24) & 0xff;
	bytes[2] = ((int) freq >> 16) & 0xff;
	bytes[1] = ((int) freq >>  8) & 0xff;
	bytes[0] =  (int) freq        & 0xff;

	cmd_len = sprintf(cmdbuf, "*%c%c%c%c%c" EOM,
		which_vfo(rig, vfo),
			bytes[3], bytes[2], bytes[1], bytes[0]);

	return tt538_transaction(rig, cmdbuf, cmd_len, NULL, NULL); 
}

/*
 * tt538_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tt538_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];
	char ttmode;

	/* Query mode */
	cmd_len = sprintf(cmdbuf, "?M" EOM);
	resp_len = 32;
	retval = tt538_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[0] != 'M' || resp_len != 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
			__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	switch (which_vfo(rig, vfo)) {
	case 'A':
		ttmode = respbuf[1]; 
		break;
	case 'B':
		ttmode = respbuf[2]; 
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
			__FUNCTION__, rig_strvfo(vfo));
		return -RIG_EINVAL;
		break;
	}

	switch (ttmode) {
	case TT538_AM:	*mode = RIG_MODE_AM;  break;
	case TT538_USB: *mode = RIG_MODE_USB; break;
	case TT538_LSB: *mode = RIG_MODE_LSB; break;
	case TT538_CW: *mode = RIG_MODE_CW;  break;
	case TT538_FM: *mode = RIG_MODE_FM;  break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
			__FUNCTION__, ttmode);
		return -RIG_EPROTO;
	}

	/* Query passband width (filter) */
	cmd_len = sprintf(cmdbuf, "?W" EOM);
	resp_len = 32;
	retval = tt538_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[0] != 'W' && resp_len != 3) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
			__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	switch (respbuf[1]) {
	case 0: *width = 8000; break;
	case 1: *width = 6000; break;
	case 2: *width = 5700; break;
	case 3: *width = 5400; break;
	case 4: *width = 5100; break;
	case 5: *width = 4800; break;
	case 6: *width = 4500; break;
	case 7: *width = 4200; break;
	case 8: *width = 3900; break;
	case 9: *width = 3600; break;
	case 10: *width = 3300; break;
	case 11: *width = 3000; break;
	case 12: *width = 2850; break;
	case 13: *width = 2700; break;
	case 14: *width = 2550; break;
	case 15: *width = 2400; break;
	case 16: *width = 2250; break;
	case 17: *width = 2100; break;
	case 18: *width = 1950; break;
	case 19: *width = 1800; break;
	case 20: *width = 1650; break;
	case 21: *width = 1500; break;
	case 22: *width = 1350; break;
	case 23: *width = 1200; break;
	case 24: *width = 1050; break;
	case 25: *width = 900; break;
	case 26: *width = 750; break;
	case 27: *width = 675; break;
	case 28: *width = 600; break;
	case 29: *width = 525; break;
	case 30: *width = 450; break;
	case 31: *width = 375; break;
	case 32: *width = 330; break;
	case 33: *width = 300; break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected bandwidth '%c'\n",
			__FUNCTION__, respbuf[1]);
		return -RIG_EPROTO;
	}

	return RIG_OK;
}

/* Find rx filter index of bandwidth the same or larger as requested. */
int tt538_filter_number(int width)
{
	int	i;

	for (i = 34; i >= 0; i--)
		if (width <= tt538_rxFilter[i])
			return i;
	return 0; /* Widest filter, 8 kHz. */
}


/*
 * tt538_set_mode
 * Assumes rig!=NULL
 */
int tt538_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char cmdbuf[32], respbuf[32], ttmode;
	int cmd_len, resp_len, retval;

	struct tt538_priv_data *priv = (struct tt538_priv_data *) rig->state.priv;

	/* Query mode for both VFOs. */
	cmd_len = sprintf(cmdbuf, "?M" EOM);
	resp_len = 32;
	retval = tt538_transaction (rig, cmdbuf, cmd_len, respbuf, &resp_len);
	if (retval != RIG_OK)
		return retval;
	if (respbuf[0] != 'M' || resp_len != 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
			__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	switch (mode) {
	case RIG_MODE_USB:	ttmode = TT538_USB; break;
	case RIG_MODE_LSB:	ttmode = TT538_LSB; break;
	case RIG_MODE_CW:	ttmode = TT538_CW; break;
	case RIG_MODE_AM:	ttmode = TT538_AM; break;
	case RIG_MODE_FM:	ttmode = TT538_FM; break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %d\n",
			__FUNCTION__, mode);
		return -RIG_EINVAL;
	}

	/* Set mode for both VFOs. */
	if (vfo == RIG_VFO_CURR)
		vfo = priv->vfo_curr;
	switch (vfo) {
	case RIG_VFO_A:
		cmd_len = sprintf(cmdbuf, "*M%c%c" EOM, ttmode, respbuf[2]);
		break;
	case RIG_VFO_B:
		cmd_len = sprintf(cmdbuf, "*M%c%c" EOM, respbuf[1], ttmode);
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, rig_strvfo(vfo));
		return -RIG_EINVAL;
	}

	retval = tt538_transaction (rig, cmdbuf, cmd_len, NULL, NULL);
	if (retval != RIG_OK)
		return retval;

	/* Set rx filter bandwidth. */

	if (width == RIG_PASSBAND_NORMAL)
		width = tt538_filter_number(rig_passband_normal(rig, mode));
	else
		width = tt538_filter_number((int) width);

	cmd_len = sprintf(cmdbuf, "*W%c" EOM, (unsigned char) width); 
	return tt538_transaction (rig, cmdbuf, cmd_len, NULL, NULL);
}

/*
 * tt538_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt538_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	char	sunits[6];
	float	fwd, refl, sstr;
	int retval, cmd_len, lvl_len;
	unsigned char cmdbuf[16],lvlbuf[32];


	/* Optimize:
	 *   sort the switch cases with the most frequent first
	 */
	switch (level) {
	case RIG_LEVEL_SWR:
		/* Get forward power. */
		lvl_len = 32;
		retval = tt538_transaction (rig, "?F" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[0] != 'F' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		fwd = (float) lvlbuf[1];

		/* Get reflected power. */
		lvl_len = 32;
		retval = tt538_transaction (rig, "?R" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[0] != 'R' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		refl = (float) lvlbuf[1];

		val->f = fwd/refl;
		break;

	case RIG_LEVEL_RAWSTR:
		retval = tt538_transaction (rig, "?S" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[0] != 'S' || lvl_len != 6) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

#if 0
		sprintf(sunits, "%c%c.%c%c",
			lvlbuf[1], lvlbuf[2], lvlbuf[3], lvlbuf[4]);
		sscanf(sunits, "%f", &sstr);
		val->f = sstr;
printf("%f\n", val->f);
#endif

		sprintf(sunits, "%c%c.%c%c",
			lvlbuf[1], lvlbuf[2], lvlbuf[3], lvlbuf[4]);
		sscanf(sunits, "%f", &sstr);
		val->i = (int) sstr;

		break;

	case RIG_LEVEL_RFPOWER:

		/* Get forward power in volts. */
		lvl_len = 32;
		retval = tt538_transaction (rig, "?P" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[0] != 'P' || lvl_len != 4) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		val->f = 100 * (float) lvlbuf[1] / 0xff;

		break;

	case RIG_LEVEL_AGC:

		/* Read rig's AGC level setting. */
		cmd_len = sprintf(cmdbuf, "?G" EOM);
		lvl_len = 32;
		retval = tt538_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[0] != 'G' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		switch(lvlbuf[1]) {
		case '1': val->i=RIG_AGC_SLOW; break;
		case '2': val->i=RIG_AGC_MEDIUM; break;
		case '3': val->i=RIG_AGC_FAST; break;
		default: return -RIG_EPROTO;
		}
		break;

	case RIG_LEVEL_AF:

		/* Volume returned as single byte. */
		cmd_len = sprintf(cmdbuf, "?U" EOM);
		lvl_len = 32;
		retval = tt538_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[0] != 'U' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = (float) lvlbuf[1] / 127;
		break;

	case RIG_LEVEL_IF:
#if 0
NO IF MONITOR??
		cmd_len = sprintf(cmdbuf, "?R%cP" EOM,
				which_receiver(rig, vfo));

		retval = tt538_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[1] != 'R' || lvlbuf[3] != 'P' || lvl_len < 5) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->i = atoi(lvlbuf+4);
#endif
		val->i = 0;
		break;

	case RIG_LEVEL_RF:

		cmd_len = sprintf(cmdbuf, "?I" EOM);
		lvl_len = 32;
		retval = tt538_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[0] != 'I' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = 1 - (float) lvlbuf[1] / 0xff;
		break;

	case RIG_LEVEL_ATT:

		cmd_len = sprintf(cmdbuf, "?J" EOM);
		lvl_len = 32;
		retval = tt538_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[0] != 'J' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		val->i = lvlbuf[1];
		break;

	case RIG_LEVEL_PREAMP:
		/* Receiver does not contain a preamp */
		val->i=0;
		break;

	case RIG_LEVEL_SQL:

		cmd_len = sprintf(cmdbuf, "?H" EOM);
		lvl_len = 32;
		retval = tt538_transaction (rig, cmdbuf, cmd_len, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[0] != 'H' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}
		val->f = ((float) lvlbuf[1] / 127);
		break;

	case RIG_LEVEL_MICGAIN:

		lvl_len = 3;
		retval = tt538_transaction (rig, "?O" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[0] != 'O' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		val->f = (float) lvlbuf[2] / 0x0f;
		break;

	case RIG_LEVEL_COMP:
		/* Query S units signal level. */
		lvl_len = 32;
		retval = tt538_transaction (rig, "?S" EOM, 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[0] != 'S' || lvl_len != 6) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		sprintf(sunits, "%c%c.%c%c",
			lvlbuf[1], lvlbuf[2], lvlbuf[3], lvlbuf[4]);
		sscanf(sunits, "%f", &sstr);
printf("%f\n", sstr);
		val->f = sstr;
		break;


	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported level %d\n", 
				__FUNCTION__, level);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

