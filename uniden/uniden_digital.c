/*
 *  Hamlib Uniden backend - uniden_digital backend
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *	$Id: uniden_digital.c,v 1.2 2008-10-17 11:52:04 roger-linux Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"

#include "uniden_digital.h"


/*
 * Uniden Digital backend should work for:
 * BCD996T as well as the BCD396T.  Some protocols available for the
 * BCD996T may not be available for the BCD396T or modified to work.
 *
 * Protocol information can be found here:
 * http://www.uniden.com/files/BCD396T_Protocol.pdf
 * http://www.uniden.com/files/BCD996T_Protocol.pdf
 *
 * There are undocumented commands such as firmware_dump and
 * firmware_load.  These commands are defined within DSctl code.
 *
 * There are two methods of retrieving the next memory location
 * (aka frequency bank).  Use either the "Get Next Location" or 
 * use the address returned from one of the commands.  If you decide
 * the latter method, the order is slightly confusing but, I have it
 * well documented within DSctl.  The latter method is also as much
 * as 30% faster then using the Uniden software or "Get Next
 * Location" command.
 */

static const struct { rig_model_t model; const char *id; }
uniden_id_string_list[] = {
	{ RIG_MODEL_BCD396T,  "BCD396T" },
	{ RIG_MODEL_BCD996T,  "BCD99tT" },
	{ RIG_MODEL_NONE, NULL },	/* end marker */
};


/*
 * skeleton
 */
int uniden_digital_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
#if 0
	char freqbuf[BUFSZ];
	size_t freq_len=BUFSZ;

	/* freq in hundreds of Hz */
	freq /= 100;

	/* exactly 8 digits */
	freq_len = sprintf(freqbuf, "RF%08u" EOM, (unsigned)freq);

	return uniden_transaction (rig, freqbuf, freq_len, NULL, NULL, NULL);
#else
	return -RIG_ENIMPL;
#endif
}

/*
 * skeleton
 */
int uniden_digital_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
#if 0
	char freqbuf[BUFSZ];
	size_t freq_len=BUFSZ;
	int ret;

	ret = uniden_transaction (rig, "RF" EOM, 3, NULL, freqbuf, &freq_len);
	if (ret != RIG_OK)
		return ret;

	if (freq_len < 10)
		return -RIG_EPROTO;

	sscanf(freqbuf+2, "%"SCNfreq, freq);
	/* returned freq in hundreds of Hz */
	*freq *= 100;

	return RIG_OK;
#else
	return -RIG_ENIMPL;
#endif
}


