/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * frame.c - Copyright (C) 2000 Stephane Fillod
 * This module provides a set of low level functions to handle the
 * CI-V interface, used in serial communication to ICOM radios.
 *
 *
 * $Id: frame.c,v 1.1 2000-10-01 12:31:20 f4cfe Exp $  
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

/*
 * Build a CI-V frame.
 * The whole frame is placed in frame[],
 * "re_id" is the transceiver's CI-V address,
 * "cmd" is the Command number,
 * "subcmd" is the Sub command number, set to -1 if not present in frame,
 * if the frame has no data, then the "data" pointer must be NULL,
 * 		and data_len==0.
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

	if (data_len != 0) {
		memcpy(frame+i, data, data_len);
		i += data_len;
	}

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

	freq_len = ic731_mode ? 4:5;
	to_bcd(frame+i, freq, freq_len*2);	/* to_bcd requires nibble len */

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

	to_bcd(frame+i, (freq_t)chan, 4);	/* to_bcd requires nibble len */

	i += 2;		/* channel number is on 4 digits = 2 octets */
	frame[i++] = FI;		/* EOM code */

	return i;
}

/*
 * icom_transaction
 * 
 * We assume that rig!=NULL, rig->state!= NULL, payload!=NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * payload can be NULL if payload_len == 0
 * subcmd can be equal to -1 (no subcmd wanted)
 */
int icom_transaction (RIG *rig, int cmd, int subcmd, const char *payload, int payload_len, char *data, int *data_len)
{
		struct icom_priv_data *priv;
		struct rig_state *rig_s;
		unsigned char buf[16];
		int frm_len,pad_len;
		int i;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		frm_len = make_cmd_frame(buf, priv->re_civ_addr, cmd, subcmd, 
						payload, payload_len);

		/* 
		 * should check return code and that write wrote cmd_len chars! 
		 */
		write_block(rig_s->fd, buf, frm_len, rig_s->write_delay);

		/*
		 * read what we just sent, because TX and RX are looped,
		 * and discard it...
		 * TODO: - if what we read is not what we sent, then it means
		 * 			a collision on the CI-V bus occured!
		 * 		- if we get a timeout, then retry to send the frame,
		 * 			up to rig_s->retry times.
		 */
		read_icom_block(rig_s->fd, buf, frm_len, rig_s->timeout);

		/*
		 * wait for ACK ... 
		 * FIXME: handle pading/collisions
		 */
		read_icom_block(rig_s->fd, buf, ACKFRMLEN, rig_s->timeout);

		pad_len = 0;
		while (buf[pad_len] == PAD)
				pad_len++;

		/*
		 * Even if the return code is NG, make sure the whole frame
		 * has been removed from buffers.
		 */
		for (i=ACKFRMLEN; buf[i-1] != FI; i++) {
			read_icom_block(rig_s->fd, buf+i, 1, rig_s->timeout);
		}

		*data_len = i-pad_len-(ACKFRMLEN-1);
		memcpy(data, buf+pad_len+4, *data_len);

		/*
		 * TODO: check addresses in reply frame
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


