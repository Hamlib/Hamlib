/*
 *  Hamlib CI-V backend - main file
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: icom.c,v 1.92 2004-09-26 08:35:03 fillods Exp $
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
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <token.h>
#include <register.h>

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

const struct ts_sc_list r8500_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 50, 0x01 },
		{ 100, 0x02 },
		{ kHz(1), 0x03 },
		{ 12500, 0x04 },
		{ kHz(5), 0x05 },
		{ kHz(9), 0x06 },
		{ kHz(10), 0x07 },
		{ 12500, 0x08 },
		{ kHz(20), 0x09 },
		{ kHz(25), 0x10 },
		{ kHz(100), 0x11 },
		{ MHz(1), 0x12 },
		{ 0, 0x13 },	/* programmable tuning step not supported */
		{ 0, 0 },
};

const struct ts_sc_list ic737_ts_sc_list[] = {
		{ 10, 0x00 },
		{ kHz(1), 0x01 },
		{ kHz(2), 0x02 },
		{ kHz(3), 0x03 },
		{ kHz(4), 0x04 },
		{ kHz(5), 0x05 },
		{ kHz(6), 0x06 },
		{ kHz(7), 0x07 },
		{ kHz(8), 0x08 },
		{ kHz(9), 0x09 },
		{ kHz(10), 0x10 },
		{ 0, 0 },
};

const struct ts_sc_list r75_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 100, 0x01 },
		{ kHz(1), 0x02 },
		{ kHz(5), 0x03 },
		{ 6250, 0x04 },
		{ kHz(9), 0x05 },
		{ kHz(10), 0x06 },
		{ 12500, 0x07 },
		{ kHz(20), 0x08 },
		{ kHz(25), 0x09 },
		{ kHz(100), 0x10 },
		{ MHz(1), 0x11 },
		{ 0, 0 },
};

const struct ts_sc_list r7100_ts_sc_list[] = {
		{ 100, 0x00 },
		{ kHz(1), 0x01 },
		{ kHz(5), 0x02 },
		{ kHz(10), 0x03 },
		{ 12500, 0x04 },
		{ kHz(20), 0x05 },
		{ kHz(25), 0x06 },
		{ kHz(100), 0x07 },
		{ 0, 0 },
};

const struct ts_sc_list r9000_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 100, 0x01 },
		{ kHz(1), 0x02 },
		{ kHz(5), 0x03 },
		{ kHz(9), 0x04 },
		{ kHz(10), 0x05 },
		{ 12500, 0x06 },
		{ kHz(20), 0x07 },
		{ kHz(25), 0x08 },
		{ kHz(100), 0x09 },
		{ 0, 0 },
};

const struct ts_sc_list ic718_ts_sc_list[] = {
		{ 10, 0x00 },
		{ kHz(1), 0x01 },
		{ kHz(5), 0x01 },
		{ kHz(9), 0x01 },
		{ kHz(10), 0x04 },
		{ kHz(100), 0x05 },
		{ 0, 0 },
};

const struct ts_sc_list ic756_ts_sc_list[] = {
		{ 10, 0x00 },
		{ kHz(1), 0x01 },
		{ kHz(5), 0x02 },
		{ kHz(9), 0x03 },
		{ kHz(10), 0x04 },
		{ 0, 0 },
};

const struct ts_sc_list ic756pro_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 100, 0x01 },
		{ kHz(1), 0x02 },
		{ kHz(5), 0x03 },
		{ kHz(9), 0x04 },
		{ kHz(10), 0x05 },
		{ kHz(12.5), 0x06 },
		{ kHz(20), 0x07 },
		{ kHz(25), 0x08 },
		{ 0, 0 },
};

const struct ts_sc_list ic706_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 100, 0x01 },
		{ kHz(1), 0x02 },
		{ kHz(5), 0x03 },
		{ kHz(9), 0x04 },
		{ kHz(10), 0x05 },
		{ 12500, 0x06 },
		{ kHz(20), 0x07 },
		{ kHz(25), 0x08 },
		{ kHz(100), 0x09 },
		{ 0, 0 },
};

const struct ts_sc_list ic910_ts_sc_list[] = {
        { Hz(1), 0x00 },
        { Hz(10), 0x01 },
        { Hz(50), 0x02 },
        { Hz(100), 0x03 },
        { kHz(1), 0x04 },
        { kHz(5), 0x05 },
        { kHz(6.25), 0x06 },
        { kHz(10), 0x07 },
        { kHz(12.5), 0x08 },
        { kHz(20), 0x09 },
        { kHz(25), 0x10 },
        { kHz(100), 0x11 },
        { 0, 0 },
};

struct icom_addr {
		rig_model_t model;
		unsigned char re_civ_addr;
};


#define TOK_CIVADDR TOKEN_BACKEND(1)
#define TOK_MODE731 TOKEN_BACKEND(2)

const struct confparams icom_cfg_params[] = {
	{ TOK_CIVADDR, "civaddr", "CI-V address", "Transceiver's CI-V address",
			"0", RIG_CONF_NUMERIC, { .n = { 0, 0xff, 1 } }
	},
	{ TOK_MODE731, "mode731", "CI-V 731 mode", "CI-V operating frequency "
			"data length, needed for IC731 and IC735",
			"0", RIG_CONF_CHECKBUTTON
	},
	{ RIG_CONF_END, NULL, }
};

/*
 * Please, if the default CI-V address of your rig is listed as UNKNOWN_ADDR,
 * send the value to <f4cfe@users.sourceforge.net> for inclusion. Thanks --SF
 *
 * TODO: sort this list with most frequent rigs first.
 */
static const struct icom_addr icom_addr_list[] = {
		{ RIG_MODEL_IC703, 0x68 },
		{ RIG_MODEL_IC706, 0x48 },
		{ RIG_MODEL_IC706MKII, 0x4e },
		{ RIG_MODEL_IC706MKIIG, 0x58 },
		{ RIG_MODEL_IC271, 0x20 },
		{ RIG_MODEL_IC275, 0x10 },
		{ RIG_MODEL_IC375, 0x12 },
		{ RIG_MODEL_IC471, 0x22 },
		{ RIG_MODEL_IC475, 0x14 },
		{ RIG_MODEL_IC575, 0x16 },
		{ RIG_MODEL_IC707, 0x3e },
		{ RIG_MODEL_IC725, 0x28 },
		{ RIG_MODEL_IC726, 0x30 },
		{ RIG_MODEL_IC728, 0x38 },
		{ RIG_MODEL_IC729, 0x3a },
		{ RIG_MODEL_IC731, 0x02 },	/* need confirmation */
		{ RIG_MODEL_IC735, 0x04 },
		{ RIG_MODEL_IC736, 0x40 },
		{ RIG_MODEL_IC746, 0x56 },
		{ RIG_MODEL_IC746PRO, 0x66 },
		{ RIG_MODEL_IC737, 0x3c },
		{ RIG_MODEL_IC738, 0x44 },
		{ RIG_MODEL_IC751, 0x1c },
		{ RIG_MODEL_IC751A, 0x1c },
		{ RIG_MODEL_IC756, 0x50 },
		{ RIG_MODEL_IC756PRO, 0x5c },
		{ RIG_MODEL_IC756PROII, 0x64 },
		{ RIG_MODEL_IC756PROIII, 0x6e },
		{ RIG_MODEL_IC761, 0x1e },
		{ RIG_MODEL_IC765, 0x2c },
		{ RIG_MODEL_IC775, 0x46 },
		{ RIG_MODEL_IC7800, 0x6a },
		{ RIG_MODEL_IC781, 0x26 },
		{ RIG_MODEL_IC820, 0x42 },
		{ RIG_MODEL_IC821, 0x4c },
		{ RIG_MODEL_IC821H, 0x4c },
		{ RIG_MODEL_IC910, 0x60 },
		{ RIG_MODEL_IC970, 0x2e },
		{ RIG_MODEL_IC1271, 0x24 },
		{ RIG_MODEL_IC1275, 0x18 },
		{ RIG_MODEL_ICR10, 0x52 },
		{ RIG_MODEL_ICR20, 0x6c },
		{ RIG_MODEL_ICR71, 0x1a },
		{ RIG_MODEL_ICR72, 0x32 },
		{ RIG_MODEL_ICR75, 0x5a },
		{ RIG_MODEL_IC78, 0x62 },
		{ RIG_MODEL_ICR7000, 0x08 },
		{ RIG_MODEL_ICR7100, 0x34 },
		{ RIG_MODEL_ICR8500, 0x4a },
		{ RIG_MODEL_ICR9000, 0x2a },
		{ RIG_MODEL_MINISCOUT, 0x94 },
		{ RIG_MODEL_IC718, 0x5e },
		{ RIG_MODEL_OS535, 0x80 },
		{ RIG_MODEL_ICID1, 0x01 },
		{ RIG_MODEL_NONE, 0 },
};

