/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icom.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the "CI-V" interface.
 *
 *
 * $Id: icom.c,v 1.5 2000-10-08 21:38:45 f4cfe Exp $  
 *
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <serial.h>
#include <misc.h>
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


struct icom_addr {
		rig_model_t model;
		unsigned char re_civ_addr;
};

/*
 * Please, if the default CI-V address of your rig is listed as UNKNOWN_ADDR,
 * send the value to <f4cfe@users.sourceforge.net> for inclusion. Thanks --SF
 *
 * TODO: sort this list with most frequent rigs first.
 */
#define UNKNOWN_ADDR 0x01
static const struct icom_addr icom_addr_list[] = {
		{ RIG_MODEL_IC706, 0x48 },
		{ RIG_MODEL_IC706MKII, 0x4e },
		{ RIG_MODEL_IC706MKIIG, 0x58 },
		{ RIG_MODEL_IC1271, 0x24 },
		{ RIG_MODEL_IC1275, 0x18 },
		{ RIG_MODEL_IC271, 0x20 },
		{ RIG_MODEL_IC275, 0x10 },
		{ RIG_MODEL_IC375, 0x12 },
		{ RIG_MODEL_IC471, 0x22 },
		{ RIG_MODEL_IC475, 0x14 },
		{ RIG_MODEL_IC575, 0x16 },
		{ RIG_MODEL_IC725, 0x28 },
		{ RIG_MODEL_IC726, 0x30 },
		{ RIG_MODEL_IC731, 0x04 },
		{ RIG_MODEL_IC735, 0x04 },
		{ RIG_MODEL_IC751, 0x1c },
		{ RIG_MODEL_IC751A, 0x1c },
		{ RIG_MODEL_IC756, 0x50 },
		{ RIG_MODEL_IC761, 0x1e },
		{ RIG_MODEL_IC765, 0x2c },
		{ RIG_MODEL_IC775, 0x46 },
		{ RIG_MODEL_IC781, 0x26 },
		{ RIG_MODEL_IC821, 0x4c },
		{ RIG_MODEL_IC821H, 0x4c },
		{ RIG_MODEL_IC970, 0x2e },
		{ RIG_MODEL_ICR7000, 0x08 },
		{ RIG_MODEL_ICR71, 0x1a },
		{ RIG_MODEL_ICR7100, 0x34 },
		{ RIG_MODEL_ICR72, 0x32 },
		{ RIG_MODEL_ICR8500, 0x4a },
		{ RIG_MODEL_ICR9000, 0x2a },
		{ RIG_MODEL_IC707, UNKNOWN_ADDR },
		{ RIG_MODEL_IC718, UNKNOWN_ADDR },
		{ RIG_MODEL_IC728, UNKNOWN_ADDR },
		{ RIG_MODEL_IC729, UNKNOWN_ADDR },
		{ RIG_MODEL_IC731, UNKNOWN_ADDR },
		{ RIG_MODEL_IC736, UNKNOWN_ADDR },
		{ RIG_MODEL_IC737, UNKNOWN_ADDR },
		{ RIG_MODEL_IC738, UNKNOWN_ADDR },
		{ RIG_MODEL_IC746, UNKNOWN_ADDR },
		{ RIG_MODEL_IC756PRO, UNKNOWN_ADDR },
		{ RIG_MODEL_IC820, UNKNOWN_ADDR },
		{ -1, 0 },
};

/*
 * This is a generic icom_init function.
 * You might want to define yours, so you can customize it for your rig
 *
 * Basically, it sets up *priv 
 * REM: serial port is already open (rig->state->fd)
 */
int icom_init(RIG *rig)
{
		struct icom_priv_data *priv;
		int i;

		if (!rig)
				return -RIG_EINVAL;

		priv = (struct icom_priv_data*)malloc(sizeof(struct icom_priv_data));
		if (!priv) {
				/* whoops! memory shortage! */
				return -RIG_ENOMEM;
		}
		/* TODO: CI-V address should be customizable */

		/*
		 * init the priv_data from static struct 
		 *          + override with preferences
		 */

		priv->re_civ_addr = 0x00;
		for (i=0; icom_addr_list[i].model >= 0; i++) {
				if (icom_addr_list[i].model == rig->caps->rig_model) {
						priv->re_civ_addr = icom_addr_list[i].re_civ_addr;
						break;
				}
		}

		if (rig->caps->rig_model == RIG_MODEL_IC731 ||
						rig->caps->rig_model == RIG_MODEL_IC735)
			priv->civ_731_mode = 1;
		else
			priv->civ_731_mode = 0;


		rig->state.priv = (void*)priv;

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
int icom_set_freq(RIG *rig, freq_t freq)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char freqbuf[16], ackbuf[16];
		int freq_len,ack_len;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;


		freq_len = priv->civ_731_mode ? 4:5;
		/*	
		 * to_bcd requires nibble len
		 */
		to_bcd(freqbuf, freq, freq_len*2);

		icom_transaction (rig, C_SET_FREQ, -1, freqbuf, freq_len, ackbuf, &ack_len);

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_freq: ack NG (%#.2x),
								len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
int icom_get_freq(RIG *rig, freq_t *freq)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char freqbuf[16];
		int freq_len;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		icom_transaction (rig, C_RD_FREQ, -1, NULL, 0, freqbuf, &freq_len);

		/*
		 * freqbuf should contain Cn,Data area
		 */
		freq_len--;
		if (freq_len != (priv->civ_731_mode ? 4:5)) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_freq: wrong frame len=%d\n",
								freq_len);
				return -RIG_ERJCTED;
		}

		/*	
		 * from_bcd requires nibble len
		 */
		*freq = from_bcd(freqbuf+1, freq_len*2);

		return RIG_OK;
}

