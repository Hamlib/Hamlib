/*
 *  Hamlib Uniden backend - main file
 *  Copyright (c) 2001-2004 by Stephane Fillod
 *
 *	$Id: uniden.c,v 1.9 2004-02-08 16:59:48 fillods Exp $
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
#include "register.h"

#include "uniden.h"

/*
 * Uniden backend: should work for BC895xlt, also for BC235xlt
 * and most probably for the RadioShack PRO-2042, which is RadioShack's
 * version of the Uniden BC235xlt.
 * Protocol information available at http://www.cantonmaine.com/pro2052.htm
 *
 * It seems like this rig has no VFO, I mean only mem channels.
 * Is that correct? --SF
 */

#define EOM "\r"

#define BUFSZ 32

/*
 * uniden_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 */
int uniden_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int retval;
	struct rig_state *rs;

	rs = &rig->state;

	serial_flush(&rs->rigport);

	retval = write_block(&rs->rigport, cmd, cmd_len);
	if (retval != RIG_OK)
		return retval;


	/* no data expected, TODO: flush input? */
	if (!data || !data_len)
			return 0;

	*data_len = read_string(&rs->rigport, data, BUFSZ, "\x0a", 1);

	return RIG_OK;
}

/*
 * uniden_set_freq
 * Assumes rig!=NULL
 */
int uniden_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		char freqbuf[BUFSZ];
		int freq_len;

		/* max 8 digits */
		if (freq >= GHz(1))
				return -RIG_EINVAL;

		/* exactly 8 digits */
		freq_len = sprintf(freqbuf, "RF%08Ld" EOM, (long long)freq);

		return uniden_transaction (rig, freqbuf, freq_len, NULL, NULL);
}

/*
 * uniden_set_mem
 * Assumes rig!=NULL
 */
int uniden_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		char cmdbuf[BUFSZ];
		int cmd_len;

		cmd_len = sprintf(cmdbuf, "MA%03d" EOM, ch);

		return uniden_transaction (rig, cmdbuf, cmd_len, NULL, NULL);
}

/*
 * initrigs_uniden is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(uniden)
{
		rig_debug(RIG_DEBUG_VERBOSE, "uniden: _init called\n");

		rig_register(&bc895_caps);
		rig_register(&bc245_caps);

		return RIG_OK;
}