/*
 * This is a generic icom_init function.
 * You might want to define yours, so you can customize it for your rig
 *
 * Basically, it sets up *priv
 * REM: serial port is already open (rig->state.rigport.fd)
 */
int icom_init(RIG *rig)
{
		struct icom_priv_data *priv;
		const struct icom_priv_caps *priv_caps;
		const struct rig_caps *caps;

		if (!rig || !rig->caps)
				return -RIG_EINVAL;

		caps = rig->caps;

		if (!caps->priv)
				return -RIG_ECONF;

		priv_caps = (const struct icom_priv_caps *) caps->priv;

		priv = (struct icom_priv_data*)malloc(sizeof(struct icom_priv_data));
		if (!priv) {
				/* whoops! memory shortage! */
				return -RIG_ENOMEM;
		}

		rig->state.priv = (void*)priv;

		/* TODO: CI-V address should be customizable */

		/*
		 * init the priv_data from static struct
		 *          + override with preferences
		 */

		priv->re_civ_addr = 0x00;

		priv->re_civ_addr = priv_caps->re_civ_addr;
		priv->civ_731_mode = priv_caps->civ_731_mode;

		return RIG_OK;
}

/*
 * ICOM Generic icom_cleanup routine
 * the serial port is closed by the frontend
 */
int icom_cleanup(RIG *rig)
{
		if (!rig)
				return -RIG_EINVAL;

		if (rig->state.priv)
				free(rig->state.priv);
		rig->state.priv = NULL;

		return RIG_OK;
}


/*
 * icom_set_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char freqbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int freq_len, ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;


		freq_len = priv->civ_731_mode ? 4:5;
		/*
		 * to_bcd requires nibble len
		 */
		to_bcd(freqbuf, freq, freq_len*2);

		retval = icom_transaction (rig, C_SET_FREQ, -1, freqbuf, freq_len,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_freq: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 * Note: old rig may return less than 4/5 bytes for get_freq
 */
int icom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char freqbuf[MAXFRAMELEN];
		int freq_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_FREQ, -1, NULL, 0,
						freqbuf, &freq_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * freqbuf should contain Cn,Data area
		 */
		freq_len--;

		/*
		 * is it a blank mem channel ?
		 */
		if (freq_len == 1 && freqbuf[1] == 0xff) {
			*freq = RIG_FREQ_NONE;

			return RIG_OK;
		}

		if (freq_len != 4 && freq_len != 5) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_freq: wrong frame len=%d\n",
								freq_len);
				return -RIG_ERJCTED;
		}

		if (freq_len != (priv->civ_731_mode ? 4:5)) {
				rig_debug(RIG_DEBUG_WARN,"icom_get_freq: "
						"freq len (%d) differs from "
						"expected\n", freq_len);
		}

		/*
		 * from_bcd requires nibble len
		 */
		*freq = from_bcd(freqbuf+1, freq_len*2);

		return RIG_OK;
}

int icom_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char freqbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int freq_len, ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;


		freq_len = 2;
		/*
		 * to_bcd requires nibble len
		 */
		to_bcd(freqbuf, rit, freq_len*2);

		retval = icom_transaction (rig, C_SET_OFFS, -1, freqbuf, freq_len,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_rit: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}


/*
 * icom_set_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[MAXFRAMELEN];
		unsigned char icmode;
		signed char icmode_ext;
		int ack_len, retval, err;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		err = rig2icom_mode(rig, mode, width, &icmode, &icmode_ext);
		if (err < 0)
				return err;

		/* IC-731 and IC-735 don't support passband data */
		if (priv->civ_731_mode || rig->caps->rig_model == RIG_MODEL_OS456)
				icmode_ext = -1;

		retval = icom_transaction (rig, C_SET_MODE, icmode, &icmode_ext,
						icmode_ext == -1 ? 0 : 1, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_mode: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL, width!=NULL
 *
 * TODO: some IC781's, when sending mode info, in wide filter mode, no
 * 	width data is send along, making the frame 1 byte short.
 * 	(Thank to Mel, VE2DC for this info)
 */
int icom_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char modebuf[MAXFRAMELEN];
		int mode_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_MODE, -1, NULL, 0,
						modebuf, &mode_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * modebuf should contain Cn,Data area
		 */
		mode_len--;
		if (mode_len != 2 && mode_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_mode: wrong frame len=%d\n",
								mode_len);
				return -RIG_ERJCTED;
		}

		icom2rig_mode(rig, modebuf[1], mode_len==2 ? modebuf[2] : -1, mode, width);

		return RIG_OK;
}

