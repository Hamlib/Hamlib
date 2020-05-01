/*
 *  Hamlib Interface - generic file based io functions
 *  Copyright (c) 2000-2012 by Stephane Fillod
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

/**
 * \file iofunc.c
 * \brief Generic file-based IO functions
 */

/**
 * \addtogroup rig_internal
 * @{
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <hamlib/rig.h>
#include "iofunc.h"
#include "misc.h"

#include "serial.h"
#include "parallel.h"
#include "usb_port.h"
#include "network.h"
#include "cm108.h"
#include "gpio.h"

/**
 * \brief Open a hamlib_port based on its rig port type
 * \param p rig port descriptor
 * \return status
 */
int HAMLIB_API port_open(hamlib_port_t *p)
{
    int status;
    int want_state_delay = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    p->fd = -1;

    switch (p->type.rig)
    {
    case RIG_PORT_SERIAL:
        status = serial_open(p);

        if (status < 0)
        {
            return status;
        }

        if (p->parm.serial.rts_state != RIG_SIGNAL_UNSET
                && p->parm.serial.handshake != RIG_HANDSHAKE_HARDWARE)
        {
            status = ser_set_rts(p,
                                 p->parm.serial.rts_state == RIG_SIGNAL_ON);
            want_state_delay = 1;
        }

        if (status != 0)
        {
            return status;
        }

        if (p->parm.serial.dtr_state != RIG_SIGNAL_UNSET)
        {
            status = ser_set_dtr(p,
                                 p->parm.serial.dtr_state == RIG_SIGNAL_ON);
            want_state_delay = 1;
        }

        if (status != 0)
        {
            return status;
        }

        /*
         * Wait whatever electrolytics in the circuit come up to voltage.
         * Is 100ms enough? Too much?
         */
        if (want_state_delay)
        {
            hl_usleep(100 * 1000);
        }

        break;

    case RIG_PORT_PARALLEL:
        status = par_open(p);

        if (status < 0)
        {
            return status;
        }

        break;

    case RIG_PORT_CM108:
        status = cm108_open(p);

        if (status < 0)
        {
            return status;
        }

        break;

    case RIG_PORT_DEVICE:
        status = open(p->pathname, O_RDWR, 0);

        if (status < 0)
        {
            return -RIG_EIO;
        }

        p->fd = status;
        break;

    case RIG_PORT_USB:
        status = usb_port_open(p);

        if (status < 0)
        {
            return status;
        }

        break;

    case RIG_PORT_NONE:
    case RIG_PORT_RPC:
        break;  /* ez :) */

    case RIG_PORT_NETWORK:
    case RIG_PORT_UDP_NETWORK:
        /* FIXME: hardcoded network port */
        status = network_open(p, 4532);

        if (status < 0)
        {
            return status;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/**
 * \brief Close a hamlib_port
 * \param p rig port descriptor
 * \param port_type equivalent rig port type
 * \return status
 */
int HAMLIB_API port_close(hamlib_port_t *p, rig_port_t port_type)
{
    int ret = RIG_OK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (p->fd != -1)
    {
        switch (port_type)
        {
        case RIG_PORT_SERIAL:
            ret = ser_close(p);
            break;

        case RIG_PORT_USB:
            ret = usb_port_close(p);
            break;

        case RIG_PORT_NETWORK:
        case RIG_PORT_UDP_NETWORK:
            ret = network_close(p);
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s(): Unknown port type %d\n",
                      __func__, port_type);

        /* fall through */
        case RIG_PORT_DEVICE:
            ret = close(p->fd);
        }

        p->fd = -1;
    }

    return ret;
}


#if defined(WIN32) && !defined(HAVE_TERMIOS_H)
#  include "win32termios.h"

extern int is_uh_radio_fd(int fd);

/* On MinGW32/MSVC/.. the appropriate accessor must be used
 * depending on the port type, sigh.
 */
static ssize_t port_read(hamlib_port_t *p, void *buf, size_t count)
{
    int i;
    ssize_t ret;

    /*
     * Since WIN32 does its special serial read, we have
     * to catch the microHam case to do just "read".
     * Note that we always have RIG_PORT_SERIAL in the
     * microHam case.
     */
    if (is_uh_radio_fd(p->fd))
    {
        return read(p->fd, buf, count);
    }

    if (p->type.rig == RIG_PORT_SERIAL)
    {
        ret = win32_serial_read(p->fd, buf, count);

        if (p->parm.serial.data_bits == 7)
        {
            unsigned char *pbuf = buf;

            /* clear MSB */
            for (i = 0; i < ret; i++)
            {
                pbuf[i] &= ~0x80;
            }
        }

        return ret;
    }
    else if (p->type.rig == RIG_PORT_NETWORK
             || p->type.rig == RIG_PORT_UDP_NETWORK)
    {
        return recv(p->fd, buf, count, 0);
    }
    else
    {
        return read(p->fd, buf, count);
    }
}

static ssize_t port_write(hamlib_port_t *p, const void *buf, size_t count)
{
    /*
     * Since WIN32 does its special serial write, we have
     * to catch the microHam case to do just "write".
     * Note that we always have RIG_PORT_SERIAL in the
     * microHam case.
     */
    if (is_uh_radio_fd(p->fd))
    {
        return write(p->fd, buf, count);
    }

    if (p->type.rig == RIG_PORT_SERIAL)
    {
        return win32_serial_write(p->fd, buf, count);
    }
    else if (p->type.rig == RIG_PORT_NETWORK
             || p->type.rig == RIG_PORT_UDP_NETWORK)
    {
        return send(p->fd, buf, count, 0);
    }
    else
    {
        return write(p->fd, buf, count);
    }
}


static int port_select(hamlib_port_t *p,
                       int n,
                       fd_set *readfds,
                       fd_set *writefds,
                       fd_set *exceptfds,
                       struct timeval *timeout)
{
#if 1

    /* select does not work very well with writefds/exceptfds
     * So let's pretend there's none of them
     */
    if (exceptfds)
    {
        FD_ZERO(exceptfds);
    }

    if (writefds)
    {
        FD_ZERO(writefds);
    }

    writefds = NULL;
    exceptfds = NULL;
#endif

    /*
     * Since WIN32 does its special serial select, we have
     * to catch the microHam case to do just "select".
     * Note that we always have RIG_PORT_SERIAL in the
     * microHam case.
     */
    if (is_uh_radio_fd(p->fd))
    {
        return select(n, readfds, writefds, exceptfds, timeout);
    }

    if (p->type.rig == RIG_PORT_SERIAL)
    {
        return win32_serial_select(n, readfds, writefds, exceptfds, timeout);
    }
    else
    {
        return select(n, readfds, writefds, exceptfds, timeout);
    }
}


#else

/* POSIX */

static ssize_t port_read(hamlib_port_t *p, void *buf, size_t count)
{
    if (p->type.rig == RIG_PORT_SERIAL && p->parm.serial.data_bits == 7)
    {
        unsigned char *pbuf = buf;

        int ret = read(p->fd, buf, count);

        /* clear MSB */
        int i;

        for (i = 0; i < ret; i++)
        {
            pbuf[i] &= ~0x80;
        }

        return ret;
    }
    else
    {
        return read(p->fd, buf, count);
    }
}

//! @cond Doxygen_Suppress
#define port_write(p,b,c) write((p)->fd,(b),(c))
#define port_select(p,n,r,w,e,t) select((n),(r),(w),(e),(t))
//! @endcond

#endif

/**
 * \brief Write a block of characters to an fd.
 * \param p rig port descriptor
 * \param txbuffer command sequence to be sent
 * \param count number of bytes to send
 * \return 0 = OK, <0 = NOK
 *
 * Write a block of count characters to port file descriptor,
 * with a pause between each character if write_delay is > 0
 *
 * The write_delay is for Yaesu type rigs..require 5 character
 * sequence to be sent with 50-200msec between each char.
 *
 * Also, post_write_delay is for some Yaesu rigs (eg: FT747) that
 * get confused with sequential fast writes between cmd sequences.
 *
 * input:
 *
 * fd - file descriptor to write to
 * txbuffer - pointer to a command sequence array
 * count - count of byte to send from the txbuffer
 * write_delay - write delay in ms between 2 chars
 * post_write_delay - minimum delay between two writes
 * post_write_date - timeval of last write
 *
 * Actually, this function has nothing specific to serial comm,
 * it could work very well also with any file handle, like a socket.
 */

int HAMLIB_API write_block(hamlib_port_t *p, const char *txbuffer, size_t count)
{
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

#ifdef WANT_NON_ACTIVE_POST_WRITE_DELAY

    if (p->post_write_date.tv_sec != 0)
    {
        signed int date_delay;    /* in us */
        struct timeval tv;

        /* FIXME in Y2038 ... */
        gettimeofday(&tv, NULL);
        date_delay = p->post_write_delay * 1000 -
                     ((tv.tv_sec - p->post_write_date.tv_sec) * 1000000 +
                      (tv.tv_usec - p->post_write_date.tv_usec));

        if (date_delay > 0)
        {
            /*
             * optional delay after last write
             */
            hl_sleep(date_delay);
        }

        p->post_write_date.tv_sec = 0;
    }

#endif

    if (p->write_delay > 0)
    {
        int i;

        for (i = 0; i < count; i++)
        {
            ret = port_write(p, txbuffer + i, 1);

            if (ret != 1)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s():%d failed %d - %s\n",
                          __func__,
                          __LINE__,
                          ret,
                          strerror(errno));

                return -RIG_EIO;
            }

            hl_usleep(p->write_delay * 1000);
        }
    }
    else
    {
        ret = port_write(p, txbuffer, count);

        if (ret != count)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s():%d failed %d - %s\n",
                      __func__,
                      __LINE__,
                      ret,
                      strerror(errno));

            return -RIG_EIO;
        }
    }

    if (p->post_write_delay > 0)
    {
#ifdef WANT_NON_ACTIVE_POST_WRITE_DELAY
#define POST_WRITE_DELAY_TRSHLD 10

        if (p->post_write_delay > POST_WRITE_DELAY_TRSHLD)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            p->post_write_date.tv_sec = tv.tv_sec;
            p->post_write_date.tv_usec = tv.tv_usec;
        }
        else
