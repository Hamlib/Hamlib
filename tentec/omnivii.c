/*
 *  Hamlib TenTenc backend - TT-588 description
 *  Copyright (c) 2003-2009 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

struct tt588_priv_data {
	int ch;     /* mem */
	vfo_t vfo_curr;
};


#define TT588_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM)
#define TT588_RXMODES (TT588_MODES)

#define TT588_FUNCS (RIG_FUNC_NR|RIG_FUNC_ANF)

#define TT588_LEVELS (RIG_LEVEL_STRENGTH|/*RIG_LEVEL_NB|*/ \
				RIG_LEVEL_SQL|/*RIG_LEVEL_IF|*/ \
				RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD| \
				RIG_LEVEL_RF|RIG_LEVEL_NR| \
				/*RIG_LEVEL_ANF|*/RIG_LEVEL_MICGAIN| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOX| \
				RIG_LEVEL_COMP|/*RIG_LEVEL_PREAMP|*/ \
				RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT588_ANTS (RIG_ANT_1|RIG_ANT_2)

#define TT588_PARMS (RIG_PARM_NONE)

#define TT588_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT588_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define TT588_AM  '0'
#define TT588_USB '1'
#define TT588_LSB '2'
#define TT588_CW  '3'
#define TT588_FM  '4'
#define EOM "\015"      /* CR */

static int tt588_init(RIG *rig);
static int tt588_reset(RIG *rig, reset_t reset);
static int tt588_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt588_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt588_set_vfo(RIG *rig, vfo_t vfo);
static int tt588_get_vfo(RIG *rig, vfo_t *vfo);
static int tt588_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt588_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static char which_vfo(const RIG *rig, vfo_t vfo);
static int tt588_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int tt588_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);

/*
 * tt588 transceiver capabilities.
 *
 * Protocol is documented at
 *		http://www.rfsquared.com/
 *
 * Only set_freq is supposed to work.
 * This is a skelton, cloned after TT-538 Jupiter.
 */
const struct rig_caps tt588_caps = {
.rig_model =  RIG_MODEL_TT588,
.model_name = "TT-588 Omni VII",
.mfg_name =  "Ten-Tec",
.version =  "0.3",
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

.has_get_func =  TT588_FUNCS,
.has_set_func =  TT588_FUNCS,
.has_get_level =  TT588_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT588_LEVELS),
.has_get_parm =  TT588_PARMS,
.has_set_parm =  TT588_PARMS,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 10, RIG_DBLST_END }, /* FIXME: real value */
.attenuator =   { 15, RIG_DBLST_END },
.max_rit =  Hz(8192),
.max_xit =  Hz(8192),
.max_ifshift =  kHz(2),
.targetable_vfo =  RIG_TARGETABLE_FREQ|RIG_TARGETABLE_MODE,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
		{   0, 127, RIG_MTYPE_MEM, TT_MEM_CAP },
		},

.rx_range_list1 =  {
	{kHz(500),MHz(30),TT588_RXMODES,-1,-1,TT588_VFO,TT588_ANTS},
	{MHz(48),MHz(54),TT588_RXMODES,-1,-1,TT588_VFO,TT588_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,TT588_MODES, W(5),W(100),TT588_VFO,TT588_ANTS),
	FRQ_RNG_6m(1,TT588_MODES, W(5),W(100),TT588_VFO,TT588_ANTS),
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(500),MHz(30),TT588_RXMODES,-1,-1,TT588_VFO,TT588_ANTS},
	{MHz(48),MHz(54),TT588_RXMODES,-1,-1,TT588_VFO,TT588_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TT588_MODES, W(5),W(100),TT588_VFO,TT588_ANTS),
	{MHz(5.25),MHz(5.40),TT588_MODES,W(5),W(100),TT588_VFO,TT588_ANTS},
	FRQ_RNG_6m(2,TT588_MODES, W(5),W(100),TT588_VFO,TT588_ANTS),
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT588_RXMODES,1},
	 {TT588_RXMODES,10},
	 {TT588_RXMODES,100},
	 {TT588_RXMODES,kHz(1)},
	 {TT588_RXMODES,kHz(10)},
	 {TT588_RXMODES,kHz(100)},
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