/*
 * icom_set_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_mode(RIG *rig, rmode_t mode)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char ackbuf[16];
		int ack_len,icmode;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		icmode = hamlib2icom_mode(mode);

		icom_transaction (rig, C_SET_MODE, icmode, NULL, 0, ackbuf, &ack_len);

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_mode: ack NG (%#.2x),
								len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL
 */
int icom_get_mode(RIG *rig, rmode_t *mode)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char modebuf[16];
		int mode_len;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		icom_transaction (rig, C_RD_MODE, -1, NULL, 0, modebuf, &mode_len);

		/*
		 * modebuf should contain Cn,Data area
		 */
		mode_len--;
		if (mode_len != 2 && mode_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_mode: wrong frame len=%d\n",
								mode_len);
				return -RIG_ERJCTED;
		}

		*mode = icom2hamlib_mode(modebuf[1]| modebuf[2]<<8);

		return RIG_OK;
}

/*
 * icom_set_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_vfo(RIG *rig, vfo_t vfo)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char ackbuf[16];
		int ack_len,icvfo;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		switch(vfo) {
		case RIG_VFO_A: icvfo = S_VFOA; break;
		case RIG_VFO_B: icvfo = S_VFOB; break;
		default:
						rig_debug(RIG_DEBUG_ERR,"icom: Unsupported VFO %d\n",
										vfo);
						return -RIG_EINVAL;
		}
		icom_transaction (rig, C_SET_VFO, icvfo, NULL, 0, ackbuf, &ack_len);

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_vfo: ack NG (%#.2x),
								len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_strength
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
int icom_get_strength(RIG *rig, int *strength)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char strbuf[16];
		int str_len;
		float str;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		icom_transaction (rig, C_RD_SQSM, S_SML, NULL, 0, strbuf, &str_len);

		/*
		 * strbuf should contain Cn,Sc,Data area
		 */
		str_len-=2;
		if (str_len != 2) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_strength: wrong frame len=%d\n",
								str_len);
				return -RIG_EPROTO;
		}

		/*	
		 * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
		 * (from_bcd is little endian)
		 */
		str = (strbuf[2]&0x0f)*100+ (strbuf[3]>>4)*10 + (strbuf[3]&0x0f);
#ifndef DONT_WANT_STR_DB
		/* translate it to dBs */
		str = (str-47)*114/(223-47);
#endif
		*strength = rint(str);

		return RIG_OK;
}

/*
 * icom_decode is called by sa_sigio, when some asynchronous
 * data has been received from the rig
 */
int icom_decode_event(RIG *rig)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char buf[32];
		int frm_len;
		freq_t freq;
		rmode_t mode;

		rig_debug(RIG_DEBUG_VERBOSE, "icom: icom_decode called\n");

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		frm_len = read_icom_frame(rig_s->stream, buf, rig_s->timeout);
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
				/* TODO: check the read freq len is 4 or 5 bytes */
				if (rig->callbacks.freq_event) {
					freq = from_bcd(buf+5, (priv->civ_731_mode ? 4:5)*2);
					return rig->callbacks.freq_event(rig,freq);
				} else
						return -RIG_ENAVAIL;
				break;
		case C_SND_MODE:
				if (rig->callbacks.mode_event) {
					mode = icom2hamlib_mode(buf[5]| buf[6]<<8);
					return rig->callbacks.mode_event(rig,mode);
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
 * init_icom is called by rig_backend_load
 */
int init_icom(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "icom: _init called\n");

		rig_register(&ic706_caps);
		rig_register(&ic706mkii_caps);
		rig_register(&ic706mkiig_caps);

		return RIG_OK;
}

