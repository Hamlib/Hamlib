/*
 *  Hamlib Interface - event handling
 *  Copyright (c) 2000-2010 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* Doc todo: Verify assignment to rig group.  Consider doc of internal rtns. */
/**
 * \addtogroup rig
 * @{
 */

/**
 * \file event.c
 * \brief Event handling
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include <sys/stat.h>

#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif

#include <fcntl.h>
#include <signal.h>
#include <errno.h>


#include <hamlib/rig.h>
#include "event.h"

#if defined(WIN32) && !defined(HAVE_TERMIOS_H)
#  include "win32termios.h"
#  define select win32_serial_select
#endif

#ifndef DOC_HIDDEN

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)


#ifdef HAVE_SIGACTION
static struct sigaction hamlib_trn_oldact, hamlib_trn_poll_oldact;

#ifdef HAVE_SIGINFO_T
static void sa_sigioaction(int signum, siginfo_t *si, void *data);
static void sa_sigalrmaction(int signum, siginfo_t *si, void *data);
#else
static void sa_sigiohandler(int signum);
static void sa_sigalrmhandler(int signum);
#endif
#endif


/* This one should be in an include file */
extern int foreach_opened_rig(int (*cfunc)(RIG *, rig_ptr_t), rig_ptr_t data);


/*
 * add_trn_rig
 * not exported in Hamlib API.
 * Assumes rig->caps->transceive == RIG_TRN_RIG
 */
int add_trn_rig(RIG *rig)
{
#ifdef HAVE_SIGACTION
    struct sigaction act;
    int status;

    /*
     * FIXME: multiple open will register several time SIGIO hndlr
     */
    memset(&act, 0, sizeof(act));

#ifdef HAVE_SIGINFO_T
    act.sa_sigaction = sa_sigioaction;
#else
    act.sa_handler = sa_sigiohandler;
#endif

    sigemptyset(&act.sa_mask);

#if defined(HAVE_SIGINFO_T) && defined(SA_SIGINFO)
    act.sa_flags = SA_SIGINFO | SA_RESTART;
#else
    act.sa_flags = SA_RESTART;
#endif

    status = sigaction(SIGIO, &act, &hamlib_trn_oldact);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: sigaction failed: %s\n",
                  __func__,
                  strerror(errno));
    }

    status = fcntl(rig->state.rigport.fd, F_SETOWN, getpid());

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: fcntl SETOWN failed: %s\n",
                  __func__,
                  strerror(errno));
    }

#if defined(HAVE_SIGINFO_T) && defined(O_ASYNC)
#ifdef F_SETSIG
    status = fcntl(rig->state.rigport.fd, F_SETSIG, SIGIO);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: fcntl SETSIG failed: %s\n",
                  __func__,
                  strerror(errno));
    }

#endif

    status = fcntl(rig->state.rigport.fd, F_SETFL, O_ASYNC);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: fcntl SETASYNC failed: %s\n",
                  __func__,
                  strerror(errno));
    }

#else
    return -RIG_ENIMPL;
#endif

    return RIG_OK;

#else
    return -RIG_ENIMPL;
#endif  /* !HAVE_SIGACTION */
}


/*
 * remove_trn_rig
 * not exported in Hamlib API.
 * Assumes rig->caps->transceive == RIG_TRN_RIG
 */
int remove_trn_rig(RIG *rig)
{
#ifdef HAVE_SIGACTION
    int status;

    /* assert(rig->caps->transceive == RIG_TRN_RIG); */

#if defined(HAVE_SIGINFO_T) && defined(O_ASYNC)
    status = fcntl(rig->state.rigport.fd, F_SETFL, 0);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: fcntl SETASYNC failed: %s\n",
                  __func__,
                  strerror(errno));
    }

#endif

    status = sigaction(SIGIO, &hamlib_trn_oldact, NULL);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: sigaction failed: %s\n",
                  __func__,
                  strerror(errno));
    }

    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif  /* !HAVE_SIGACTION */
}


#ifdef HAVE_SIGACTION


/*
 * add_trn_poll_rig
 * not exported in Hamlib API.
 */
static int add_trn_poll_rig(RIG *rig)
{
#ifdef HAVE_SIGACTION
    struct sigaction act;
    int status;

    /*
     * FIXME: multiple open will register several time SIGALRM hndlr
     */
    memset(&act, 0, sizeof(act));

#ifdef HAVE_SIGINFO_T
    act.sa_sigaction = sa_sigalrmaction;
#else
    act.sa_handler = sa_sigalrmhandler;
#endif
    act.sa_flags = SA_RESTART;

    sigemptyset(&act.sa_mask);

    status = sigaction(SIGALRM, &act, &hamlib_trn_poll_oldact);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s sigaction failed: %s\n",
                  __func__,
                  strerror(errno));
    }

    return RIG_OK;

#else
    return -RIG_ENIMPL;
#endif  /* !HAVE_SIGINFO */
}