#endif
            hl_usleep(p->post_write_delay * 1000); /* optional delay after last write */

        /* otherwise some yaesu rigs get confused */
        /* with sequential fast writes*/
    }

    rig_debug(RIG_DEBUG_TRACE, "%s(): TX %d bytes\n", __func__, (int)count);
    dump_hex((unsigned char *) txbuffer, count);

    return RIG_OK;
}


/**
 * \brief Read bytes from an fd
 * \param p rig port descriptor
 * \param rxbuffer buffer to receive text
 * \param count number of bytes
 * \return count of bytes received
 *
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 *
 * Blocks on read until timeout hits.
 *
 * It then reads "num" bytes into rxbuffer.
 *
 * Actually, this function has nothing specific to serial comm,
 * it could work very well also with any file handle, like a socket.
 */

int HAMLIB_API read_block(hamlib_port_t *p, char *rxbuffer, size_t count)
{
    fd_set rfds, efds;
    struct timeval tv, tv_timeout, start_time, end_time, elapsed_time;
    int total_count = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /*
     * Wait up to timeout ms.
     */
    tv_timeout.tv_sec = p->timeout / 1000;
    tv_timeout.tv_usec = (p->timeout % 1000) * 1000;

    /* Store the time of the read loop start */
    gettimeofday(&start_time, NULL);

    while (count > 0)
    {
        int retval;
        int rd_count;
        tv = tv_timeout;    /* select may have updated it */

        FD_ZERO(&rfds);
        FD_SET(p->fd, &rfds);
        efds = rfds;

        retval = port_select(p, p->fd + 1, &rfds, NULL, &efds, &tv);

        if (retval == 0)
        {
            /* Record timeout time and caculate elapsed time */
            gettimeofday(&end_time, NULL);
            timersub(&end_time, &start_time, &elapsed_time);

            dump_hex((unsigned char *) rxbuffer, total_count);
            rig_debug(RIG_DEBUG_WARN,
                      "%s(): Timed out %d.%d seconds after %d chars\n",
                      __func__,
                      (int)elapsed_time.tv_sec,
                      (int)elapsed_time.tv_usec,
                      total_count);

            return -RIG_ETIMEOUT;
        }

        if (retval < 0)
        {
            dump_hex((unsigned char *) rxbuffer, total_count);
            rig_debug(RIG_DEBUG_ERR,
                      "%s(): select() error after %d chars: %s\n",
                      __func__,
                      total_count,
                      strerror(errno));

            return -RIG_EIO;
        }

        if (FD_ISSET(p->fd, &efds))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s(): fd error after %d chars\n",
                      __func__,
                      total_count);

            return -RIG_EIO;
        }

        /*
         * grab bytes from the rig
         * The file descriptor must have been set up non blocking.
         */
        rd_count = port_read(p, rxbuffer + total_count, count);

        if (rd_count < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s(): read() failed - %s\n",
                      __func__,
                      strerror(errno));

            return -RIG_EIO;
        }

        total_count += rd_count;
        count -= rd_count;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s(): RX %d bytes\n", __func__, total_count);
    dump_hex((unsigned char *) rxbuffer, total_count);

    return total_count;           /* return bytes count read */
}


