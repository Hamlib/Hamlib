/*
 *  Hamlib CI-V backend - low level communication routines
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *		$Id: frame.c,v 1.15 2002-03-05 00:39:30 fillods Exp $
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
int read_icom_block(port_t *p, unsigned char *rxbuffer, size_t count);

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
 *
 * return RIG_OK if transaction completed, 
 * or a negative value otherwise indicating the error.
 */
int icom_transaction (RIG *rig, int cmd, int subcmd, const char *payload, int payload_len, char *data, int *data_len)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char buf[16];
		int frm_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		frm_len = make_cmd_frame(buf, priv->re_civ_addr, cmd, subcmd, 
						payload, payload_len);

		/* 
		 * should check return code and that write wrote cmd_len chars! 
		 */
		Hold_Decode(rig);

		serial_flush(&rs->rigport);

		retval = write_block(&rs->rigport, buf, frm_len);
		if (retval != RIG_OK) {
				Unhold_Decode(rig);
				return retval;
		}

		/*
		 * read what we just sent, because TX and RX are looped,
		 * and discard it...
		 * TODO: - if what we read is not what we sent, then it means
		 * 			a collision on the CI-V bus occured!
		 * 		- if we get a timeout, then retry to send the frame,
		 * 			up to rs->retry times.
		 */

		retval = read_icom_block(&rs->rigport, buf, frm_len);
		if (retval != frm_len) {
				Unhold_Decode(rig);
				return retval < 0 ? retval : -RIG_EPROTO;
		}

		/*
		 * wait for ACK ... 
		 * FIXME: handle pading/collisions
		 * ACKFRMLEN is the smallest frame we can expect from the rig
		 */
		frm_len = read_icom_frame(&rs->rigport, buf);
		Unhold_Decode(rig);
		if (frm_len < 0) {
				return frm_len;
		} else if (frm_len < ACKFRMLEN) {
				return  -RIG_EPROTO;
		}

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
int read_icom_block(port_t *p, unsigned char *rxbuffer, size_t count)
{
		int i;

		i = fread_block(p, rxbuffer, count);

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
		return i;	/* duh! */
}

/*
 * read_icom_frame
 * read a whole CI-V frame (until 0xfd is encountered)
 * TODO: strips padding/collisions
 * FIXME: check return codes/bytes read
 */
int read_icom_frame(port_t *p, unsigned char rxbuffer[])
{
		int i, count;

		/*
		 * ACKFRMLEN is supposed to be the smallest frame
		 * we can expected on the CI-V bus
		 * FIXME: a COLlision is smaller!!
		 */
		count = fread_block(p, rxbuffer, ACKFRMLEN);
		if (count != ACKFRMLEN)
				return count;

		/*
		 * buffered read are quite helpful here!
		 * However, an automate with a state model would be more efficient..
		 */
		for (i=ACKFRMLEN; rxbuffer[i-1]!=FI; i++) {
			count = fread_block(p, rxbuffer+i, 1);
			if (count != 1)
					return i;
		}

		return i;
}


/*
 * TODO: be more exhaustive
 * assumes rig!=NULL
 */
unsigned short rig2icom_mode(RIG *rig, rmode_t mode, pbwidth_t width)
{
		int icmode, icmode_ext;
		pbwidth_t norm_width;

		icmode_ext = 0;

		switch (mode) {
		case RIG_MODE_AM:	icmode = S_AM; break;
		case RIG_MODE_CW:	icmode = S_CW; break;
		case RIG_MODE_USB:	icmode = S_USB; break;
		case RIG_MODE_LSB:	icmode = S_LSB; break;
		case RIG_MODE_RTTY:	icmode = S_RTTY; break;
		case RIG_MODE_FM:	icmode = S_FM; break;
		case RIG_MODE_WFM:	icmode = S_WFM; break;
		default:
			rig_debug(RIG_DEBUG_ERR,"icom: Unsupported Hamlib mode %d\n",mode);
			icmode = 0xff;
		}

		norm_width = rig_passband_normal(rig, mode);
		if (width == norm_width || width == RIG_PASSBAND_NORMAL)
				icmode_ext = 0x00;
		else if (width < norm_width)
				icmode_ext = 0x02;
		else
				icmode_ext = 0x01;

		if (rig->caps->rig_model == RIG_MODEL_ICR7000) {
				if (mode == RIG_MODE_USB || mode == RIG_MODE_LSB) {
						icmode = S_AM;
						icmode_ext = 0x00;
				} else if (mode == RIG_MODE_AM && icmode_ext == 0x00) {
						icmode_ext = 0x01;	/* default to Wide */
				}
		}
		
		return (icmode_ext<<8 | icmode);
}

/*
 * assumes rig!=NULL, mode!=NULL, width!=NULL
 */
void icom2rig_mode(RIG *rig, unsigned short icmode, rmode_t *mode, pbwidth_t *width)
{
		*width = RIG_PASSBAND_NORMAL;

		switch (icmode & 0xff) {
		case S_AM:	*mode = RIG_MODE_AM; break;
		case S_CW:	*mode = RIG_MODE_CW; break;
		case S_FM:	if (rig->caps->rig_model == RIG_MODEL_ICR7000
									&& ((icmode>>8) & 0xff) == 0x00) {
							*mode = RIG_MODE_USB;
							*width = rig_passband_normal(rig, *mode);
							return;
					} else
							*mode = RIG_MODE_FM;
					break;
		case S_WFM:	*mode = RIG_MODE_WFM; break;
		case S_USB:	*mode = RIG_MODE_USB; break;
		case S_LSB:	*mode = RIG_MODE_LSB; break;
		case S_RTTY:	*mode = RIG_MODE_RTTY; break;
		case 0xff:	*mode = RIG_MODE_NONE; break;	/* blank mem channel */

		default:
			rig_debug(RIG_DEBUG_ERR,"icom: Unsupported Icom mode %#.2x\n",
							icmode);
			*mode = RIG_MODE_NONE;
		}
		switch ((icmode>>8) & 0xff) {
			case 0x00: *width = rig_passband_narrow(rig, *mode); break;
			case 0x01: *width = rig_passband_normal(rig, *mode); break;
			case 0x02: *width = rig_passband_wide(rig, *mode); break;
		default:
			rig_debug(RIG_DEBUG_ERR,"icom: Unsupported Icom mode width %#.2x\n",
							icmode);
			*width = RIG_PASSBAND_NORMAL;
		}
}

