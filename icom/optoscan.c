/*
 *  Hamlib CI-V backend - OptoScan extensions
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: optoscan.c,v 1.3 2003-04-07 22:41:51 fillods Exp $
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


