/*
 *  Hamlib CI-V backend - OptoScan extensions
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: optoscan.c,v 1.5 2003-04-09 06:43:54 fillods Exp $
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
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "token.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


/*
 * optoscan_open
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_open(RIG *rig)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		/* select REMOTE control */
		retval = icom_transaction (rig, C_CTL_MISC, S_OPTO_REMOTE, 
						NULL, 0, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"optoscan_open: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * optoscan_close
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_close(RIG *rig)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		/* select LOCAL control */
		retval = icom_transaction (rig, C_CTL_MISC, S_OPTO_LOCAL, 
						NULL, 0, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"optoscan_close: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * optoscan_get_info
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
const char* optoscan_get_info(RIG *rig)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16];
		int ack_len, retval;
		static char info[64];

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		/* select LOCAL control */
		retval = icom_transaction (rig, C_CTL_MISC, S_OPTO_RDID, 
						NULL, 0, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return NULL;

		if (ack_len != 7) {
				rig_debug(RIG_DEBUG_ERR,"optoscan_get_info: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return NULL;
		}

		sprintf(info, "OptoScan%c%c%c, software version %d.%d, "
						"interface version %d.%d\n",
						ackbuf[2], ackbuf[3], ackbuf[4],
						ackbuf[5] >> 4, ackbuf[5] & 0xf, 
						ackbuf[6] >> 4, ackbuf[6] & 0xf);

		return info;
}

/*
 * optoscan_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[MAXFRAMELEN];
		int tone_len, retval;

		caps = rig->caps;

		retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDCTCSS, NULL, 0,
												tonebuf, &tone_len);
		if (retval != RIG_OK)
				return retval;

		if (tone_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"optoscan_get_ctcss_tone: ack NG (%#.2x), "
								"len=%d\n", tonebuf[0], tone_len);
				return -RIG_ERJCTED;
		}

		tone_len -= 2;

		*tone = from_bcd_be(tonebuf+2, tone_len*2);
		rig_debug(RIG_DEBUG_ERR,"optoscan_get_ctcss_tone: *tone=%d\n",*tone);

		return RIG_OK;
}


/*
 * optoscan_get_dcs_code
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_get_dcs_code(RIG * rig, vfo_t vfo, tone_t *code)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[MAXFRAMELEN];
		int tone_len, retval;

		caps = rig->caps;

		retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDDCS, NULL, 0,
												tonebuf, &tone_len);
		if (retval != RIG_OK)
				return retval;

		if (tone_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"optoscan_get_dcs_code: ack NG (%#.2x), "
								"len=%d\n", tonebuf[0], tone_len);
				return -RIG_ERJCTED;
		}

		tone_len -= 2;

		*code = from_bcd_be(tonebuf+2, tone_len*2);
		rig_debug(RIG_DEBUG_ERR,"optoscan_get_dcs_code: *code=%d\n",*code);

        return RIG_OK;
}

int optoscan_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
		const struct rig_caps *caps;
		unsigned char dtmfbuf[MAXFRAMELEN],digit;
		int len, retval, digitpos;
		unsigned char xlate[] = {'0','1','2','3','4','5','6',
					 '7','8','9','A','B','C','D',
					 '*','#'};
		caps = rig->caps;
		digitpos=0;
	       
		do {
		  retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDDTMF, 
					    NULL,0,dtmfbuf, &len);
		  if (retval != RIG_OK)
		    return retval;
		  
		  if (len != 3) {
		    rig_debug(RIG_DEBUG_ERR,"optoscan_recv_dtmf: ack NG (%#.2x), len=%d\n", dtmfbuf[0], len);
		    return -RIG_ERJCTED;
		  }
		  
		  digit = dtmfbuf[2];
		  
		  if( digit < 0x16 )
		    {
		      digits[digitpos] = xlate[digit];
		      digitpos++;
		    }
		} while( (digit != 0x99) && (digitpos < *length) );
		  
		*length = digitpos;
		digits[digitpos]=0;

		if(*length > 0)
		  {
		    rig_debug(RIG_DEBUG_ERR,"optoscan_recv_dtmf: %d digits - %s\n",*length,digits);
		  }
		else
		  {
		    rig_debug(RIG_DEBUG_ERR,"optoscan_recv_dtmf: no digits to read.\n");
		  }

        return RIG_OK;
}