/*
 * icom_set_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_vfo(RIG *rig, vfo_t vfo)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, icvfo, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		if (vfo == RIG_VFO_CURR)
			return RIG_OK;

		switch(vfo) {
		case RIG_VFO_A: icvfo = S_VFOA; break;
		case RIG_VFO_B: icvfo = S_VFOB; break;
		case RIG_VFO_MAIN: icvfo = S_MAIN; break;
		case RIG_VFO_SUB:  icvfo = S_SUB; break;
		case RIG_VFO_VFO:
			retval = icom_transaction (rig, C_SET_VFO, -1, NULL, 0,
							ackbuf, &ack_len);
			if (retval != RIG_OK)
				return retval;
			if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_vfo: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
			}
			return RIG_OK;
		case RIG_VFO_MEM:
			retval = icom_transaction (rig, C_SET_MEM, -1, NULL, 0,
							ackbuf, &ack_len);
			if (retval != RIG_OK)
				return retval;
			if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_vfo: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
			}
			return RIG_OK;
		default:
						rig_debug(RIG_DEBUG_ERR,"icom: Unsupported VFO %d\n",
										vfo);
						return -RIG_EINVAL;
		}
		retval = icom_transaction (rig, C_SET_VFO, icvfo, NULL, 0,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_vfo: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char lvlbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int ack_len, lvl_len;
		int lvl_cn, lvl_sc;		/* Command Number, Subcommand */
		int icom_val;
		int i, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;


		/*
		 * So far, levels of float type are in [0.0..1.0] range
		 */
		if (RIG_LEVEL_IS_FLOAT(level))
				icom_val = val.f * 255;
		else
				icom_val = val.i;

		/* convert values to 0 .. 255 range */
		if (rig->caps->rig_model == RIG_MODEL_ICR75) {
			switch (level) {
				case RIG_LEVEL_NR:
					icom_val = val.f * 240;
					break;
				case RIG_LEVEL_PBT_IN:
				case RIG_LEVEL_PBT_OUT:
					icom_val = (val.f / 10.0) + 128;
					if (icom_val > 255)
						icom_val = 255;
					break;
				default:
					break;
			}
		}

		/*
		 * Most of the time, the data field is a 3 digit BCD,
		 * but in *big endian* order: 0000..0255
		 * (from_bcd is little endian)
		 */
		lvl_len = 2;
		to_bcd_be(lvlbuf, (long long)icom_val, lvl_len*2);

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (level) {
		case RIG_LEVEL_PREAMP:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_PAMP;
			lvl_len = 1;
			if (val.i == 0) {
				lvlbuf[0] = 0;	/* 0=OFF */
				break;
			}
			for (i=0; i<MAXDBLSTSIZ; i++) {
					if (rs->preamp[i] == val.i)
							break;
			}
			if (i==MAXDBLSTSIZ || rs->preamp[i] == 0) {
				rig_debug(RIG_DEBUG_ERR,"Unsupported preamp set_level %ddB",
								val.i);
				return -RIG_EINVAL;
			}
			lvlbuf[0] = i+1;	/* 1=P.AMP1, 2=P.AMP2 */
			break;
		case RIG_LEVEL_ATT:
			lvl_cn = C_CTL_ATT;
			/* attenuator level is dB, in BCD mode */
			lvl_sc = (val.i/10)<<4 | (val.i%10);
			lvl_len = 0;
			break;
		case RIG_LEVEL_AF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_AF;
			break;
		case RIG_LEVEL_RF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RF;
			break;
		case RIG_LEVEL_SQL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_SQL;
			break;
		case RIG_LEVEL_IF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_IF;
			break;
		case RIG_LEVEL_APF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_APF;
			break;
		case RIG_LEVEL_NR:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NR;
			break;
		case RIG_LEVEL_PBT_IN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTIN;
			break;
		case RIG_LEVEL_PBT_OUT:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTOUT;
			break;
		case RIG_LEVEL_CWPITCH:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_CWPITCH;
			/* use 'set mode' call for CWPITCH on IC-R75*/
			if (rig->caps->rig_model == RIG_MODEL_ICR75) {
				lvl_cn = C_CTL_MEM;
				lvl_sc = S_MEM_MODE_SLCT;
				lvl_len = 3;
				lvlbuf[0] = S_PRM_CWPITCH;
				to_bcd_be(lvlbuf+1, (long long)icom_val, 4);
			}
			break;
		case RIG_LEVEL_RFPOWER:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RFPOWER;
			break;
		case RIG_LEVEL_MICGAIN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_MICGAIN;
			break;
		case RIG_LEVEL_KEYSPD:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_KEYSPD;
			break;
		case RIG_LEVEL_NOTCHF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NOTCHF;
			break;
		case RIG_LEVEL_COMP:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_COMP;
			break;
		case RIG_LEVEL_AGC:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_AGC;
			if (rig->caps->rig_model == RIG_MODEL_ICR75) {
				lvl_len = 1;
				lvlbuf[0] = val.i;
			}
			break;
		case RIG_LEVEL_BKINDL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BKINDL;
			break;
		case RIG_LEVEL_BALANCE:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BALANCE;
			break;
        case RIG_LEVEL_VOXGAIN:     /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_VOXGAIN;
            break;
        case RIG_LEVEL_VOXDELAY:    /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_VOXDELAY;
            break;
        case RIG_LEVEL_ANTIVOX:     /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_ANTIVOX;
            break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_level %d", level);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, lvl_cn, lvl_sc, lvlbuf, lvl_len,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_level: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int icom_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char lvlbuf[MAXFRAMELEN], lvl2buf[MAXFRAMELEN];
		int lvl_len, lvl2_len;
		int lvl_cn, lvl_sc;		/* Command Number, Subcommand */
		int icom_val;
		int cmdhead;
		int retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		lvl2_len = 0;
		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (level) {
		case RIG_LEVEL_STRENGTH:
		case RIG_LEVEL_RAWSTR:
			lvl_cn = C_RD_SQSM;
			lvl_sc = S_SML;
			break;
		case RIG_LEVEL_PREAMP:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_PAMP;
			break;
		case RIG_LEVEL_ATT:
			lvl_cn = C_CTL_ATT;
			lvl_sc = -1;
			break;
		case RIG_LEVEL_AF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_AF;
			break;
		case RIG_LEVEL_RF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RF;
			break;
		case RIG_LEVEL_SQL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_SQL;
			break;
		case RIG_LEVEL_IF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_IF;
			break;
		case RIG_LEVEL_APF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_APF;
			break;
		case RIG_LEVEL_NR:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NR;
			break;
		case RIG_LEVEL_PBT_IN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTIN;
			break;
		case RIG_LEVEL_PBT_OUT:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTOUT;
			break;
		case RIG_LEVEL_CWPITCH:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_CWPITCH;
			/* use 'set mode' call for CWPITCH on IC-R75*/
			if (rig->caps->rig_model == RIG_MODEL_ICR75) {
				lvl_cn = C_CTL_MEM;
				lvl_sc = S_MEM_MODE_SLCT;
				lvl2_len = 1;
				lvl2buf[0] = S_PRM_CWPITCH;
			}
			break;
		case RIG_LEVEL_RFPOWER:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RFPOWER;
			break;
		case RIG_LEVEL_MICGAIN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_MICGAIN;
			break;
		case RIG_LEVEL_KEYSPD:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_KEYSPD;
			break;
		case RIG_LEVEL_NOTCHF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NOTCHF;
			break;
		case RIG_LEVEL_COMP:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_COMP;
			break;
		case RIG_LEVEL_AGC:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_AGC;
			break;
		case RIG_LEVEL_BKINDL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BKINDL;
			break;
		case RIG_LEVEL_BALANCE:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BALANCE;
			break;
        case RIG_LEVEL_VOXGAIN:     /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_VOXGAIN;
            break;
        case RIG_LEVEL_VOXDELAY:    /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_VOXDELAY;
            break;
        case RIG_LEVEL_ANTIVOX:     /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_ANTIVOX;
            break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_level %d", level);
			return -RIG_EINVAL;
		}

		/* use lvl2buf and lvl2_len for 'set mode' subcommand */
		retval = icom_transaction (rig, lvl_cn, lvl_sc, lvl2buf, lvl2_len,
						lvlbuf, &lvl_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * strbuf should contain Cn,Sc,Data area
		 */
		cmdhead = (lvl_sc == -1) ? 1:2;
		lvl_len -= cmdhead;
		/* back off one char since first char in buffer is now 'set mode' subcommand */
		if ((rig->caps->rig_model == RIG_MODEL_ICR75)&&(level==RIG_LEVEL_CWPITCH)){
			cmdhead = 3;
			lvl_len--;
		}

		if (lvlbuf[0] != ACK && lvlbuf[0] != lvl_cn) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_level: ack NG (%#.2x), "
								"len=%d\n", lvlbuf[0],lvl_len);
				return -RIG_ERJCTED;
		}

		/*
		 * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
		 * (from_bcd is little endian)
		 */
		icom_val = from_bcd_be(lvlbuf+cmdhead, lvl_len*2);

		switch (level) {
		case RIG_LEVEL_RAWSTR:
			val->i = icom_val;
			break;
		case RIG_LEVEL_STRENGTH:
			val->i = (int)rig_raw2val(icom_val,&rig->caps->str_cal);
			break;
		case RIG_LEVEL_PREAMP:
			if (icom_val == 0) {
					val->i = 0;
					break;
			}
			if (icom_val > MAXDBLSTSIZ || rs->preamp[icom_val-1]==0) {
				rig_debug(RIG_DEBUG_ERR,"Unsupported preamp get_level %ddB",
								icom_val);
				return -RIG_EPROTO;
			}
			val->i = rs->preamp[icom_val-1];
			break;
		/* RIG_LEVEL_ATT: returned value is already an integer in dB (coded in BCD) */
		default:
			if (RIG_LEVEL_IS_FLOAT(level))
				val->f = (float)icom_val/255;
			else
				val->i = icom_val;
		}

		/* convert values from 0 .. 255 range */
		if (rig->caps->rig_model == RIG_MODEL_ICR75) {
			switch (level) {
				case RIG_LEVEL_NR:
					val->f = (float)icom_val / 240;
					break;
				case RIG_LEVEL_PBT_IN:
				case RIG_LEVEL_PBT_OUT:
					if (icom_val == 255)
						val->f = 1280.0;
					else
						val->f = (float)(icom_val - 128) * 10.0;
					break;
				default:
					break;
			}
		}

		rig_debug(RIG_DEBUG_TRACE,"icom_get_level: %d %d %d %f\n", lvl_len,
						icom_val, val->i, val->f);

		return RIG_OK;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_conf(RIG *rig, token_t token, const char *val)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		switch(token) {
			case TOK_CIVADDR:
					if (val[0] == '0' && val[1] == 'x')
						priv->re_civ_addr = strtol(val, (char **)NULL, 16);
					else
						priv->re_civ_addr = atoi(val);
					break;
			case TOK_MODE731:
					priv->civ_731_mode = atoi(val) ? 1:0;
					break;
			default:
					return -RIG_EINVAL;
		}
		return RIG_OK;
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int icom_get_conf(RIG *rig, token_t token, char *val)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		switch(token) {
			case TOK_CIVADDR:
					sprintf(val, "%d", priv->re_civ_addr);
					break;
			case TOK_MODE731:
					sprintf(val, "%d", priv->civ_731_mode);
					break;
			default:
					return -RIG_EINVAL;
		}
		return RIG_OK;
}