.rig_init =  tt588_init,
.set_freq =  tt588_set_freq,
.get_freq =  tt588_get_freq,
.set_vfo =  tt588_set_vfo,
.get_vfo =  tt588_get_vfo,
.set_mode =  tt588_set_mode,
.get_mode =  tt588_get_mode,
.get_level =  tt588_get_level,
.set_level =  tt588_set_level,
.set_split_vfo =  tentec2_set_split_vfo,
.get_split_vfo =  tentec2_get_split_vfo,
.set_ptt =  tentec2_set_ptt,
.reset =  tt588_reset,
.get_info =  tentec2_get_info,

};

/* Filter table for 588 reciver support. */
static int tt588_rxFilter[] = {
  12000, 9000, 8000, 7500, 7000, 6500, 6000, 5500, 5000, 4500, 4000, 3800, 3600, 3400, 3200,
  3000, 2800, 2600, 2500, 2400, 2200, 2000, 1800, 1600, 1400,
  1200, 1000, 900, 800, 700, 600, 500, 450, 400, 350, 300, 250, 200
};

/*
 * Function definitions below
 */

/* I frequently see the Omni VII and my laptop get out of sync.  A
   response from the 538 isn't seen by the laptop.  A few "XX"s
   sometimes get things going again, hence this hack, er, function. */

static int tt588_transaction (RIG *rig, const char *cmd, int cmd_len,
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
 * tt588_init:
 * Basically, it just sets up *priv
 */
int tt588_init(RIG *rig)
{
	struct tt588_priv_data *priv;

	priv = (struct tt588_priv_data *) malloc(sizeof(struct tt588_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	memset(priv, 0, sizeof(struct tt588_priv_data));

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
	struct tt588_priv_data *priv = (struct tt588_priv_data *)rig->state.priv;

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

int tt588_get_vfo(RIG *rig, vfo_t *vfo) {

	struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;
	*vfo = priv->vfo_curr;
	return RIG_OK;
}

/*
 * tt588_set_vfo
 * Assumes rig!=NULL
 */
int tt588_set_vfo(RIG *rig, vfo_t vfo)
{
	struct tt588_priv_data *priv = (struct tt588_priv_data *)rig->state.priv;

	if (vfo == RIG_VFO_CURR)
		return RIG_OK;

	priv->vfo_curr = vfo;
	return RIG_OK;
}

/*
 * Software restart
 */
int tt588_reset(RIG *rig, reset_t reset) {
	int retval, reset_len;
	char reset_buf[32];

	reset_len = 32;
	retval = tt588_transaction (rig, "XX" EOM, 3, reset_buf, &reset_len);
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
 * tt588_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt588_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {

	char	curVfo;
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];

	cmd_len = sprintf((char *) cmdbuf, "?%c" EOM, which_vfo(rig, vfo));
	resp_len = 32;
	retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) respbuf, &resp_len);

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
 * tt588_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */

int tt588_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	char	bytes[4];
	int cmd_len;
	unsigned char cmdbuf[16];

	/* Freq is 4 bytes long, MSB sent first. */
	bytes[3] = ((int) freq >> 24) & 0xff;
	bytes[2] = ((int) freq >> 16) & 0xff;
	bytes[1] = ((int) freq >>  8) & 0xff;
	bytes[0] =  (int) freq        & 0xff;

	cmd_len = sprintf((char *) cmdbuf, "*%c%c%c%c%c" EOM,
                          which_vfo(rig, vfo),
                          bytes[3], bytes[2], bytes[1], bytes[0]);

	return tt588_transaction(rig, (char *) cmdbuf, cmd_len, NULL, NULL);
}

/*
 * tt588_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tt588_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	int cmd_len, resp_len, retval;
	unsigned char cmdbuf[16], respbuf[32];
	char ttmode;

	/* Query mode */
	cmd_len = sprintf((char *) cmdbuf, "?M" EOM);
	resp_len = 32;
	retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) respbuf, &resp_len);

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
	case TT588_AM:	*mode = RIG_MODE_AM;  break;
	case TT588_USB: *mode = RIG_MODE_USB; break;
	case TT588_LSB: *mode = RIG_MODE_LSB; break;
	case TT588_CW: *mode = RIG_MODE_CW;  break;
	case TT588_FM: *mode = RIG_MODE_FM;  break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
			__FUNCTION__, ttmode);
		return -RIG_EPROTO;
	}

	/* Query passband width (filter) */
	cmd_len = sprintf((char *) cmdbuf, "?W" EOM);
	resp_len = 32;
	retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) respbuf, &resp_len);

	if (retval != RIG_OK)
		return retval;

	if (respbuf[0] != 'W' && resp_len != 3) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
			__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	switch (respbuf[1]) {
	case 0: *width = 12000; break;
	case 1: *width = 9000; break;
	case 2: *width = 8000; break;
	case 3: *width = 7500; break;
	case 4: *width = 7000; break;
	case 5: *width = 6500; break;
	case 6: *width = 6000; break;
	case 7: *width = 5500; break;
	case 8: *width = 5000; break;
	case 9: *width = 4500; break;
	case 10: *width = 4000; break;
	case 11: *width = 3800; break;
	case 12: *width = 3600; break;
	case 13: *width = 3400; break;
	case 14: *width = 3200; break;
	case 15: *width = 3000; break;
	case 16: *width = 2800; break;
	case 17: *width = 2600; break;
	case 18: *width = 2500; break;
	case 19: *width = 2400; break;
	case 20: *width = 2200; break;
	case 21: *width = 2000; break;
	case 22: *width = 1800; break;
	case 23: *width = 1600; break;
	case 24: *width = 1400; break;
	case 25: *width = 1200; break;
	case 26: *width = 1000; break;
	case 27: *width = 900; break;
	case 28: *width = 800; break;
	case 29: *width = 700; break;
	case 30: *width = 600; break;
	case 31: *width = 500; break;
	case 32: *width = 450; break;
	case 33: *width = 400; break;
	case 34: *width = 350; break;
	case 35: *width = 300; break;
	case 36: *width = 250; break;
	case 37: *width = 200; break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected bandwidth '%c'\n",
			__FUNCTION__, respbuf[1]);
		return -RIG_EPROTO;
	}

	return RIG_OK;
}

