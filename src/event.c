/*
 *  Hamlib Interface - event handling
 *  Copyright (c) 2000-2003 by Stephane Fillod and Frank Singleton
 *
 *	$Id: event.c,v 1.19 2003-08-15 01:25:26 fillods Exp $
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
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <errno.h>


#include <hamlib/rig.h>

#include "event.h"

#if defined(WIN32)
#include "win32termios.h"
#endif

#ifndef DOC_HIDDEN

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)


#ifdef HAVE_SIGINFO_T
static RETSIGTYPE sa_sigioaction(int signum, siginfo_t *si, void *data);
static RETSIGTYPE sa_sigalrmaction(int signum, siginfo_t *si, void *data);
#else
static RETSIGTYPE sa_sigiohandler(int signum);
static RETSIGTYPE sa_sigalrmhandler(int signum);
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
#ifndef WIN32
	struct sigaction act;
	int status;

		/*
		 * FIXME: multiple open will register several time SIGIO hndlr
		 */
#ifdef HAVE_SIGINFO_T
	act.sa_sigaction = sa_sigioaction;
#else
	act.sa_handler = sa_sigiohandler;
#endif

	sigemptyset(&act.sa_mask);

#if defined(HAVE_SIGINFO_T) && defined(SA_SIGINFO)
	act.sa_flags = SA_SIGINFO;
#else
	act.sa_flags = 0;
#endif

	status = sigaction(SIGIO, &act, NULL);
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open sigaction failed: %s\n",
						strerror(errno));

	status = fcntl(rig->state.rigport.fd, F_SETOWN, getpid());
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open fcntl SETOWN failed: %s\n",
						strerror(errno));

#if defined(HAVE_SIGINFO_T) && defined(O_ASYNC)
#ifdef F_SETSIG
	status = fcntl(rig->state.rigport.fd, F_SETSIG, SIGIO);
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open fcntl SETSIG failed: %s\n",
						strerror(errno));
#endif

	status = fcntl(rig->state.rigport.fd, F_SETFL, O_ASYNC);
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"rig_open fcntl SETASYNC failed: %s\n",
						strerror(errno));
#else
	return -RIG_ENIMPL;
#endif

	return RIG_OK;

#else
	return -RIG_ENIMPL;
#endif	/* !WIN32 */
}

/*
 * add_trn_poll_rig
 * not exported in Hamlib API.
 */
int add_trn_poll_rig(RIG *rig)
{
#ifndef WIN32
	struct sigaction act;
	int status;

		/*
		 * FIXME: multiple open will register several time SIGIO hndlr
		 */
#ifdef HAVE_SIGINFO_T
	act.sa_sigaction = sa_sigalrmaction;
#else
	act.sa_handler = sa_sigalrmhandler;
#endif

	sigemptyset(&act.sa_mask);

	status = sigaction(SIGALRM, &act, NULL);
	if (status < 0)
		rig_debug(RIG_DEBUG_ERR,"%s sigaction failed: %s\n",
				__FUNCTION__,
				strerror(errno));

	return RIG_OK;

#else
	return -RIG_ENIMPL;
#endif	/* !WIN32 */
}

/*
 * remove_trn_poll_rig
 * not exported in Hamlib API.
 */
int remove_trn_poll_rig(RIG *rig)
{
	return -RIG_ENIMPL;
}


/*
 * remove_trn_rig
 * not exported in Hamlib API.
 * Assumes rig->caps->transceive == RIG_TRN_RIG
 */
int remove_trn_rig(RIG *rig)
{
	if (rig->caps->transceive == RIG_TRN_RIG)
		return -RIG_ENIMPL;

	if (rig->state.transceive == RIG_TRN_POLL)
		return remove_trn_poll_rig(rig);

	return -RIG_EINVAL;
}


/*
 * This is used by sa_sigio, the SIGIO handler
 * to find out which rig generated this event,
 * and decode/process it.
 *
 * assumes rig!=NULL
 */