/*
 * remove_trn_poll_rig
 * not exported in Hamlib API.
 */
static int remove_trn_poll_rig(RIG *rig)
{
#ifdef HAVE_SIGINFO
    int status;

    status = sigaction(SIGALRM, &hamlib_trn_poll_oldact, NULL);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s sigaction failed: %s\n",
                  __func__,
                  strerror(errno));
    }

    return RIG_OK;

#else
    return -RIG_ENIMPL;
#endif  /* !HAVE_SIGINFO */
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
    if (rig->state.rigport.type.rig != RIG_PORT_SERIAL
            || rig->state.rigport.fd == -1)
    {
        return -1;
    }

    /* FIXME: siginfo is not portable, however use it where available */
#if 0&&defined(HAVE_SIGINFO_T)
    siginfo_t *si = (siginfo_t *)data;

    if (rig->state.rigport.fd != si->si_fd)
    {
        return -1;
    }

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
    retval = select(rig->state.rigport.fd + 1, &rfds, NULL, NULL, &tv);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: select: %s\n",
                  __func__,
                  strerror(errno));
        return -1;
    }

#endif

    /*
     * Do not disturb, the backend is currently receiving data
     */
    if (rig->state.hold_decode)
    {
        return -1;
    }

    if (rig->caps->decode_event)
    {
        rig->caps->decode_event(rig);
    }

    return 1;   /* process each opened rig */
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
    {
        return -1;
    }

    /*
     * Do not disturb, the backend is currently receiving data
     */
    if (rig->state.hold_decode)
    {
        return -1;
    }

    rig->state.hold_decode = 2;

    if (rig->caps->get_vfo && rig->callbacks.vfo_event)
    {
        vfo_t vfo = RIG_VFO_CURR;

        retval = rig->caps->get_vfo(rig, &vfo);

        if (retval == RIG_OK)
        {
            if (vfo != rs->current_vfo)
            {
                rig->callbacks.vfo_event(rig, vfo, rig->callbacks.vfo_arg);
            }

            rs->current_vfo = vfo;
        }
    }

    if (rig->caps->get_freq && rig->callbacks.freq_event)
    {
        freq_t freq;

        retval = rig->caps->get_freq(rig, RIG_VFO_CURR, &freq);

        if (retval == RIG_OK)
        {
            if (freq != rs->current_freq)
            {
                rig->callbacks.freq_event(rig,
                                          RIG_VFO_CURR,
                                          freq,
                                          rig->callbacks.freq_arg);
            }

            rs->current_freq = freq;
        }
    }

    if (rig->caps->get_mode && rig->callbacks.mode_event)
    {
        rmode_t rmode;
        pbwidth_t width;

        retval = rig->caps->get_mode(rig, RIG_VFO_CURR, &rmode, &width);

        if (retval == RIG_OK)
        {
            if (rmode != rs->current_mode || width != rs->current_width)
            {
                rig->callbacks.mode_event(rig,
                                          RIG_VFO_CURR,
                                          rmode,
                                          width,
                                          rig->callbacks.mode_arg);
            }

            rs->current_mode = rmode;
            rs->current_width = width;
        }
    }

    rig->state.hold_decode = 0;

    return 1;   /* process each opened rig */
}


/*
 * This is the SIGIO handler
 *
 * lookup in the list of open rigs,
 * check the rig is not holding SIGIO,
 * then call rig->caps->decode_event()  (this is done by search_rig)
 */
#ifdef HAVE_SIGINFO_T
static void sa_sigioaction(int signum, siginfo_t *si, rig_ptr_t data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: activity detected\n", __func__);

    foreach_opened_rig(search_rig_and_decode, si);
}

#else

static void sa_sigiohandler(int signum)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: activity detected\n", __func__);

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
static void sa_sigalrmaction(int signum, siginfo_t *si, rig_ptr_t data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s entered\n", __func__);

    foreach_opened_rig(search_rig_and_poll, si);
}

#else

static void sa_sigalrmhandler(int signum)
{
    rig_debug(RIG_DEBUG_TRACE, "%s entered\n", __func__);

    foreach_opened_rig(search_rig_and_poll, NULL);
}

#endif /* !HAVE_SIGINFO_T */

#endif /* HAVE_SIGINFO */

#endif  /* !DOC_HIDDEN */


