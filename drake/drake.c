/*
 *  Hamlib Drake backend - main file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: drake.c,v 1.1 2002-07-08 22:10:43 fillods Exp $
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
#include <sys/ioctl.h>
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>

#include "drake.h"


/*
 * This is a skeleton backend. Please fill in the gaps.
 */


#define BUFSZ 64

/*
 * drake_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 *
 * FIXME: this is a skeleton, rewrite me for Drake protocol
 */
int drake_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
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
 * drake_set_freq
 * Assumes rig!=NULL
 */
int drake_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	return -RIG_ENIMPL;
}



/*
 * initrigs_drake is called by rig_backend_load
 */
int initrigs_drake(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "drake: _init called\n");

		rig_register(&r8b_caps);

		return RIG_OK;
}

