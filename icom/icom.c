/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icom.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the "CI-V" interface.
 *
 *
 * $Id: icom.c,v 1.3 2000-09-21 06:46:13 f4cfe Exp $  
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
#include "serial.h"
#include "icom.h"
#include "icom_defs.h"

/* Prototypes */
unsigned char *freq2bcd(unsigned char bcd_data[], freq_t freq, int bcd_len);
freq_t bcd2freq(const unsigned char bcd_data[], int bcd_len);
int read_icom_block(int fd, unsigned char *rxbuffer, size_t count, int timeout);

struct icom_addr {
		rig_model_t model;
		unsigned char re_civ_addr;
};

static const struct icom_addr icom_addr_list[] = {
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
		struct icom_priv_data *p;
		int i;

		if (!rig)
				return -RIG_EINVAL;

		p = (struct icom_priv_data*)malloc(sizeof(struct icom_priv_data));
		if (!p) {
				/* whoops! memory shortage! */
				return -RIG_ENOMEM;
		}
		/* TODO: CI-V address should be customizable */

		/* init the priv_data from static struct 
		 *          + override with preferences
		 */

		p->re_civ_addr = 0x00;
		for (i=0; icom_addr_list[i].model >= 0; i++) {
				if (icom_addr_list[i].model == rig->caps->rig_model) {
						p->re_civ_addr = icom_addr_list[i].re_civ_addr;
						break;
				}
		}

		if (rig->caps->rig_model == RIG_MODEL_IC731)
			p->civ_731_mode = 1;
		else
			p->civ_731_mode = 0;


		rig->state.priv = (void*)p;

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
 * Build a CI-V frame.
 * The whole frame is placed in frame[],
 * "re_id" is the transceiver's CI-V address,
 * "cmd" is the Command number,
 * "subcmd" is the Sub command number, set to -1 if not present in frame,
 * if the frame has no data, then the "data" pointer must be NULL.
 * "data_len" holds the Data area length pointed by the "data" pointer.
 * REM: if "data" is NULL, then "data_len" MUST be 0.
 *
 * NB: the frame array must be big enough to hold the frame.
 * 		The smallest frame is 8 bytes, the biggest is at least 13 bytes.
 *
 * TODO: inline the function?
 */
int make_cmd_frame(char frame[], char re_id, char cmd, int subcmd, const char *data, int data_len)
{	
	int i = 0;

	frame[i++] = PAD;	/* give old rigs a chance to flush their rx buffers */
	frame[i++] = PR;	/* Preamble code */
	frame[i++] = PR;
	frame[i++] = re_id;
	frame[i++] = CTRLID;
	frame[i++] = cmd;
	if (subcmd != -1)
			frame[i++] = subcmd & 0xff;

	if (data) {
		memcpy(frame+7, data, data_len);
	}

	i += data_len;
	frame[i++] = FI;		/* EOM code */

	return i;
}

int make_cmd_frame_freq(char frame[], char re_id, char cmd, int subcmd, freq_t freq, int ic731_mode)
{
	int freq_len;
	int i = 0;

	frame[i++] = PAD;	/* give old rigs a chance to flush their rx buffers */
	frame[i++] = PR;	/* Preamble code */
	frame[i++] = PR;
	frame[i++] = re_id;
	frame[i++] = CTRLID;
	frame[i++] = cmd;
	if (subcmd != -1)
			frame[i++] = subcmd & 0xff;

	freq_len = ic731_mode ? 5:4;
	freq2bcd(frame+i, freq, freq_len*2);	/* freq2bcd requires nibble len */

	i += freq_len;
	frame[i++] = FI;		/* EOM code */

	return i;
}

/*
 * for C_SET_MEM, subcmd=-1
 */
int 
make_cmd_frame_chan(char frame[], char re_id, char cmd, int subcmd, int chan)
{
	int i = 0;

	frame[i++] = PAD;	/* give old rigs a chance to flush their rx buffers */
	frame[i++] = PR;	/* Preamble code */
	frame[i++] = PR;
	frame[i++] = re_id;
	frame[i++] = CTRLID;
	frame[i++] = cmd;
	if (subcmd != -1)
			frame[i++] = subcmd & 0xff;

	freq2bcd(frame+i, (freq_t)chan, 4);	/* freq2bcd requires nibble len */

	i += 2;		/* channel number is on 4 digits = 2 octets */
	frame[i++] = FI;		/* EOM code */

	return i;
}


/*
 * TODO: implement (ie. rewrite it) this function properly,
 * 		right now it just serves as a backend example
 */
int icom_set_freq(RIG *rig, freq_t freq)
{
		struct icom_priv_data *p;
		struct rig_state *rig_s;
		unsigned char buf[16];
		int frm_len;

		if (!rig)
				return -RIG_EINVAL;

		p = (struct icom_priv_data*)rig->state.priv;
		rig_s = &rig->state;

		frm_len = make_cmd_frame_freq(buf, p->re_civ_addr, C_SET_FREQ, -1, 
						freq, p->civ_731_mode);

		/* 
		 * should check return code and that write wrote cmd_len chars! 
		 */
		write_block(rig_s->fd, buf, frm_len, rig_s->write_delay);

		/*
		 * wait for ACK ... etc.. 
		 */
		read_icom_block(rig_s->fd, buf, ACKFRMLEN, rig_s->timeout);

		if (buf[5] != ACK) {
				return -RIG_ERJCTED;
		}

		/*
		 * TODO: check address, Command number and ack!
		 */

		return RIG_OK;
}


/*
 * read count usefull bytes, discarding collisions
 * FIXME: check return codes/bytes read
 */
int read_icom_block(int fd, unsigned char *rxbuffer, size_t count, int timeout)
{
		int i;

		read_block(fd, rxbuffer, count, timeout);

		for (i=0; i<count; i++) {
			if (rxbuffer[i] != COL)
				break;
		}
		if (i > 0) {
			memmove(rxbuffer, rxbuffer+i, count-i);
			read_block(fd, rxbuffer+i, count-i, timeout);
		}
		return count;	/* duh! */
}



/*
 * Convert an int (ie. long long frequency in Hz) to 4-bit BCD digits, 
 * packed two digits per octet, in little-endian order.
 * bcd_len is the number of BCD digits, usually 10 or 8 in 1-Hz units, 
 *	and 6 digits in 100-Hz units for Tx offset data.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/ the 64bit freq)
 */
unsigned char *
freq2bcd(unsigned char bcd_data[], freq_t freq, int bcd_len)
{
	int i;
	unsigned char a;

	/* '450'-> 0,5;4,0 */

	for (i=0; i < bcd_len/2; i++) {
			a = freq%10;
			freq /= 10;
			a |= (freq%10)<<4;
			freq /= 10;
			bcd_data[i] = a;
	}
	if (bcd_len&1)
			bcd_data[i] |= freq%10;	/* NB: high nibble is not cleared */

	return bcd_data;
}

/*
 * Convert BCD digits to an int (ie. long long frequency in Hz)
 * bcd_len is the number of BCD digits.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/ the 64bit freq)
 */
freq_t bcd2freq(const unsigned char bcd_data[], int bcd_len)
{
	int i;
	freq_t f = 0;

	if (bcd_len&1)
			f = bcd_data[bcd_len/2] & 0x0f;

	for (i=(bcd_len/2)-1; i >= 0; i--) {
			f *= 10;
			f += bcd_data[i]>>4;
			f *= 10;
			f += bcd_data[i] & 0x0f;
	}
	
	return f;
}