static int search_rig_and_decode(RIG *rig, rig_ptr_t data)
{
	fd_set rfds;
	struct timeval tv;
	int retval;

	/*
	 * so far, only file oriented ports have event reporting support
	 */
	if (rig->state.rigport.type.rig != RIG_PORT_SERIAL || 
			rig->state.rigport.fd == -1)
		return -1;

	/* FIXME: siginfo is not portable, however use it where available */
#if 0&&defined(HAVE_SIGINFO_T)
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
 * This is used by sa_sigio, the SIGALRM handler
 * to poll each RIG in RIG_TRN_POLL mode.
 *
 * assumes rig!=NULL
 */
static int search_rig_and_poll(RIG *rig, rig_ptr_t data)
{
	struct rig_state *rs = &rig->state;
	int retval;

	if (rig->state.transceive != RIG_TRN_POLL)
		return -1;
	/*
	 * Do not disturb, the backend is currently receiving data
	 */
	if (rig->state.hold_decode)
		return -1;

	if (rig->caps->get_vfo && rig->callbacks.vfo_event) {
		vfo_t vfo = RIG_VFO_CURR;

		retval = rig->caps->get_vfo(rig, &vfo);
		if (retval == RIG_OK) {
 			if (vfo != rs->current_vfo) {
				rig->callbacks.vfo_event(rig, vfo, rig->callbacks.vfo_arg);
			}
	 		rs->current_vfo = vfo;
		}
	}
	if (rig->caps->get_freq && rig->callbacks.freq_event) {
		freq_t freq;

		retval = rig->caps->get_freq(rig, RIG_VFO_CURR, &freq);
		if (retval == RIG_OK) {
 			if (freq != rs->current_freq) {
				rig->callbacks.freq_event(rig, RIG_VFO_CURR, freq, rig->callbacks.freq_arg);
			}
	 		rs->current_freq = freq;
		}
	}
	if (rig->caps->get_mode && rig->callbacks.mode_event) {
		rmode_t rmode;
		pbwidth_t width;

		retval = rig->caps->get_mode(rig, RIG_VFO_CURR, &rmode, &width);
		if (retval == RIG_OK) {
 			if (rmode != rs->current_mode || width != rs->current_width) {
				rig->callbacks.mode_event(rig, RIG_VFO_CURR, 
						rmode, width, rig->callbacks.mode_arg);
			}
	 		rs->current_mode = rmode;
	 		rs->current_width = width;
		}
	}

	return 1;	/* process each opened rig */
}


/*
 * This is the SIGIO handler
 *
 * lookup in the list of open rigs,
 * check the rig is not holding SIGIO,
 * then call rig->caps->decode_event()  (this is done by search_rig)
 */
#ifdef HAVE_SIGINFO_T
static RETSIGTYPE sa_sigioaction(int signum, siginfo_t *si, rig_ptr_t data)
{
	rig_debug(RIG_DEBUG_VERBOSE, "sa_sigioaction: activity detected\n");

	foreach_opened_rig(search_rig_and_decode, si);
}

#else

static RETSIGTYPE sa_sigiohandler(int signum)
{
	rig_debug(RIG_DEBUG_VERBOSE, "sa_sigiohandler: activity detected\n");

	foreach_opened_rig(search_rig_and_decode, NULL);
}

#endif


/*
 * This is the SIGALRM handler
 *
 * lookup in the list of open rigs,
 * check the rig is not holding SIGALRM,
 * then call get_freq and check for changes  (this is done by search_rig)
 */
#ifdef HAVE_SIGINFO_T
static RETSIGTYPE sa_sigalrmaction(int signum, siginfo_t *si, rig_ptr_t data)
{
	rig_debug(RIG_DEBUG_TRACE, "sa_sigalrmaction entered\n");

	foreach_opened_rig(search_rig_and_poll, si);
}

#else

static RETSIGTYPE sa_sigalrmhandler(int signum)
{
	rig_debug(RIG_DEBUG_TRACE, "sa_sigalrmaction entered\n");

	foreach_opened_rig(search_rig_and_poll, NULL);
}

#endif

#endif	/* !DOC_HIDDEN */

/**
 * \brief set the callback for freq events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for freq events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_freq_callback(RIG *rig, freq_cb_t cb, rig_ptr_t arg)
{
	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	rig->callbacks.freq_event = cb;
	rig->callbacks.freq_arg = arg;

	return RIG_OK;
}

/**
 * \brief set the callback for mode events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for mode events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_mode_callback(RIG *rig, mode_cb_t cb, rig_ptr_t arg)
{
	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	rig->callbacks.mode_event = cb;
	rig->callbacks.mode_arg = arg;

	return RIG_OK;
}

/**
 * \brief set the callback for vfo events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for vfo events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_vfo_callback(RIG *rig, vfo_cb_t cb, rig_ptr_t arg)
{
	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	rig->callbacks.vfo_event = cb;
	rig->callbacks.vfo_arg = arg;

	return RIG_OK;
}

/**
 * \brief set the callback for ptt events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for ptt events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_ptt_callback(RIG *rig, ptt_cb_t cb, rig_ptr_t arg)
{
	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	rig->callbacks.ptt_event = cb;
	rig->callbacks.ptt_arg = arg;

	return RIG_OK;
}

/**
 * \brief set the callback for dcd events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for dcd events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_dcd_callback(RIG *rig, dcd_cb_t cb, rig_ptr_t arg)
{
	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	rig->callbacks.dcd_event = cb;
	rig->callbacks.dcd_arg = arg;

	return RIG_OK;
}

/**
 * \brief control the transceive mode 
 * \param rig	The rig handle
 * \param trn	The transceive status to set to
 *
 *  Enable/disable the transceive handling of a rig and kick off async mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_trn()
 */

int rig_set_trn(RIG *rig, int trn)
{
	const struct rig_caps *caps;
	int retcode;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (trn == RIG_TRN_RIG) {
		if (caps->transceive != RIG_TRN_RIG)
			return -RIG_ENAVAIL;

		if (rig->state.transceive == RIG_TRN_OFF) {
			retcode = add_trn_rig(rig);
			if (retcode == RIG_OK && caps->set_trn) {
				retcode = caps->set_trn(rig, RIG_TRN_RIG);
			}
		} else {
			return -RIG_ECONF;
		}
	} else if (trn == RIG_TRN_POLL) {
#ifdef HAVE_SETITIMER
		struct itimerval value;

		add_trn_poll_rig(rig);

		/* install handler here */
		value.it_value.tv_sec = 0;
		value.it_value.tv_usec = rig->state.poll_interval*1000;
		value.it_interval.tv_sec = 0;
		value.it_interval.tv_usec = rig->state.poll_interval*1000;
		retcode = setitimer(ITIMER_REAL, &value, NULL);
		if (retcode == -1) {
			rig_debug(RIG_DEBUG_ERR, "%s: setitimer: %s\n",
					__FUNCTION__,
					strerror(errno));
			remove_trn_rig(rig);
			return -RIG_EINTERNAL;
		}
#else
		return -RIG_ENAVAIL;
#endif
	} else if (trn == RIG_TRN_OFF) {
		if (rig->state.transceive == RIG_TRN_POLL) {
#ifdef HAVE_SETITIMER
			struct itimerval value;

			retcode = remove_trn_rig(rig);

			value.it_value.tv_sec = 0;
			value.it_value.tv_usec = 0;
			value.it_interval.tv_sec = 0;
			value.it_interval.tv_usec = 0;

			retcode = setitimer(ITIMER_REAL, &value, NULL);
			if (retcode == -1) {
				rig_debug(RIG_DEBUG_ERR, "%s: setitimer: %s\n",
					__FUNCTION__,
					strerror(errno));
				return -RIG_EINTERNAL;
			}
#else
		return -RIG_ENAVAIL;
#endif
		} else {
			retcode = remove_trn_rig(rig);
			if (caps->set_trn && caps->transceive == RIG_TRN_RIG) {
				retcode = caps->set_trn(rig, RIG_TRN_OFF);
			}
		}
	} else {
		return -RIG_EINVAL;
	}

	if (retcode == RIG_OK)
		rig->state.transceive = trn;
	return retcode;
}

/**
 * \brief get the current transceive mode
 * \param rig	The rig handle
 * \param trn	The location where to store the current transceive mode
 *
 *  Retrieves the current status of the transceive mode, i.e. if radio 
 *  sends new status automatically when some changes happened on the radio.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int rig_get_trn(RIG *rig, int *trn)
{
	if (CHECK_RIG_ARG(rig) || !trn)
		return -RIG_EINVAL;

	if (rig->caps->get_trn != NULL)
		return rig->caps->get_trn(rig, trn);

	*trn = rig->state.transceive;
	return RIG_OK;
}


