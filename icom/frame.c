/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * frame.c - Copyright (C) 2000 Stephane Fillod
 * This module provides a set of low level functions to handle the
 * CI-V interface, used in serial communication to ICOM radios.
 *
 *
 * $Id: frame.c,v 1.5 2000-11-01 23:21:47 f4cfe Exp $  
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

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <serial.h>
#include <misc.h>
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

/* Prototypes */
int read_icom_block(FILE *stream, unsigned char *rxbuffer, size_t count, int timeout);

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
		int frm_len;

		rig_s = &rig->state;
		priv = (struct icom_priv_data*)rig_s->priv;

		frm_len = make_cmd_frame(buf, priv->re_civ_addr, cmd, subcmd, 
						payload, payload_len);

		/* 
		 * should check return code and that write wrote cmd_len chars! 
		 */
		write_block(rig_s->fd, buf, frm_len, rig_s->write_delay, rig_s->post_write_delay);

		/*
		 * read what we just sent, because TX and RX are looped,
		 * and discard it...
		 * TODO: - if what we read is not what we sent, then it means
		 * 			a collision on the CI-V bus occured!
		 * 		- if we get a timeout, then retry to send the frame,
		 * 			up to rig_s->retry times.
		 */

		Hold_Decode(rig);
		read_icom_block(rig_s->stream, buf, frm_len, rig_s->timeout);

		/*
		 * wait for ACK ... 
		 * FIXME: handle pading/collisions
		 * ACKFRMLEN is the smallest frame we can expect from the rig
		 */
		frm_len = read_icom_frame(rig_s->stream, buf, rig_s->timeout);
		Unhold_Decode(rig);

		*data_len = frm_len-(ACKFRMLEN-1);
		memcpy(data, buf+4, *data_len);

		/*
		 * TODO: check addresses in reply frame
		 */

		return RIG_OK;
}

/*
 * read count useful bytes, discarding collisions
 * FIXME: check return codes/bytes read
 * this function will be deprecated soon!
 */
int read_icom_block(FILE *stream, unsigned char *rxbuffer, size_t count, int timeout)
{
		int i;

		fread_block(stream, rxbuffer, count, timeout);

#if 0
		for (i=0; i<count; i++) {
			if (rxbuffer[i] != COL)
				break;
		}
		if (i > 0) {
			memmove(rxbuffer, rxbuffer+i, count-i);
			fread_block(stream, rxbuffer+i, count-i, timeout);
		}
#endif
		return count;	/* duh! */
}

/*
 * read_icom_frame
 * read a whole CI-V frame (until 0xfd is encountered)
 * TODO: strips padding/collisions
 * FIXME: check return codes/bytes read
 */
int read_icom_frame(FILE *stream, unsigned char rxbuffer[], int timeout)
{
		int i;

		/*
		 * ACKFRMLEN is supposed to be the smallest frame
		 * we can expected on the CI-V bus
		 * FIXME: a COL is smaller!!
		 */
		fread_block(stream, rxbuffer, ACKFRMLEN, timeout);

		/*
		 * buffered read are quite helpful here!
		 * However, an automate with a state model would be more efficient..
		 */
		for (i=ACKFRMLEN; rxbuffer[i-1]!=FI; i++) {
			fread_block(stream, rxbuffer+i, 1, timeout);
		}

		return i;
}


unsigned short hamlib2icom_mode(rmode_t mode)
{
		int icmode, icmode_ext;

		icmode_ext = 0;

		switch (mode) {
		case RIG_MODE_AM:	icmode = S_AM; break;
		case RIG_MODE_CW:	icmode = S_CW; break;
		case RIG_MODE_USB:	icmode = S_USB; break;
		case RIG_MODE_LSB:	icmode = S_LSB; break;
		case RIG_MODE_RTTY:	icmode = S_RTTY; break;
		case RIG_MODE_FM:	icmode = S_FM; break;
#if 0
		case RIG_MODE_WFM:	icmode = S_WFM; break;

		case RIG_MODE_NFM:	icmode = S_FM; icmode_ext = 0x01; break;
		case RIG_MODE_NAM:	icmode = S_AM; break;
		case RIG_MODE_WAM:	icmode = S_AM; break;
		case RIG_MODE_NCW:	icmode = S_CW; break;
		case RIG_MODE_WCW:	icmode = S_CW; break;
		case RIG_MODE_CWR:	icmode = S_CW; break;
		case RIG_MODE_NUSB:	icmode = S_USB; break;
		case RIG_MODE_WUSB:	icmode = S_USB; break;
		case RIG_MODE_NLSB:	icmode = S_LSB; break;
		case RIG_MODE_WLSB:	icmode = S_LSB; break;
		case RIG_MODE_NRTTY: icmode = S_RTTY; break;
		case RIG_MODE_WRTTY: icmode = S_RTTY; break;
#endif

		default:
			rig_debug(RIG_DEBUG_ERR,"icom: Unsupported Hamlib mode %d\n",mode);
			icmode = 0xff;
		}
		return (icmode_ext<<8 | icmode);
}

rmode_t icom2hamlib_mode(unsigned short icmode)
{
		rmode_t mode;

		switch (icmode & 0xff) {
		case S_AM:	mode = RIG_MODE_AM; break;
		case S_CW:	mode = RIG_MODE_CW; break;
		case S_FM:	mode = RIG_MODE_FM; break;
		case S_WFM:	mode = RIG_MODE_FM; break;	/* and set width to wide.. */
		case S_USB:	mode = RIG_MODE_USB; break;
		case S_LSB:	mode = RIG_MODE_LSB; break;
		case S_RTTY:	mode = RIG_MODE_RTTY; break;
		case 0xff:	mode = 0; break;	/* blank mem channel */

		default:
			rig_debug(RIG_DEBUG_ERR,"icom: Unsupported Icom mode %#.2x\n",
							icmode);
			mode = 0;
		}
		return mode;
}

