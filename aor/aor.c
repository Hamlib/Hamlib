/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * aor.c - Copyright (C) 2000,2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an AOR scanner.
 *
 *
 * $Id: aor.c,v 1.11 2001-06-20 06:08:21 f4cfe Exp $  
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

#if defined(__CYGWIN__)
#  undef HAMLIB_DLL
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#endif

#include "aor.h"



/*
 * acknowledge is CR
 * Is \r portable enough?
 */
#define CR '\r'

/*
 * modes in use by the "MD" command
 */
#define MD_WFM	'0'
#define MD_NFM	'1'
#define	MD_AM	'2'
#define MD_USB	'3'
#define MD_LSB	'4'
#define MD_CW	'5'
#define MD_SFM	'6'
#define MD_WAM	'7'
#define MD_NAM	'8'


/*
 * aor_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling
 */
int aor_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int i, retval;
	struct rig_state *rs;

	rs = &rig->state;

	retval = write_block(&rs->rigport, cmd, cmd_len);
	if (retval != RIG_OK)
			return retval;

	retval = write_block(&rs->rigport, "\n", 1);
	if (retval != RIG_OK)
			return retval;

	/*
	 * buffered read are quite helpful here!
	 * However, an automate with a state model would be more efficient..
	 */
	i = 0;
	do {
		retval = fread_block(&rs->rigport, data+i, 1);
		if (retval == 0)
				continue;	/* huh!? */
		if (retval != RIG_OK)
			return retval;
	} while (data[i++] != CR);

	*data_len = i;

	return RIG_OK;
}

/*
 * aor_close
 * Assumes rig!=NULL
 */
int aor_close(RIG *rig)
{
		unsigned char ackbuf[16];
		int ack_len;

		/* terminate remote operation via the RS-232 */

		return aor_transaction (rig, "EX", 2, ackbuf, &ack_len);
}


/*
 * aor_set_freq
 * Assumes rig!=NULL
 */
int aor_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		unsigned char freqbuf[16], ackbuf[16];
		int freq_len, ack_len, retval;

		/*
		 * actually, frequency must be like nnnnnnnnm0, 
		 * where m must be 0 or 5 (for 50Hz).
		 */
		freq_len = sprintf(freqbuf,"RF%010Ld", freq);

		retval = aor_transaction (rig, freqbuf, freq_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

		return RIG_OK;
}

/*
 * aor_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int aor_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		unsigned char freqbuf[16];
		char *rfp;
		int freq_len, retval;

		retval = aor_transaction (rig, "RX", 2, freqbuf, &freq_len);
		if (retval != RIG_OK)
			return retval;

		rfp = strstr(freqbuf, "RF");
		sscanf(rfp+2,"%Ld", freq);

		return RIG_OK;
}

/*
 * aor_set_mode
 * Assumes rig!=NULL
 */
int aor_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		unsigned char mdbuf[16],ackbuf[16];
		int mdbuf_len, ack_len, aormode, retval;

		switch (mode) {
			case RIG_MODE_AM:       
					switch(width) {
						case RIG_PASSBAND_NORMAL:
						case kHz(9): aormode = MD_AM; break;

						case kHz(12): aormode = MD_WAM; break;
						case kHz(3): aormode = MD_NAM; break;
						default:
							rig_debug(RIG_DEBUG_ERR,
								"aor_set_mode: unsupported passband %d %d\n",
								mode, width);
						return -RIG_EINVAL;
					}
					break;
			case RIG_MODE_CW:       aormode = MD_CW; break;
			case RIG_MODE_USB:      aormode = MD_USB; break;
			case RIG_MODE_LSB:      aormode = MD_LSB; break;
			case RIG_MODE_WFM:      aormode = MD_WFM; break;
			case RIG_MODE_FM:
					switch(width) {
						case RIG_PASSBAND_NORMAL:
						case kHz(12): aormode = MD_NFM; break;

						case kHz(9): aormode = MD_SFM; break;
						default:
							rig_debug(RIG_DEBUG_ERR,
								"aor_set_mode: unsupported passband %d %d\n",
								mode, width);
						return -RIG_EINVAL;
					}
					break;
			default:
				rig_debug(RIG_DEBUG_ERR,"aor_set_mode: unsupported mode %d\n",
								mode);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf, "MD%c", aormode);
		retval = aor_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

		return retval;
}

/*
 * aor_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int aor_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		unsigned char ackbuf[16];
		int ack_len, retval;


		retval = aor_transaction (rig, "MD", 2, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 2 || ackbuf[1] != CR) {
				rig_debug(RIG_DEBUG_ERR,"aor_get_mode: ack NG, len=%d\n",
								ack_len);
				return -RIG_ERJCTED;
		}

		*width = RIG_PASSBAND_NORMAL;
		switch (ackbuf[0]) {
			case MD_AM:		*mode = RIG_MODE_AM; break;
			case MD_NAM:	
							*mode = RIG_MODE_AM;
							*width = rig_passband_narrow(rig, *mode); 
							break;
			case MD_WAM:	
							*mode = RIG_MODE_AM;
							*width = rig_passband_wide(rig, *mode); 
							break;
			case MD_CW:		*mode = RIG_MODE_CW; break;
			case MD_USB:	*mode = RIG_MODE_USB; break;
			case MD_LSB:	*mode = RIG_MODE_LSB; break;
			case MD_WFM:	*mode = RIG_MODE_WFM; break;
			case MD_NFM:	*mode = RIG_MODE_FM; break;
			case MD_SFM:	
							*mode = RIG_MODE_FM;
							*width = rig_passband_narrow(rig, *mode); 
							break;
			default:
				rig_debug(RIG_DEBUG_ERR,"aor_get_mode: unsupported mode %d\n",
								ackbuf[0]);
				return -RIG_EINVAL;
		}
		if (*width != RIG_PASSBAND_NORMAL)
				*width = rig_passband_normal(rig, *mode);

		return RIG_OK;
}

/*
 * aor_set_ts
 * Assumes rig!=NULL
 */
int aor_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
		unsigned char tsbuf[16],ackbuf[16];
		int ts_len, ack_len;

		/*
		 * actually, tuning step must be like nnnnm0, 
		 * where m must be 0 or 5 (for 50Hz).
		 */
		ts_len = sprintf(tsbuf,"ST%06ld", ts);

		return aor_transaction (rig, tsbuf, ts_len, ackbuf, &ack_len);
}

/*
 * aor_set_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_set_powerstat(RIG *rig, powerstat_t status)
{
		unsigned char ackbuf[16];
		int ack_len;

		if (status != RIG_POWER_OFF)
				return -RIG_EINVAL;

		/* turn off power */
		return aor_transaction (rig, "QP", 2, ackbuf, &ack_len);
}


/*
 * init_aor is called by rig_backend_load
 */
int init_aor(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "aor: _init called\n");

		rig_register(&ar8200_caps);

		return RIG_OK;
}