/*
 * icom_set_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[MAXFRAMELEN], pttbuf[1];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		pttbuf[0] = ptt == RIG_PTT_ON ? 1 : 0;

		retval = icom_transaction (rig, C_CTL_PTT, S_PTT, pttbuf, 1,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ptt: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char pttbuf[MAXFRAMELEN];
		int ptt_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_CTL_PTT, S_PTT, NULL, 0,
						pttbuf, &ptt_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * freqbuf should contain Cn,Data area
		 */
		ptt_len--;
		if (ptt_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ptt: wrong frame len=%d\n",
								ptt_len);
				return -RIG_ERJCTED;
		}

		*ptt = pttbuf[1] == 1 ? RIG_PTT_ON : RIG_PTT_OFF;

		return RIG_OK;
}

/*
 * icom_get_dcd
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char dcdbuf[MAXFRAMELEN];
		int dcd_len, retval;
		int icom_val;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_SQSM, S_SQL, NULL, 0,
						dcdbuf, &dcd_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * freqbuf should contain Cn,Data area
		 */
		dcd_len -= 2;
		if (dcd_len != 2) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_dcd: wrong frame len=%d\n",
								dcd_len);
				return -RIG_ERJCTED;
		}

		/*
		 * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
		 * (from_bcd is little endian)
		 */
		icom_val = from_bcd_be(dcdbuf+2, dcd_len*2);
			/*
			 * 0x00=sql closed, 0x01=sql open
			 *
			 * TODO: replace icom_val by dcdbuf[2] ?
			 */
		*dcd = (icom_val==0x01) ? RIG_DCD_ON : RIG_DCD_OFF;

		return RIG_OK;
}
/*
 * icom_set_rptr_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval;
		int rptr_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		switch (rptr_shift) {
		case RIG_RPT_SHIFT_NONE:
			rptr_sc = S_DUP_OFF;	/* Simplex mode */
			break;
		case RIG_RPT_SHIFT_MINUS:
			rptr_sc = S_DUP_M;		/* Duplex - mode */
			break;
		case RIG_RPT_SHIFT_PLUS:
			rptr_sc = S_DUP_P;		/* Duplex + mode */
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported shift %d", rptr_shift);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, C_CTL_SPLT, rptr_sc, NULL, 0,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_rptr_shift: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}


/*
 * icom_get_rptr_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL, rptr_shift!=NULL
 * NOTE: seems not to work (tested on IC-706MkIIG), please report --SF
 */
int icom_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char rptrbuf[MAXFRAMELEN];
		int rptr_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_CTL_SPLT, -1, NULL, 0,
						rptrbuf, &rptr_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * rptrbuf should contain Cn,Sc
		 */
		rptr_len--;
		if (rptr_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_rptr_shift: wrong frame len=%d\n",
								rptr_len);
				return -RIG_ERJCTED;
		}

		switch (rptrbuf[1]) {
		case S_DUP_OFF:
			*rptr_shift = RIG_RPT_SHIFT_NONE;	/* Simplex mode */
			break;
		case S_DUP_M:
			*rptr_shift = RIG_RPT_SHIFT_MINUS;		/* Duples - mode */
			break;
		case S_DUP_P:
			*rptr_shift = RIG_RPT_SHIFT_PLUS;		/* Duplex + mode */
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported shift %d", rptrbuf[1]);
			return -RIG_EPROTO;
		}

		return RIG_OK;
}

/*
 * icom_set_rptr_offs
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char offsbuf[MAXFRAMELEN],ackbuf[MAXFRAMELEN];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		/*
		 * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF
		 */
		to_bcd(offsbuf, rptr_offs/100, OFFS_LEN*2);

		retval = icom_transaction (rig, C_SET_OFFS, -1, offsbuf, OFFS_LEN,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_rptr_offs: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}


/*
 * icom_get_rptr_offs
 * Assumes rig!=NULL, rig->state.priv!=NULL, rptr_offs!=NULL
 */
int icom_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char offsbuf[MAXFRAMELEN];
		int offs_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_OFFS, -1, NULL, 0,
						offsbuf, &offs_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * offsbuf should contain Cn
		 */
		offs_len--;
		if (offs_len != OFFS_LEN) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_rptr_offs: "
								"wrong frame len=%d\n", offs_len);
				return -RIG_ERJCTED;
		}

		/*
		 * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF
		 */
		*rptr_offs = from_bcd(offsbuf+1, offs_len*2)*100;

		return RIG_OK;
}


/*
 * icom_set_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 * 	icom_set_vfo,icom_set_freq works for this rig
 * FIXME: status
 *
 * Assumes also that the current VFO is the rx VFO.
 */
int icom_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
		int status;

		/* This method works also in memory mode(RIG_VFO_MEM) */
		if (rig_has_vfo_op(rig, RIG_OP_XCHG)) {
			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			status = icom_set_freq(rig, RIG_VFO_CURR, tx_freq);
			if (status != RIG_OK)
				return status;

			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			return 0;
		}

		status = icom_set_vfo(rig, RIG_VFO_B);
		if (status != RIG_OK)
				return status;

		status = icom_set_freq(rig, RIG_VFO_CURR, tx_freq);
		if (status != RIG_OK)
				return status;

		status = icom_set_vfo(rig, RIG_VFO_A);
		if (status != RIG_OK)
				return status;

		return status;
}

/*
 * icom_get_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, rx_freq!=NULL, tx_freq!=NULL
 * 	icom_set_vfo,icom_get_freq works for this rig
 * FIXME: status
 */
int icom_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
		int status;

		/* This method works also in memory mode(RIG_VFO_MEM) */
		if (rig_has_vfo_op(rig, RIG_OP_XCHG)) {
			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			status = icom_get_freq(rig, RIG_VFO_CURR, tx_freq);
			if (status != RIG_OK)
				return status;

			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			return 0;
		}

		status = icom_set_vfo(rig, RIG_VFO_B);
		if (status != RIG_OK)
				return status;

		status = icom_get_freq(rig, RIG_VFO_CURR, tx_freq);
		if (status != RIG_OK)
				return status;

		status = icom_set_vfo(rig, RIG_VFO_A);
		if (status != RIG_OK)
				return status;

		return status;
}

/*
 * icom_set_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 * 	icom_set_vfo,icom_set_mode works for this rig
 * FIXME: status
 */
int icom_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
		int status;

		/* This method works also in memory mode(RIG_VFO_MEM) */
		if (rig_has_vfo_op(rig, RIG_OP_XCHG)) {
			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			status = icom_set_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
			if (status != RIG_OK)
				return status;

			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			return 0;
		}

		status = icom_set_vfo(rig, RIG_VFO_B);
		if (status != RIG_OK)
				return status;

		status = icom_set_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
		if (status != RIG_OK)
				return status;

		status = icom_set_vfo(rig, RIG_VFO_A);
		if (status != RIG_OK)
				return status;

		return status;
}

/*
 * icom_get_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  rx_mode!=NULL, rx_width!=NULL, tx_mode!=NULL, tx_width!=NULL
 * 	icom_set_vfo,icom_get_mode works for this rig
 * FIXME: status
 */
int icom_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
		int status;

		/* This method works also in memory mode(RIG_VFO_MEM) */
		if (rig_has_vfo_op(rig, RIG_OP_XCHG)) {
			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			status = icom_get_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
			if (status != RIG_OK)
				return status;

			status = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
			if (status != RIG_OK)
				return status;

			return 0;
		}

		status = icom_set_vfo(rig, RIG_VFO_B);
		if (status != RIG_OK)
				return status;

		status = icom_get_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
		if (status != RIG_OK)
				return status;

		status = icom_set_vfo(rig, RIG_VFO_A);
		if (status != RIG_OK)
				return status;

		return status;
}


/*
 * icom_set_split
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval;
		int split_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		switch (split) {
		case RIG_SPLIT_OFF:
			split_sc = S_SPLT_OFF;
			break;
		case RIG_SPLIT_ON:
			split_sc = S_SPLT_ON;
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"%s: Unsupported split %d", __FUNCTION__, split);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, C_CTL_SPLT, split_sc, NULL, 0,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_split: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_split_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL, split!=NULL
 */