/* Find rx filter index of bandwidth the same or larger as requested. */
static int tt588_filter_number(int width)
{
	int	i;

	for (i = 34; i >= 0; i--)
		if (width <= tt588_rxFilter[i])
			return i;
	return 0; /* Widest filter, 8 kHz. */
}


/*
 * tt588_set_mode
 * Assumes rig!=NULL
 */
int tt588_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char cmdbuf[32], respbuf[32], ttmode;
	int cmd_len, resp_len, retval;

	struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

	/* Query mode for both VFOs. */
	cmd_len = sprintf((char *) cmdbuf, "?M" EOM);
	resp_len = 32;
	retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) respbuf, &resp_len);
	if (retval != RIG_OK)
		return retval;
	if (respbuf[0] != 'M' || resp_len != 4) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
			__FUNCTION__, respbuf);
		return -RIG_EPROTO;
	}

	switch (mode) {
	case RIG_MODE_USB:	ttmode = TT588_USB; break;
	case RIG_MODE_LSB:	ttmode = TT588_LSB; break;
	case RIG_MODE_CW:	ttmode = TT588_CW; break;
	case RIG_MODE_AM:	ttmode = TT588_AM; break;
	case RIG_MODE_FM:	ttmode = TT588_FM; break;
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
		cmd_len = sprintf((char *) cmdbuf, "*M%c%c" EOM, ttmode, respbuf[2]);
		break;
	case RIG_VFO_B:
		cmd_len = sprintf((char *) cmdbuf, "*M%c%c" EOM, respbuf[1], ttmode);
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, rig_strvfo(vfo));
		return -RIG_EINVAL;
	}

	retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, NULL, NULL);
	if (retval != RIG_OK)
		return retval;

	/* Set rx filter bandwidth. */

	if (width == RIG_PASSBAND_NORMAL)
		width = tt588_filter_number(rig_passband_normal(rig, mode));
	else
		width = tt588_filter_number((int) width);

	cmd_len = sprintf((char *) cmdbuf, "*W%c" EOM, (unsigned char) width);
	return tt588_transaction (rig, (char *) cmdbuf, cmd_len, NULL, NULL);
}

