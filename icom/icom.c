/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icom.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the "CI-V" interface.
 *
 *
 * $Id: icom.c,v 1.4 2000-10-01 12:37:10 f4cfe Exp $  
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

#include <rig.h>
#include <riglist.h>
#include <serial.h>
#include <misc.h>
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

/* Prototypes */
int read_icom_block(int fd, unsigned char *rxbuffer, size_t count, int timeout);

struct icom_addr {
		rig_model_t model;
		unsigned char re_civ_addr;
};

static const struct icom_addr icom_addr_list[] = {
		{ RIG_MODEL_IC706, 0x58 },
		{ RIG_MODEL_IC706MKII, 0x58 },
		{ RIG_MODEL_IC706MKIIG, 0x58 },
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

		if (rig->caps->rig_model == RIG_MODEL_IC731)
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
		 * to_bcd requires nibble len
		 */
		*freq = from_bcd(freqbuf+1, freq_len*2);

		return RIG_OK;
}