int icom_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char splitbuf[MAXFRAMELEN];
		int split_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_CTL_SPLT, -1, NULL, 0,
						splitbuf, &split_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * splitbuf should contain Cn,Sc
		 */
		split_len--;
		if (split_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"%s: wrong frame len=%d\n",
						__FUNCTION__, split_len);
				return -RIG_ERJCTED;
		}

		switch (splitbuf[1]) {
		case S_SPLT_OFF:
			*split = RIG_SPLIT_OFF;
			break;
		case S_SPLT_ON:
			*split = RIG_SPLIT_ON;
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported split %d", splitbuf[1]);
			return -RIG_EPROTO;
		}

		return RIG_OK;
}


/*
 * icom_mem_get_split_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL, split!=NULL
 */
int icom_mem_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
		int retval;

		/* this hacks works only when in memory mode
		 * I have no clue how to detect split in regular VFO mode
		 */
		if (rig->state.current_vfo != RIG_VFO_MEM)
			return -RIG_ENAVAIL;

		retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
		if (retval == RIG_OK) {
			*split = RIG_SPLIT_ON;
			/* get it back to normal */
			retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG);
		} else if (retval == -RIG_ERJCTED) {
			*split = RIG_SPLIT_OFF;
		} else {
			/* this is really an error! */
			return retval;
		}

		return RIG_OK;
}

/*
 * icom_set_ts
 * Assumes rig!=NULL, rig->caps->priv!=NULL
 */
int icom_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
		const struct icom_priv_caps *priv_caps;
		unsigned char ackbuf[MAXFRAMELEN];
		int i, ack_len, retval;
		int ts_sc = 0;

		priv_caps = (const struct icom_priv_caps*)rig->caps->priv;

		for (i=0; i<TSLSTSIZ; i++) {
				if (priv_caps->ts_sc_list[i].ts == ts) {
						ts_sc = priv_caps->ts_sc_list[i].sc;
						break;
				}
		}
		if (i >= TSLSTSIZ) {
				return -RIG_EINVAL;	/* not found, unsupported */
		}

		retval = icom_transaction (rig, C_SET_TS, ts_sc, NULL, 0,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ts: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ts
 * Assumes rig!=NULL, rig->caps->priv!=NULL, ts!=NULL
 * NOTE: seems not to work (tested on IC-706MkIIG), please report --SF
 */
int icom_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
		const struct icom_priv_caps *priv_caps;
		unsigned char tsbuf[MAXFRAMELEN];
		int ts_len, i, retval;

		priv_caps = (const struct icom_priv_caps*)rig->caps->priv;

		retval = icom_transaction (rig, C_SET_TS, -1, NULL, 0, tsbuf, &ts_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * tsbuf should contain Cn,Sc
		 */
		ts_len--;
		if (ts_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ts: wrong frame len=%d\n",
								ts_len);
				return -RIG_ERJCTED;
		}

		for (i=0; i<TSLSTSIZ; i++) {
				if (priv_caps->ts_sc_list[i].sc == tsbuf[1]) {
						*ts = priv_caps->ts_sc_list[i].ts;
						break;
				}
		}
		if (i >= TSLSTSIZ) {
				return -RIG_EPROTO;	/* not found, unsupported */
		}

		return RIG_OK;
}


/*
 * icom_set_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
		unsigned char fctbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int fct_len, acklen, retval;
		int fct_cn, fct_sc;		/* Command Number, Subcommand */

		/*
		 * except for IC-R8500
		 */
		fctbuf[0] = status? 0x01:0x00;
		fct_len = rig->caps->rig_model == RIG_MODEL_ICR8500 ? 0 : 1;

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (func) {
		case RIG_FUNC_FAGC:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_AGC;
			/* note: should it be a LEVEL only, and no func? --SF */
			if (status != 0)
				fctbuf[0] = 0x01;	/* default to 0x01 super-fast */
			else
				fctbuf[0] = 0x02;
			break;
		case RIG_FUNC_NB:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NB;
			break;
		case RIG_FUNC_COMP:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_COMP;
			break;
		case RIG_FUNC_VOX:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_VOX;
			break;
		case RIG_FUNC_TONE:		/* repeater tone */
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TONE;
			break;
		case RIG_FUNC_TSQL:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TSQL;
			break;
		case RIG_FUNC_SBKIN:		/* FIXME ? */
		case RIG_FUNC_FBKIN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_BKIN;
			break;
		case RIG_FUNC_ANF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_ANF;
			break;
		case RIG_FUNC_NR:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NR;
			break;
		case RIG_FUNC_APF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_APF;
			break;
		case RIG_FUNC_MON:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MON;
			break;
		case RIG_FUNC_MN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MN;
			break;
		case RIG_FUNC_RNF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_RNF;
			break;
        case RIG_FUNC_AFC:      /* IC-910H */
            fct_cn = C_CTL_FUNC;
            fct_sc = S_FUNC_AFC;
            break;
        case RIG_FUNC_SATMODE:  /* IC-910H */
            fct_cn = C_CTL_MEM;
            fct_sc = S_MEM_SATMODE;
            break;
        case RIG_FUNC_SCOPE:    /* IC-910H */
            fct_cn = C_CTL_MEM;
            fct_sc = S_MEM_BANDSCOPE;
            break;
		case RIG_FUNC_RESUME:	/* IC-910H */
			fct_cn = C_CTL_SCAN;
			fct_sc = status ? S_SCAN_RSMON : S_SCAN_RSMOFF;
			fct_len = 0;
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_func %d", func);
			return -RIG_EINVAL;
		}

		retval = icom_transaction(rig, fct_cn, fct_sc, fctbuf, fct_len,
						ackbuf, &acklen);
		if (retval != RIG_OK)
				return retval;

		if (acklen != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_func: wrong frame len=%d\n",
								acklen);
				return -RIG_EPROTO;
		}

		return RIG_OK;
}

/*
 * icom_get_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 * FIXME: IC8500 and no-sc, any support?
 */
int icom_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval;
		int fct_cn, fct_sc;		/* Command Number, Subcommand */

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (func) {
		case RIG_FUNC_FAGC:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_AGC;	/* default to 0x01 super-fast */
			break;
		case RIG_FUNC_NB:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NB;
			break;
		case RIG_FUNC_COMP:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_COMP;
			break;
		case RIG_FUNC_VOX:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_VOX;
			break;
		case RIG_FUNC_TONE:		/* repeater tone */
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TONE;
			break;
		case RIG_FUNC_TSQL:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TSQL;
			break;
		case RIG_FUNC_SBKIN:		/* FIXME ? */
		case RIG_FUNC_FBKIN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_BKIN;
			break;
		case RIG_FUNC_ANF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_ANF;
			break;
		case RIG_FUNC_NR:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NR;
			break;
		case RIG_FUNC_APF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_APF;
			break;
		case RIG_FUNC_MON:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MON;
			break;
		case RIG_FUNC_MN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MN;
			break;
		case RIG_FUNC_RNF:
            fct_cn = C_CTL_FUNC;
            fct_sc = S_FUNC_RNF;
            break;
        case RIG_FUNC_AFC:      /* IC-910H */
            fct_cn = C_CTL_FUNC;
            fct_sc = S_FUNC_AFC;
            break;
        case RIG_FUNC_SATMODE:  /* IC-910H */
            fct_cn = C_CTL_MEM;
            fct_sc = S_MEM_SATMODE;
            break;
        case RIG_FUNC_SCOPE:    /* IC-910H */
            fct_cn = C_CTL_MEM;
            fct_sc = S_MEM_BANDSCOPE;
            break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_func %d", func);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, fct_cn, fct_sc, NULL, 0,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_func: wrong frame len=%d\n",
								ack_len);
				return -RIG_EPROTO;
		}

		*status = ackbuf[2];

		return RIG_OK;
}

/*
 * icom_set_parm
 * Assumes rig!=NULL
 */
