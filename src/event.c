/* hamlib - Ham Radio Control Libraries
   Copyright (C) 2000,2001 Stephane Fillod and Frank Singleton
   This file is part of the hamlib package.

   $Id: event.c,v 1.8 2001-06-17 21:54:31 f4cfe Exp $

   Hamlib is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   Hamlib is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>


#define HAMLIB_DLL
#include <hamlib/rig.h>

#include "event.h"


#if defined(_WIN32) || defined(__CYGWIN__)
static void sa_sigiohandler(int signum);
#else
static void sa_sigioaction(int signum, siginfo_t *si, void *data);
#endif

/* This one should be in an include file */
int foreach_opened_rig(int (*cfunc)(RIG *, rig_ptr_t),rig_ptr_t data);

/*
 * add_trn_rig
 * not exported in Hamlib API.
 * Assumes rig->caps->transceive == RIG_TRN_RIG
 */
int add_trn_rig(RIG *rig)
{
	struct sigaction act;
	int status;

		/*
		 * FIXME: multiple open will register several time SIGIO hndlr
		 */
#if defined(_WIN32) || defined(__CYGWIN__)
	act.sa_handler = sa_sigiohandler;
#else
	act.sa_sigaction = sa_sigioaction;
#endif

	sigemptyset(&act.sa_mask);

#if defined(_WIN32) || defined(__CYGWIN__)
	act.sa_flags = 0;
#else
	act.sa_flags = SA_SIGINFO;
#endif

	status = sigaction(SIGIO, &act, NULL);
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open sigaction failed: %s\n",
						strerror(errno));

	status = fcntl(rig->state.rigport.fd, F_SETOWN, getpid());
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open fcntl SETOWN failed: %s\n",
						strerror(errno));

#if defined(_WIN32) || defined(__CYGWIN__)
#else
	status = fcntl(rig->state.rigport.fd, F_SETSIG, SIGIO);
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open fcntl SETSIG failed: %s\n",
						strerror(errno));

	status = fcntl(rig->state.rigport.fd, F_SETFL, O_ASYNC);
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open fcntl SETASYNC failed: %s\n",
						strerror(errno));
#endif

	return RIG_OK;
}

/*
 * remove_trn_rig
 * not exported in Hamlib API.
 * Assumes rig->caps->transceive == RIG_TRN_RIG
 */
int remove_trn_rig(RIG *rig)
{
	return -RIG_ENIMPL;
}

/*
 * This is used by sa_sigio, the SIGIO handler
 * to find out which rig generated this event,
 * and decode/process it.
 *
 * assumes rig!=NULL, rig->state.rigport.fd>=0
 */
static int search_rig_and_decode(RIG *rig, rig_ptr_t data)
{
	fd_set rfds;
	struct timeval tv;
	int retval;
#if 0
	siginfo_t *si = (siginfo_t*)data;

	if (rig->state.rigport.fd != si->si_fd)
			return -1;
#else
	FD_ZERO(&rfds);
	FD_SET(rig->state.rigport.fd, &rfds);
	/* Read status immediately. */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	/* don't use FIONREAD to detect activity
	 * since it is less portable than select
	 * REM: EINTR possible with 0sec timeout? retval==0?
	 */
	retval = select(rig->state.rigport.fd+1, &rfds, NULL, NULL, &tv);                                                      
	if (retval < 0) {
		rig_debug(RIG_DEBUG_ERR, "search_rig_and_decode: select: %s\n",
								strerror(errno));
		return -1;
	}
#endif

	/*
	 * Do not disturb, the backend is currently receiving data
	 */
	if (rig->state.hold_decode)
			return -1;

	if (rig->caps->decode_event)
		rig->caps->decode_event(rig);

	return 1;	/* process each opened rig */
}


/*
 * This is the SIGIO handler
 *
 * lookup in the list of open rigs,
 * check the rig is not holding SIGIO,
 * then call rig->caps->decode_event()  (this is done by search_rig)
 */
#if defined(_WIN32) || defined(__CYGWIN__)
static void sa_sigiohandler(int signum)
{
	rig_debug(RIG_DEBUG_VERBOSE, "sa_sigiohandler: activity detected\n");

	foreach_opened_rig(search_rig_and_decode, NULL);
}

#else
static void sa_sigioaction(int signum, siginfo_t *si, rig_ptr_t data)
{
	rig_debug(RIG_DEBUG_VERBOSE, "sa_sigioaction: activity detected\n");

	foreach_opened_rig(search_rig_and_decode, si);
}

#endif