/**
 * \brief Read a string from an fd
 * \param p Hamlib port descriptor
 * \param rxbuffer buffer to receive string
 * \param rxmax maximum string size + 1
 * \param stopset string of recognized end of string characters
 * \param stopset_len length of stopset
 * \return number of characters read if the operation has been sucessful,
 * otherwise a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * Read a string from "fd" and put result into
 * an array of unsigned char pointed to by "rxbuffer"
 *
 * Blocks on read until timeout hits.
 *
 * It then reads characters until one of the characters in
 * "stopset" is found, or until "rxmax-1" characters was copied
 * into rxbuffer.  String termination character is added at the end.
 *
 * Actually, this function has nothing specific to serial comm,
 * it could work very well also with any file handle, like a socket.
 *
 * Assumes rxbuffer!=NULL
 */
int HAMLIB_API read_string(hamlib_port_t *p,
                           char *rxbuffer,
                           size_t rxmax,
                           const char *stopset,
                           int stopset_len)
{
    fd_set rfds, efds;
    struct timeval tv, tv_timeout, start_time, end_time, elapsed_time;
    int total_count = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (!p || !rxbuffer)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error p=%p, rxbuffer=%p\n", __func__, p,
                  rxbuffer);
        return -RIG_EINVAL;
    }

    if (rxmax < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error rxmax=%ld\n", __func__, (long)rxmax);
        return 0;
    }

    /*
     * Wait up to timeout ms.
     */
    tv_timeout.tv_sec = p->timeout / 1000;
    tv_timeout.tv_usec = (p->timeout % 1000) * 1000;

    /* Store the time of the read loop start */
    gettimeofday(&start_time, NULL);

    rxbuffer[0] = 0; /* ensure string is terminated */

    while (total_count < rxmax - 1)
    {
        int rd_count;
        int retval;
        tv = tv_timeout;    /* select may have updated it */

        FD_ZERO(&rfds);
        FD_SET(p->fd, &rfds);
        efds = rfds;

        retval = port_select(p, p->fd + 1, &rfds, NULL, &efds, &tv);

        if (retval == 0)
        {
            if (0 == total_count)
            {
                /* Record timeout time and caculate elapsed time */
                gettimeofday(&end_time, NULL);
                timersub(&end_time, &start_time, &elapsed_time);

                dump_hex((unsigned char *) rxbuffer, total_count);
                rig_debug(RIG_DEBUG_WARN,
                          "%s(): Timed out %d.%d seconds after %d chars\n",
                          __func__,
                          (int)elapsed_time.tv_sec,
                          (int)elapsed_time.tv_usec,
                          total_count);

                return -RIG_ETIMEOUT;
            }

            break;                      /* return what we have read */
        }

        if (retval < 0)
        {
            dump_hex((unsigned char *) rxbuffer, total_count);
            rig_debug(RIG_DEBUG_ERR,
                      "%s(): select() error after %d chars: %s\n",
                      __func__,
                      total_count,
                      strerror(errno));

            return -RIG_EIO;
        }

        if (FD_ISSET(p->fd, &efds))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s(): fd error after %d chars\n",
                      __func__,
                      total_count);

            return -RIG_EIO;
        }

        /*
             * read 1 character from the rig, (check if in stop set)
         * The file descriptor must have been set up non blocking.
         */
        rd_count = port_read(p, &rxbuffer[total_count], 1);

        /* if we get 0 bytes or an error something is wrong */
        if (rd_count <= 0)
        {
            dump_hex((unsigned char *) rxbuffer, total_count);
            rig_debug(RIG_DEBUG_ERR,
                      "%s(): read() failed - %s\n",
                      __func__,
                      strerror(errno));

            return -RIG_EIO;
        }

        ++total_count;

        if (stopset && memchr(stopset, rxbuffer[total_count - 1], stopset_len))
        {
            break;
        }
    }

    /*
     * Doesn't hurt anyway. But be aware, some binary protocols may have
     * null chars within the received buffer.
     */
    rxbuffer[total_count] = '\000';

    rig_debug(RIG_DEBUG_TRACE,
              "%s(): RX %d characters\n",
              __func__,
              total_count);

    dump_hex((unsigned char *) rxbuffer, total_count);

    return total_count;           /* return bytes count read */
}

/** @} */