int icom_set_parm(RIG *rig, setting_t parm, value_t val)
{
		unsigned char prmbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int ack_len, prm_len;
		int prm_cn, prm_sc;
		int icom_val;
		int retval;
		int min,hr,sec;

		switch (parm) {
		case RIG_PARM_ANN:
			if ((val.i == RIG_ANN_FREQ) || (val.i == RIG_ANN_RXMODE)) {
				prm_cn = C_CTL_ANN;
				prm_sc = val.i;
				prm_len = 0;
			}
			else {
				if ((val.i == RIG_ANN_ENG)||(val.i == RIG_ANN_JAP)) {
					prm_cn = C_CTL_MEM;
					prm_sc = S_MEM_MODE_SLCT;
					prm_len = 2;
					prmbuf[0] = S_PRM_LANG;
					prmbuf[1] = (val.i == RIG_ANN_ENG ? 0 : 1);
				}
				else {
					rig_debug(RIG_DEBUG_ERR,"Unsupported set_parm_ann %d\n", val.i);
					return -RIG_EINVAL;
				}
			}
			break;
		case RIG_PARM_APO:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			hr = (float)val.i/60.0;
			min = val.i - (hr*60);
			prm_len = 3;
			prmbuf[0] = S_PRM_SLPTM;
			to_bcd_be(prmbuf+1, (long long)hr, 2);
			to_bcd_be(prmbuf+2, (long long)min, 2);
			break;
		case RIG_PARM_BACKLIGHT:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			icom_val = val.f * 255;
			prm_len = 3;
			prmbuf[0] = S_PRM_BACKLT;
			to_bcd_be(prmbuf+1, (long long)icom_val, (prm_len-1)*2);
			break;
		case RIG_PARM_BEEP:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			prm_len = 2;
			prmbuf[0] = S_PRM_BEEP;
			prmbuf[1] = val.i;
			break;
		case RIG_PARM_TIME:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			hr = (float)val.i/3600.0;
			min = (float)(val.i - (hr*3600))/60.0;
			sec = (val.i - (hr*3600) - (min*60));
			prm_len = 4;
			prmbuf[0] = S_PRM_TIME;
			to_bcd_be(prmbuf+1, (long long)hr, 2);
			to_bcd_be(prmbuf+2, (long long)min, 2);
			to_bcd_be(prmbuf+3, (long long)sec, 2);
			break;
		default:
		  rig_debug(RIG_DEBUG_ERR,"Unsupported set_parm %d\n", parm);
		  return -RIG_EINVAL;
		}

		retval = icom_transaction(rig, prm_cn, prm_sc, prmbuf, prm_len,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_parm: wrong frame len=%d\n",
								ack_len);
				return -RIG_EPROTO;
		}

		return RIG_OK;
}

/*
 * icom_get_parm
 * Assumes rig!=NULL
 */
int icom_get_parm(RIG *rig, setting_t parm, value_t *val)
{
		unsigned char prmbuf[MAXFRAMELEN], resbuf[MAXFRAMELEN];
		int prm_len, res_len;
		int prm_cn, prm_sc;
		int icom_val;
		int cmdhead;
		int retval;
		int min,hr,sec;

		switch (parm) {
		case RIG_PARM_APO:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			prm_len = 1;
			prmbuf[0] = S_PRM_SLPTM;
			break;
		case RIG_PARM_BACKLIGHT:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			prm_len = 1;
			prmbuf[0] = S_PRM_BACKLT;
			break;
		case RIG_PARM_BEEP:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			prm_len = 1;
			prmbuf[0] = S_PRM_BEEP;
			break;
		case RIG_PARM_TIME:
			prm_cn = C_CTL_MEM;
			prm_sc = S_MEM_MODE_SLCT;
			prm_len = 1;
			prmbuf[0] = S_PRM_TIME;
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_parm %d", parm);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, prm_cn, prm_sc, prmbuf, prm_len,
						resbuf, &res_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * strbuf should contain Cn,Sc,[pn],Data area
		 */
		cmdhead = (prm_sc == -1) ? 1:3;
		res_len -= cmdhead;

		if (resbuf[0] != ACK && resbuf[0] != prm_cn) {
				rig_debug(RIG_DEBUG_ERR,"%s: ack NG (%#.2x), "
							"len=%d\n", __FUNCTION__,resbuf[0],res_len);
				return -RIG_ERJCTED;
		}

		switch (parm) {
		case RIG_PARM_APO:
			hr = from_bcd_be(resbuf+cmdhead, 2);
			min = from_bcd_be(resbuf+cmdhead+1, 2);
			icom_val = (hr*60)+min;
			val->i = icom_val;
			break;
		case RIG_PARM_TIME:
			hr = from_bcd_be(resbuf+cmdhead, 2);
			min = from_bcd_be(resbuf+cmdhead+1, 2);
			sec = from_bcd_be(resbuf+cmdhead+2, 2);
			icom_val = (hr*3600)+(min*60)+sec;
			val->i = icom_val;
			break;
		default:
			icom_val = from_bcd_be(resbuf+cmdhead, res_len*2);
			if (RIG_PARM_IS_FLOAT(parm))
				val->f = (float)icom_val/255;
			else
				val->i = icom_val;
		}

		rig_debug(RIG_DEBUG_TRACE,"%s: %d %d %d %f\n",
				__FUNCTION__, res_len, icom_val, val->i, val->f);

		return RIG_OK;
}

/*
 * icom_set_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Warning! This is untested stuff! May work at least on 756PRO and IC746.
 * 	Please owners report to me <f4cfe@users.sourceforge.net>, thanks. --SF
 */
int icom_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int tone_len, ack_len, retval;
		int i;

		caps = rig->caps;

		/*
		 * I don't have documentation for this function,
		 * and I can't experiment (no hardware), so let's guess.
		 * Most probably, it might be the index of the CTCSS subaudible
		 * tone, and not the tone itself, starting from zero.
		 *
		 * Something in the range of 00..51, BCD big endian
		 * Please someone let me know if it works this way. --SF
		 */
		for (i = 0; caps->ctcss_list[i] != 0 && i<200; i++) {
				if (caps->ctcss_list[i] == tone)
						break;
		}
		if (caps->ctcss_list[i] != tone)
				return -RIG_EINVAL;

		tone_len = 1;
		to_bcd_be(tonebuf, (long long)i, tone_len*2);

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR,
						tonebuf, tone_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ctcss_tone: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[MAXFRAMELEN];
		int tone_len, tone_idx, retval;
		int i;

		caps = rig->caps;

		/*
		 * see icom_set_ctcss for discussion on the untested status!
		 */

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR, NULL, 0,
												tonebuf, &tone_len);
		if (retval != RIG_OK)
				return retval;

		if (tone_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss_tone: ack NG (%#.2x), "
								"len=%d\n", tonebuf[0], tone_len);
				return -RIG_ERJCTED;
		}

		tone_len -= 2;
		tone_idx = from_bcd_be(tonebuf, tone_len*2);

		/* check this tone exists. That's better than nothing. */
		for (i = 0; i<=tone_idx; i++) {
				if (caps->ctcss_list[i] == 0) {
						rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss_tone: CTCSS NG "
								"(%#.2x)\n", tonebuf[2]);
						return -RIG_EPROTO;
				}
		}
		*tone = caps->ctcss_list[tone_idx];

		return RIG_OK;
}

/*
 * icom_set_ctcss_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Warning! This is untested stuff! May work at least on 756PRO and IC746.
 * 	Please owners report to me <f4cfe@users.sourceforge.net>, thanks. --SF
 */
int icom_set_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int tone_len, ack_len, retval;
		int i;

		caps = rig->caps;

		/*
		 * see icom_set_ctcss for discussion on the untested status!
		 */

		for (i = 0; caps->ctcss_list[i] != 0 && i<200; i++) {
				if (caps->ctcss_list[i] == tone)
						break;
		}
		if (caps->ctcss_list[i] != tone)
				return -RIG_EINVAL;

		tone_len = 1;
		to_bcd_be(tonebuf, (long long)i, tone_len*2);

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL,
						tonebuf, tone_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ctcss_sql: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ctcss_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int *tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[MAXFRAMELEN];
		int tone_len, tone_idx, retval;
		int i;

		caps = rig->caps;

		/*
		 * see icom_set_ctcss for discussion on the untested status!
		 */

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL, NULL, 0,
												tonebuf, &tone_len);
		if (retval != RIG_OK)
				return retval;

		if (tone_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss_sql: ack NG (%#.2x), "
								"len=%d\n", tonebuf[0], tone_len);
				return -RIG_ERJCTED;
		}

		tone_len -= 2;
		tone_idx = from_bcd_be(tonebuf, tone_len*2);

		/* check this tone exists. That's better than nothing. */
		for (i = 0; i<=tone_idx; i++) {
				if (caps->ctcss_list[i] == 0) {
						rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss_sql: CTCSS NG "
								"(%#.2x)\n", tonebuf[2]);
						return -RIG_EPROTO;
				}
		}
		*tone = caps->ctcss_list[tone_idx];

		return RIG_OK;
}


