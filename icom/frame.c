/*
 *  Hamlib CI-V backend - low level communication routines
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: frame.c,v 1.21 2003-04-26 09:54:49 fillods Exp $
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
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

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

#if 0
	frame[i++] = PAD;	/* give old rigs a chance to flush their rx buffers */
#endif
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
		unsigned char buf[MAXFRAMELEN];
		unsigned char sendbuf[MAXFRAMELEN];
		int frm_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		frm_len = make_cmd_frame(sendbuf, priv->re_civ_addr, cmd, subcmd, 
						payload, payload_len);

		/* 
		 * should check return code and that write wrote cmd_len chars! 
		 */
		Hold_Decode(rig);

		serial_flush(&rs->rigport);

		retval = write_block(&rs->rigport, sendbuf, frm_len);
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

		retval = read_icom_frame(&rs->rigport, buf);
		if (retval == -RIG_ETIMEOUT || retval == 0)
		  {
		    /* Nothing recieved, CI-V interface is not echoing */
		    Unhold_Decode(rig);
		    return -RIG_BUSERROR;
		  }
		if (retval < 0)
		  {
		    /* Other error, return it */
		    Unhold_Decode(rig);
		    return retval;
		  }

		switch (buf[retval-1])
		  {
		  case COL:
		    /* Collision */
		    Unhold_Decode(rig);
		    return -RIG_BUSBUSY;
		  case FI:
		    /* Ok, normal frame */
		    break;
		  default:
		    /* Timeout after reading at least one character */
		    /* Problem on ci-v bus? */
		    Unhold_Decode(rig);
		    return -RIG_BUSERROR;
		  }

		if (retval != frm_len) 
		  {
		    /* Not the same length??? */
		    /* Problem on ci-v bus? */
		    /* Someone else got a packet in? */
		    Unhold_Decode(rig);
		    return -RIG_EPROTO;
		  }
		if (memcmp(buf,sendbuf,frm_len))
		  {
		    /* Frames are different? */
		    /* Problem on ci-v bus? */
		    /* Someone else got a packet in? */
		    Unhold_Decode(rig);
		    return -RIG_EPROTO;
		  }

		/*
		 * wait for ACK ... 
		 * FIXME: handle pading/collisions
		 * ACKFRMLEN is the smallest frame we can expect from the rig
		 */
		frm_len = read_icom_frame(&rs->rigport, buf);
		Unhold_Decode(rig);
		
		if (frm_len < 0) 
		  {
		    /* RIG_TIMEOUT: timeout getting response, return timeout */
		    /* other error: return it */
		    return frm_len;
		  }
		
		switch (buf[frm_len-1])
		  {
		  case COL:
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

		if (frm_len < ACKFRMLEN) {
		  return  -RIG_EPROTO;
		}

		*data_len = frm_len-(ACKFRMLEN-1);
		memcpy(data, buf+4, *data_len);

		/*
		 * TODO: check addresses in reply frame
		 */
		
		return RIG_OK;
}

/* used in read_icom_frame as end of block */
static const char icom_block_end[2] = {FI, COL};
#define icom_block_end_length 2

/*
 * read_icom_frame
 * read a whole CI-V frame (until 0xfd is encountered)
 * TODO: strips padding/collisions
 * FIXME: check return codes/bytes read
 */
int read_icom_frame(port_t *p, unsigned char rxbuffer[])
{
		int i;

		i = read_string(p, rxbuffer, MAXFRAMELEN, 
						icom_block_end, icom_block_end_length);

		return i;
}


/*
 * convert mode and width as expressed by Hamlib frontend
 * to mode and passband data understandable by a CI-V rig
 *
 * if pd == -1, no passband data is to be sent
 *
 * return RIG_OK if everything's fine, negative value otherwise
 *
 * TODO: be more exhaustive
 * assumes rig!=NULL
 */
int rig2icom_mode(RIG *rig, rmode_t mode, pbwidth_t width, 
				unsigned char *md, char *pd)
{
		unsigned char icmode;
		signed char icmode_ext;
		pbwidth_t medium_width;

		icmode_ext = -1;

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
			return -RIG_EINVAL;
		}

		medium_width = rig_passband_normal(rig, mode);
		if (width == medium_width || width == RIG_PASSBAND_NORMAL)
				icmode_ext = -1;	/* medium, no passband data */
		else if (width < medium_width)
				icmode_ext = PD_NARROW;
		else
				icmode_ext = PD_WIDE;

		if (rig->caps->rig_model == RIG_MODEL_ICR7000) {
				if (mode == RIG_MODE_USB || mode == RIG_MODE_LSB) {
						icmode = S_R7000_SSB;
						icmode_ext = 0x00;
				} else if (mode == RIG_MODE_AM && icmode_ext == -1) {
						icmode_ext = PD_WIDE;	/* default to Wide */
				}
		}
		
		*md = icmode;
		*pd = icmode_ext;
		return RIG_OK;
}

/*
 * assumes rig!=NULL, mode!=NULL, width!=NULL
 */
void icom2rig_mode(RIG *rig, unsigned char md, int pd, rmode_t *mode, pbwidth_t *width)
{
		*width = RIG_PASSBAND_NORMAL;

		switch (md) {
		case S_AM:	*mode = RIG_MODE_AM; break;
		case S_CW:	*mode = RIG_MODE_CW; break;
		case S_FM:	if (rig->caps->rig_model == RIG_MODEL_ICR7000
									&& pd == 0x00) {
							*mode = RIG_MODE_USB;
							*width = rig_passband_normal(rig, RIG_MODE_USB);
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
							md);
			*mode = RIG_MODE_NONE;
		}
		switch (pd) {
			case 0x00: *width = rig_passband_narrow(rig, *mode); break;
			case 0x01: *width = rig_passband_normal(rig, *mode); break;
			case 0x02: *width = rig_passband_wide(rig, *mode); break;
			case -1: break;		/* no passband data */
		default:
			rig_debug(RIG_DEBUG_ERR,"icom: Unsupported Icom mode width %#.2x\n",
							pd);
			*width = RIG_PASSBAND_NORMAL;
		}
}