/*
 * tt588_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt588_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
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
		retval = tt588_transaction (rig, "?F" EOM, 3, (char *) lvlbuf, &lvl_len);
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
		retval = tt588_transaction (rig, "?R" EOM, 3, (char *) lvlbuf, &lvl_len);
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

	case RIG_LEVEL_STRENGTH:
		retval = tt588_transaction (rig, "?S" EOM, 3, (char *) lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[0] != 'S' || lvl_len != 6) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		/* Reply in the form S0944 for 44 dB over S9 */
		/* TODO: check whether in RX or TX mode */
		val->i = (int)lvlbuf[2] * 6 - 54 + lvlbuf[3]*10 + lvlbuf[4];

		break;

	case RIG_LEVEL_RFPOWER:

		/* Get forward power in volts. */
		lvl_len = 32;
		retval = tt588_transaction (rig, "?P" EOM, 3, (char *) lvlbuf, &lvl_len);
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
		cmd_len = sprintf((char *) cmdbuf, "?G" EOM);
		lvl_len = 32;
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		if (lvlbuf[0] != 'G' || lvl_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		switch(lvlbuf[1]) {
		case '0': val->i=RIG_AGC_OFF; break;
		case '1': val->i=RIG_AGC_SLOW; break;
		case '2': val->i=RIG_AGC_MEDIUM; break;
		case '3': val->i=RIG_AGC_FAST; break;
		default: return -RIG_EPROTO;
		}
		break;

	case RIG_LEVEL_AF:

		/* Volume returned as single byte. */
		cmd_len = sprintf((char *) cmdbuf, "?U" EOM);
		lvl_len = 32;
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) lvlbuf, &lvl_len);
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
		cmd_len = sprintf((char *) cmdbuf, "?R%cP" EOM,
                                  which_receiver(rig, vfo));

		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) lvlbuf, &lvl_len);
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

		cmd_len = sprintf((char *) cmdbuf, "?I" EOM);
		lvl_len = 32;
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) lvlbuf, &lvl_len);
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

		cmd_len = sprintf((char *) cmdbuf, "?J" EOM);
		lvl_len = 32;
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) lvlbuf, &lvl_len);
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

		cmd_len = sprintf((char *) cmdbuf, "?H" EOM);
		lvl_len = 32;
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, (char *) lvlbuf, &lvl_len);
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
		retval = tt588_transaction (rig, "?O" EOM, 3, (char *) lvlbuf, &lvl_len);
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
		retval = tt588_transaction (rig, "?S" EOM, 3, (char *) lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[0] != 'S' || lvl_len != 6) {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer '%s'\n",
					__FUNCTION__, lvlbuf);
			return -RIG_EPROTO;
		}

		sprintf((char *) sunits, "%c%c.%c%c",
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

/*
 * tt588_set_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt588_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	int retval, cmd_len;
	unsigned char cmdbuf[16], agcmode;


	switch (level) {
	case RIG_LEVEL_AF:

		/* Volume */
		cmd_len = sprintf((char *) cmdbuf, "*U%c" EOM, (char)(val.f * 127));
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, NULL, NULL);
		if (retval != RIG_OK)
			return retval;

		break;

	case RIG_LEVEL_RF:

		/* RF gain. Omni-VII expects value 0 for full gain, and 127 for lowest gain */
		cmd_len = sprintf((char *) cmdbuf, "*I%c" EOM, (char)(127- val.f * 127));
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, NULL, NULL);
		if (retval != RIG_OK)
			return retval;

		break;

	case RIG_LEVEL_AGC:

		switch(val.i) {
		case RIG_AGC_OFF:    agcmode = '0'; break;
		case RIG_AGC_SLOW:   agcmode = '1'; break;
		case RIG_AGC_MEDIUM: agcmode = '2'; break;
		case RIG_AGC_FAST:   agcmode = '3'; break;
		default: return -RIG_EINVAL;
		}

		cmd_len = sprintf((char *) cmdbuf, "*G%c" EOM, agcmode);
		retval = tt588_transaction (rig, (char *) cmdbuf, cmd_len, NULL, NULL);
		if (retval != RIG_OK)
			return retval;

		break;

	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported level %d\n",
				__FUNCTION__, level);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