/*
 * icom_set_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_powerstat(RIG *rig, powerstat_t status)
{
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval;
		int pwr_sc;

		pwr_sc = status==RIG_POWER_ON ? S_PWR_ON:S_PWR_OFF;

		retval = icom_transaction(rig, C_SET_PWR, pwr_sc, NULL, 0,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_powerstat: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_powerstat(RIG *rig, powerstat_t *status)
{
		unsigned char cmdbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int cmd_len, ack_len, retval;

		/* r75 has no way to get power status, so fake it */
		if (rig->caps->rig_model == RIG_MODEL_ICR75) {
			/* getting the mode doesn't work if a memory is blank */
			/* so use one of the more innculous 'set mode' commands instead */
			cmd_len = 1;
			cmdbuf[0] = S_PRM_TIME;
			retval = icom_transaction(rig, C_CTL_MEM, S_MEM_MODE_SLCT, 
									cmdbuf, cmd_len, ackbuf, &ack_len);
			if (retval != RIG_OK)
				return retval;

			*status = ((ack_len == 6)&&(ackbuf[0] == C_CTL_MEM)) ?
						RIG_POWER_ON : RIG_POWER_OFF;
		}
		else {
			retval = icom_transaction(rig, C_SET_PWR, -1, NULL, 0,
							ackbuf, &ack_len);
			if (retval != RIG_OK)
					return retval;

			if (ack_len != 1 || ackbuf[0] != ACK) {
					rig_debug(RIG_DEBUG_ERR,"icom_get_powerstat: ack NG (%#.2x), "
									"len=%d\n", ackbuf[0],ack_len);
					return -RIG_ERJCTED;
			}
			*status = ackbuf[1] == S_PWR_ON ? RIG_POWER_ON : RIG_POWER_OFF;
		}
		return RIG_OK;
}


/*
 * icom_set_mem
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char membuf[2];
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval;
		int chan_len;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		chan_len = ch < 100 ? 1 : 2;

		to_bcd_be(membuf, ch, chan_len*2);
		retval = icom_transaction (rig, C_SET_MEM, -1, membuf, chan_len,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_mem: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_set_bank
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_bank(RIG *rig, vfo_t vfo, int bank)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char bankbuf[2];
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		to_bcd_be(bankbuf, bank, BANK_NB_LEN*2);
		retval = icom_transaction (rig, C_SET_MEM, S_BANK,
						bankbuf, CHAN_NB_LEN, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_bank: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_set_ant
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_ant(RIG * rig, vfo_t vfo, ant_t ant)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char antarg;
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval, i_ant;
		int ant_len;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		/*
		 * FIXME: IC-756*, IC-746*
		 */
		i_ant = ant == RIG_ANT_1 ? 0: 1;

		antarg = 0;
		ant_len = (rig->caps->rig_model == RIG_MODEL_ICR75) ? 0 : 1;
		retval = icom_transaction (rig, C_CTL_ANT, i_ant,
						&antarg, ant_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ant: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ant
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
		unsigned char ackbuf[MAXFRAMELEN];
		int ack_len, retval;

		retval = icom_transaction(rig, C_CTL_ANT, -1, NULL, 0,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (rig->caps->rig_model == RIG_MODEL_ICR75) {
			if (ack_len != 2 || ackbuf[0] != C_CTL_ANT) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ant: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
			}
		}
		else
		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ant: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		/*
		 * FIXME: IC-756*, IC-746*
		 */
		*ant = ackbuf[1] == 0 ? RIG_ANT_1 : RIG_ANT_2;

		return RIG_OK;
}


/*
 * icom_vfo_op, Mem/VFO operation
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char mvbuf[MAXFRAMELEN];
		unsigned char ackbuf[MAXFRAMELEN];
		int mv_len, ack_len, retval;
		int mv_cn, mv_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		mv_len = 0;

		switch(op) {
			case RIG_OP_CPY:
				mv_cn = C_SET_VFO;
				mv_sc = S_BTOA;
				break;
			case RIG_OP_XCHG:
				mv_cn = C_SET_VFO;
				mv_sc = S_XCHNG;
				break;
#if 0
			case RIG_OP_DUAL_OFF:
				mv_cn = C_SET_VFO;
				mv_sc = S_DUAL_OFF;
				break;
			case RIG_OP_DUAL_ON:
				mv_cn = C_SET_VFO;
				mv_sc = S_DUAL_ON;
				break;
#endif
			case RIG_OP_FROM_VFO:
				mv_cn = C_WR_MEM;
				mv_sc = -1;
				break;
			case RIG_OP_TO_VFO:
				mv_cn = C_MEM2VFO;
				mv_sc = -1;
				break;
			case RIG_OP_MCL:
				mv_cn = C_CLR_MEM;
				mv_sc = -1;
				break;
			default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported mem/vfo op %#x", op);
				return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, mv_cn, mv_sc, mvbuf, mv_len,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_vfo_op: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_scan, scan operation
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char scanbuf[MAXFRAMELEN];
		unsigned char ackbuf[MAXFRAMELEN];
		int scan_len, ack_len, retval;
		int scan_cn, scan_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		scan_len = 0;
		scan_cn = C_CTL_SCAN;

		switch(scan) {
			case RIG_SCAN_STOP:
				scan_sc = S_SCAN_STOP;
				break;
			case RIG_SCAN_MEM:
				retval = icom_set_vfo(rig, RIG_VFO_MEM);
				if (retval != RIG_OK)
						return retval;
				scan_sc = S_SCAN_START;
				break;
			case RIG_SCAN_SLCT:
				retval = icom_set_vfo(rig, RIG_VFO_MEM);
				if (retval != RIG_OK)
						return retval;
				scan_sc = S_SCAN_START;
				break;
			case RIG_SCAN_PRIO:
			case RIG_SCAN_PROG:
				/* TODO: for SCAN_PROG, check this is an edge chan */
				/* BTW, I'm wondering if this is possible with CI-V */
				retval = icom_set_mem(rig, RIG_VFO_CURR, ch);
				if (retval != RIG_OK)
						return retval;
				retval = icom_set_vfo(rig, RIG_VFO_VFO);
				if (retval != RIG_OK)
						return retval;
				scan_sc = S_SCAN_START;
				break;
			case RIG_SCAN_DELTA:
				scan_sc = S_SCAN_DELTA;	/* TODO: delta-f support */
				break;
			default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported scan %#x", scan);
				return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, scan_cn, scan_sc, scanbuf, scan_len,
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_scan: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_decode is called by sa_sigio, when some asynchronous
 * data has been received from the rig
 */