/**
 * \brief set the callback for freq events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for freq events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int HAMLIB_API rig_set_freq_callback(RIG *rig, freq_cb_t cb, rig_ptr_t arg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    rig->callbacks.freq_event = cb;
    rig->callbacks.freq_arg = arg;

    return RIG_OK;
}


/**
 * \brief set the callback for mode events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for mode events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int HAMLIB_API rig_set_mode_callback(RIG *rig, mode_cb_t cb, rig_ptr_t arg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    rig->callbacks.mode_event = cb;
    rig->callbacks.mode_arg = arg;

    return RIG_OK;
}


/**
 * \brief set the callback for vfo events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for vfo events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int HAMLIB_API rig_set_vfo_callback(RIG *rig, vfo_cb_t cb, rig_ptr_t arg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    rig->callbacks.vfo_event = cb;
    rig->callbacks.vfo_arg = arg;

    return RIG_OK;
}


/**
 * \brief set the callback for ptt events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for ptt events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int HAMLIB_API rig_set_ptt_callback(RIG *rig, ptt_cb_t cb, rig_ptr_t arg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    rig->callbacks.ptt_event = cb;
    rig->callbacks.ptt_arg = arg;

    return RIG_OK;
}


/**
 * \brief set the callback for dcd events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for dcd events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int HAMLIB_API rig_set_dcd_callback(RIG *rig, dcd_cb_t cb, rig_ptr_t arg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    rig->callbacks.dcd_event = cb;
    rig->callbacks.dcd_arg = arg;

    return RIG_OK;
}


/**
 * \brief set the callback for pipelined tuning module
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 * used to maintain state during pipelined tuning.
 *
 *  Install a callback for pipelined tuning module, to be called when the
 *  rig_scan( SCAN_PLT ) loop needs a new frequency, mode and width.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int HAMLIB_API rig_set_pltune_callback(RIG *rig, pltune_cb_t cb, rig_ptr_t arg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    rig->callbacks.pltune = cb;
    rig->callbacks.pltune_arg = arg;

    return RIG_OK;
}


/**
 * \brief control the transceive mode
 * \param rig   The rig handle
 * \param trn   The transceive status to set to
 *
 *  Enable/disable the transceive handling of a rig and kick off async mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_trn()
 */
int HAMLIB_API rig_set_trn(RIG *rig, int trn)
{
    const struct rig_caps *caps;
    int retcode = RIG_OK;
#ifdef HAVE_SETITIMER
    struct itimerval value;
#endif

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    /* detect whether tranceive is active already */
    if (trn != RIG_TRN_OFF && rig->state.transceive != RIG_TRN_OFF)
    {
        if (trn == rig->state.transceive)
        {
            return RIG_OK;
        }
        else
        {
            /* when going POLL<->RIG, transtition to OFF */
            retcode = rig_set_trn(rig, RIG_TRN_OFF);

            if (retcode != RIG_OK)
            {
                return retcode;
            }
        }
    }

    switch (trn)
    {
    case RIG_TRN_RIG:
        if (caps->transceive != RIG_TRN_RIG)
        {
            return -RIG_ENAVAIL;
        }

        retcode = add_trn_rig(rig);

        /* some protocols (e.g. CI-V's) offer no way
         * to turn on/off the transceive mode */
        if (retcode == RIG_OK && caps->set_trn)
        {
            retcode = caps->set_trn(rig, RIG_TRN_RIG);
        }

        break;

    case RIG_TRN_POLL:
#ifdef HAVE_SETITIMER

        add_trn_poll_rig(rig);

        /* install handler here */
        value.it_value.tv_sec = 0;
        value.it_value.tv_usec = rig->state.poll_interval * 1000;
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_usec = rig->state.poll_interval * 1000;
        retcode = setitimer(ITIMER_REAL, &value, NULL);

        if (retcode == -1)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: setitimer: %s\n",
                      __func__,
                      strerror(errno));
            remove_trn_poll_rig(rig);
            return -RIG_EINTERNAL;
        }

#else
        return -RIG_ENAVAIL;
#endif
        break;

    case RIG_TRN_OFF:
        if (rig->state.transceive == RIG_TRN_POLL)
        {
#ifdef HAVE_SETITIMER

            retcode = remove_trn_poll_rig(rig);

            value.it_value.tv_sec = 0;
            value.it_value.tv_usec = 0;
            value.it_interval.tv_sec = 0;
            value.it_interval.tv_usec = 0;

            retcode = setitimer(ITIMER_REAL, &value, NULL);

            if (retcode == -1)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: setitimer: %s\n",
                          __func__,
                          strerror(errno));
                return -RIG_EINTERNAL;
            }

#else
            return -RIG_ENAVAIL;
#endif
        }
        else if (rig->state.transceive == RIG_TRN_RIG)
        {
            retcode = remove_trn_rig(rig);

            if (caps->set_trn && caps->transceive == RIG_TRN_RIG)
            {
                retcode = caps->set_trn(rig, RIG_TRN_OFF);
            }
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    if (retcode == RIG_OK)
    {
        rig->state.transceive = trn;
    }

    return retcode;
}


/**
 * \brief get the current transceive mode
 * \param rig   The rig handle
 * \param trn   The location where to store the current transceive mode
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
int HAMLIB_API rig_get_trn(RIG *rig, int *trn)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !trn)
    {
        return -RIG_EINVAL;
    }

    if (rig->caps->get_trn != NULL)
    {
        return rig->caps->get_trn(rig, trn);
    }

    *trn = rig->state.transceive;
    return RIG_OK;
}

/** @} */
