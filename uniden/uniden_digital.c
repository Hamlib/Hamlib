/*
 *  Hamlib Uniden backend - uniden_digital backend
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *	$Id: uniden_digital.c,v 1.1 2008-10-07 18:58:08 fillods Exp $
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