int icom_decode_event(RIG *rig)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char buf[MAXFRAMELEN];
		int frm_len;
		freq_t freq;
		rmode_t mode;
		pbwidth_t width;

		rig_debug(RIG_DEBUG_VERBOSE, "icom: icom_decode called\n");

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		frm_len = read_icom_frame(&rs->rigport, buf);

		if (frm_len == -RIG_ETIMEOUT)
		     rig_debug(RIG_DEBUG_VERBOSE, "icom: icom_decode got a timeout before the first character\n");

		if (frm_len < 0)
				return frm_len;

		switch (buf[frm_len-1])
		  {
		  case COL:
		    rig_debug(RIG_DEBUG_VERBOSE, "icom: icom_decode saw a collision\n");
		    /* Collision */
		    return -RIG_BUSBUSY;
		  case FI:
		    /* Ok, normal frame */
		    break;
		  default:
		    /* Timeout after reading at least one character */
		    /* Problem on ci-v bus? */
		    return  -RIG_EPROTO;
		  }

		if (buf[3] != BCASTID && buf[3] != priv->re_civ_addr) {
			rig_debug(RIG_DEBUG_WARN, "icom_decode: CI-V %#x called for %#x!\n",
							priv->re_civ_addr, buf[3]);
		}

		/*
		 * the first 2 bytes must be 0xfe
		 * the 3rd one the emitter
		 * the 4rd one 0x00 since this is transceive mode
		 * then the command number
		 * the rest is data
		 * and don't forget one byte at the end for the EOM
		 */
		switch (buf[4]) {
		case C_SND_FREQ:
				/*
				 * TODO: the freq length might be less than 4 or 5 bytes
				 * 			on older rigs!
				 */
				if (rig->callbacks.freq_event) {
					freq = from_bcd(buf+5, (priv->civ_731_mode ? 4:5)*2);
					return rig->callbacks.freq_event(rig, RIG_VFO_CURR, freq,
									rig->callbacks.freq_arg);
				} else
						return -RIG_ENAVAIL;
				break;
		case C_SND_MODE:
				if (rig->callbacks.mode_event) {
					icom2rig_mode(rig, buf[5], buf[6], &mode, &width);
					return rig->callbacks.mode_event(rig, RIG_VFO_CURR,
									mode, width,
									rig->callbacks.mode_arg);
				} else
						return -RIG_ENAVAIL;
				break;
		default:
				rig_debug(RIG_DEBUG_VERBOSE,"icom_decode: tranceive cmd "
									"unsupported %#2.2x\n",buf[4]);
				return -RIG_ENIMPL;
		}

		return RIG_OK;
}

/*
 * init_icom is called by rig_probe_all (register.c)
 *
 * probe_icom reports all the devices on the CI-V bus.
 *
 * rig_model_t probeallrigs_icom(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(icom)
{
	unsigned char buf[MAXFRAMELEN], civ_addr, civ_id;
	int frm_len, i;
	int retval;
	rig_model_t model = RIG_MODEL_NONE;
	int rates[] = { 19200, 9600, 300, 0 };
	int rates_idx;

	if (!port)
		return RIG_MODEL_NONE;

	if (port->type.rig != RIG_PORT_SERIAL)
		return RIG_MODEL_NONE;

	port->write_delay = port->post_write_delay = 0;
	port->retry = 1;

	/*
	 * try for all different baud rates
	 */
	for (rates_idx = 0; rates[rates_idx]; rates_idx++) {
		port->parm.serial.rate = rates[rates_idx];
		port->timeout = 2*1000/rates[rates_idx] + 40;

		retval = serial_open(port);
		if (retval != RIG_OK)
			return RIG_MODEL_NONE;

		/*
		 * try all possible addresses on the CI-V bus
		 * FIXME: actualy, old rigs do not support C_RD_TRXID cmd!
		 * 		Try to be smart, and deduce model depending
		 * 		on freq range, return address, and
		 * 		available commands.
		 */
		for (civ_addr=0x01; civ_addr<=0x7f; civ_addr++) {

			frm_len = make_cmd_frame(buf, civ_addr, C_RD_TRXID, S_RD_TRXID,
							NULL, 0);

			serial_flush(port);
			write_block(port, buf, frm_len);

			/* read out the bytes we just sent
		 	* TODO: check this is what we expect
		 	*/
			frm_len = read_icom_frame(port, buf);

			/* this is the reply */
			frm_len = read_icom_frame(port, buf);

			/* timeout.. nobody's there */
			if (frm_len <= 0)
					continue;

			if (buf[7] != FI && buf[5] != FI) {
					/* protocol error, unexpected reply.
					 * is this a CI-V device?
					 */
					close(port->fd);
					return RIG_MODEL_NONE;
			} else if (buf[4] == NAK) {
					/*
				 	* this is an Icom, but it does not support transceiver ID
				 	* try to guess from the return address
				 	*/
					civ_id = buf[3];
			} else {
					civ_id = buf[6];
			}

			for (i=0; icom_addr_list[i].model != RIG_MODEL_NONE; i++) {
					if (icom_addr_list[i].re_civ_addr == civ_id) {
							rig_debug(RIG_DEBUG_VERBOSE,"probe_icom: found %#x"
											" at %#x\n", civ_id, buf[3]);
							model = icom_addr_list[i].model;
							if (cfunc)
								(*cfunc)(port, model, data);
							break;
					}
			}
			/*
			 * not found in known table....
			 * update icom_addr_list[]!
			 */
			if (icom_addr_list[i].model == RIG_MODEL_NONE)
				rig_debug(RIG_DEBUG_WARN,"probe_icom: found unknown device "
							"with CI-V ID %#x, please report to Hamlib "
							"developers.\n", civ_id);
		}

		/*
		 * Try to identify OptoScan
		 */
		for (civ_addr=0x80; civ_addr<=0x8f; civ_addr++) {

			frm_len = make_cmd_frame(buf, civ_addr, C_CTL_MISC, S_OPTO_RDID,
							NULL, 0);

			serial_flush(port);
			write_block(port, buf, frm_len);

			/* read out the bytes we just sent
		 	* TODO: check this is what we expect
		 	*/
			frm_len = read_icom_frame(port, buf);

			/* this is the reply */
			frm_len = read_icom_frame(port, buf);

			/* timeout.. nobody's there */
			if (frm_len <= 0)
					continue;

			/* wrong protocol? */
			if (frm_len != 7 || buf[4] != C_CTL_MISC || buf[5] != S_OPTO_RDID) {
				continue;
			}

			rig_debug(RIG_DEBUG_VERBOSE, "%s, found OptoScan%c%c%c, software version %d.%d, "
					"interface version %d.%d, at %#x\n",
					__FUNCTION__,
					buf[2], buf[3], buf[4],
					buf[5] >> 4, buf[5] & 0xf,
					buf[6] >> 4, buf[6] & 0xf,
					civ_addr);

			if (buf[6] == '5' && buf[7] == '3' && buf[8] == '5')
				model = RIG_MODEL_OS535;
			else if (buf[6] == '4' && buf[7] == '5' && buf[8] == '6')
				model = RIG_MODEL_OS456;
			else
				continue;

			if (cfunc)
				(*cfunc)(port, model, data);
			break;
		}

		close(port->fd);

		/*
		 * Assumes all the rigs on the bus are running at same speed.
		 * So if one at least has been found, none will be at lower speed.
		 */
		if (model != RIG_MODEL_NONE)
			return model;
	}

	return model;
}

/*
 * initrigs_icom is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(icom)
{
	rig_debug(RIG_DEBUG_VERBOSE, "icom: _init called\n");

	rig_register(&ic703_caps);
	rig_register(&ic706_caps);
	rig_register(&ic706mkii_caps);
	rig_register(&ic706mkiig_caps);
	rig_register(&ic718_caps);
	rig_register(&ic725_caps);
	rig_register(&ic726_caps);
	rig_register(&ic735_caps);
	rig_register(&ic736_caps);
	rig_register(&ic737_caps);
	rig_register(&ic746_caps);
	rig_register(&ic746pro_caps);
	rig_register(&ic751_caps);
	rig_register(&ic761_caps);
	rig_register(&ic775_caps);
	rig_register(&ic756_caps);
	rig_register(&ic756pro_caps);
	rig_register(&ic756pro2_caps);
	rig_register(&ic756pro3_caps);
	rig_register(&ic765_caps);
	rig_register(&ic78_caps);
	rig_register(&ic7800_caps);
	rig_register(&ic781_caps);
	rig_register(&ic707_caps);
	rig_register(&ic728_caps);

	rig_register(&ic821h_caps);
	rig_register(&ic910_caps);
	rig_register(&ic970_caps);

        rig_register(&icr10_caps);
        rig_register(&icr20_caps);
        rig_register(&icr71_caps);
        rig_register(&icr72_caps);
        rig_register(&icr75_caps);
        rig_register(&icr7000_caps);
        rig_register(&icr7100_caps);
	rig_register(&icr8500_caps);
	rig_register(&icr9000_caps);

	rig_register(&ic271_caps);
	rig_register(&ic275_caps);
	rig_register(&ic471_caps);
	rig_register(&ic475_caps);

	rig_register(&os535_caps);
	rig_register(&os456_caps);

	rig_register(&omnivip_caps);

	rig_register(&id1_caps);

	return RIG_OK;
}

