/*
 *  Hamlib Interface - serial communication low-level support
 *  Copyright (c) 2000-2013 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
 *  Parts of the PTT handling are derived from soundmodem, an excellent
 *  ham packet softmodem written by Thomas Sailer, HB9JNX.
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
 * \addtogroup rig_internal
 * @{
 */

/**
 * \brief Serial port IO
 * \file serial.c
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/types.h>
#include <ctype.h>

#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#ifdef HAVE_TERMIOS_H
#  include <termios.h> /* POSIX terminal control definitions */
#else
#  ifdef HAVE_TERMIO_H
#    include <termio.h>
#  else   /* sgtty */
#    ifdef HAVE_SGTTY_H
#      include <sgtty.h>
#    endif
#  endif
#endif

//! @cond Doxygen_Suppress
#if defined(WIN32) && !defined(HAVE_TERMIOS_H)
#  include "win32termios.h"
#  define HAVE_TERMIOS_H  1   /* we have replacement */
#else
#  define OPEN open
#  define CLOSE close
#  define IOCTL ioctl
#endif
//! @endcond

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"

#ifdef HAVE_SYS_IOCCOM_H
#  include <sys/ioccom.h>
#endif

#include "microham.h"

static int uh_ptt_fd   = -1;
static int uh_radio_fd = -1;

//! @cond Doxygen_Suppress
typedef struct term_options_backup
{
    int fd;
#if defined(HAVE_TERMIOS_H)
    struct termios options;
#elif defined(HAVE_TERMIO_H)
    struct termio options;
#elif defined(HAVE_SGTTY_H)
    struct sgttyb sg;
#endif
    struct term_options_backup *next;
} term_options_backup_t;
//! @endcond
static term_options_backup_t *term_options_backup_head = NULL;


/*
 * This function simply returns TRUE if the argument matches uh_radio_fd and
 * is >= 0
 *
 * This function is only used in the WIN32 case and implements access "from
 * outside" to uh_radio_fd.
 */
//! @cond Doxygen_Suppress
int is_uh_radio_fd(int fd)
{
    if (uh_radio_fd >= 0 && uh_radio_fd == fd)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
//! @endcond


/**
 * \brief Open serial port using rig.state data
 * \param rp port data structure (must spec port id eg /dev/ttyS1)
 * \return RIG_OK or < 0 if error
 */
int HAMLIB_API serial_open(hamlib_port_t *rp)
{

    int fd;               /* File descriptor for the port */
    int err;

    if (!rp)
    {
        return (-RIG_EINVAL);
    }


    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, rp->pathname);

    if (!strncmp(rp->pathname, "uh-rig", 6))
    {
        /*
         * If the pathname is EXACTLY "uh-rig", try to use a microHam device
         * rather than a conventional serial port.
         * The microHam devices ALWAYS use "no parity", and can either use no handshake
         * or hardware handshake. Return with error if something else is requested.
         */
        if (rp->parm.serial.parity != RIG_PARITY_NONE)
        {
            return (-RIG_EIO);
        }

        if ((rp->parm.serial.handshake != RIG_HANDSHAKE_HARDWARE) &&
                (rp->parm.serial.handshake != RIG_HANDSHAKE_NONE))
        {
            return (-RIG_EIO);
        }

        /*
         * Note that serial setup is also don in uh_open_radio.
         * So we need to dig into serial_setup().
         */
        fd = uh_open_radio(
                 rp->parm.serial.rate,                                  // baud
                 rp->parm.serial.data_bits,                              // databits
                 rp->parm.serial.stop_bits,                              // stopbits
                 (rp->parm.serial.handshake == RIG_HANDSHAKE_HARDWARE)); // rtscts

        if (fd == -1)
        {
            return (-RIG_EIO);
        }

        rp->fd = fd;
        /*
         * Remember the fd in a global variable. We can do read(), write() and select()
         * on fd but whenever it is tried to do an ioctl(), we have to catch it
         * (e.g. setting DTR or tcflush on this fd does not work)
         * While this may look dirty, it is certainly easier and more efficient than
         * to check whether fd corresponds to a serial line or a socket everywhere.
         *
         * CAVEAT: for WIN32, it might be necessary to use win_serial_read() instead
         *         of read() for serial lines in iofunc.c. Therefore, we have to
         *         export uh_radio_fd to iofunc.c because in the case of sockets,
         *         read() must be used also in the WIN32 case.
         *         This is why uh_radio_fd is declared globally in microham.h.
         * Notes from Joe Subich about microham behavior
         * Microham debug tags
         * A-RX ; Asynchronous data received (data not responsive to any
         *        poll from host or Router) - Set AI0;
         * H2-RX;  Data received in response to poll from Host 2 poll (2nd CAT port).
         * H2-TX;  Data poll/command from Host 2 (2nd CAT Port)
         * R-RX;  Data received in response to poll by microHAM USB Device Router
         * R-TX;  Router poll
         * Note: R-TX; and R-RX; data is not passed to Host ports.
         * 1) Router only polls when it has not seen a poll for FA; FB; and IF;
         *    (or equivalent for other manufacturers) within its timeout period.
         * 2) The results of router's polling are not passed to the Host/apps.
         * 3) Router only polls when there is no activity from the applications.
         * 4) Router is designed to be transparent as far as the applications
         *    are concerned.  The only exception is when the user chooses to
         *    run two applications (CAT and 2nd CAT) at the same time and has
         *    "auto-information" or CI-V enabled.  In that case asynchronous data
         *    from the transceiver will be returned to both applications.
         */
        uh_radio_fd = fd;
        return (RIG_OK);
    }

    /*
     * Open in Non-blocking mode. Watch for EAGAIN errors!
     */
    int i = 1;

    do   // some serial ports fail to open 1st time for some unknown reason
    {
        fd = OPEN(rp->pathname, O_RDWR | O_NOCTTY | O_NDELAY);

        if (fd == -1) // some serial ports fail to open 1st time for some unknown reason
        {
            rig_debug(RIG_DEBUG_WARN, "%s(%d): open failed#%d\n", __func__, __LINE__, i);
            hl_usleep(500 * 1000);
            fd = OPEN(rp->pathname, O_RDWR | O_NOCTTY | O_NDELAY);
        }
    }
    while (++i <= 4 && fd == -1);

    if (fd == -1)
    {
        /* Could not open the port. */
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Unable to open %s - %s\n",
                  __func__,
                  rp->pathname,
                  strerror(errno));
        return (-RIG_EIO);
    }

    rp->fd = fd;

    err = serial_setup(rp);

    if (err != RIG_OK)
    {
        CLOSE(fd);
        return (err);
    }

    serial_flush(rp); // ensure nothing is there when we open
    hl_usleep(50 * 1000); // give a little time for MicroKeyer to finish

    return (RIG_OK);
}


/**
 * \brief Set up Serial port according to requests in port
 * \param rp
 * \return RIG_OK or < 0
 */
int HAMLIB_API serial_setup(hamlib_port_t *rp)
{
    int fd;
    /* There's a lib replacement for termios under Mingw */
#if defined(HAVE_TERMIOS_H)
    speed_t speed;            /* serial comm speed */
    struct termios options, orig_options;
#elif defined(HAVE_TERMIO_H)
    struct termio options, orig_options;
#elif defined(HAVE_SGTTY_H)
    struct sgttyb sg, orig_sg;
#else
#  error "No term control supported!"
#endif
    term_options_backup_t *term_backup = NULL;

    if (!rp)
    {
        return (-RIG_EINVAL);
    }

    fd = rp->fd;

    // Linux sets pins high so we force them low once
    // This fails on Linux and MacOS with hardware flow control
    // Seems setting low disables hardware flow setting later
//    ser_set_rts(rp, 0);
//    ser_set_dtr(rp, 0);

    /*
     * Get the current options for the port...
     */
#if defined(HAVE_TERMIOS_H)
    rig_debug(RIG_DEBUG_TRACE, "%s: tcgetattr\n", __func__);
    tcgetattr(fd, &options);
    memcpy(&orig_options, &options, sizeof(orig_options));
#elif defined(HAVE_TERMIO_H)
    rig_debug(RIG_DEBUG_TRACE, "%s: IOCTL TCGETA\n", __func__);
    IOCTL(fd, TCGETA, &options);
    memcpy(&orig_options, &options, sizeof(orig_options));
#else   /* sgtty */
    rig_debug(RIG_DEBUG_TRACE, "%s: IOCTL TIOCGETP\n", __func__);
    IOCTL(fd, TIOCGETP, &sg);
    memcpy(&orig_sg, &sg, sizeof(orig_sg));
#endif

#ifdef HAVE_CFMAKERAW
    /* Set serial port to RAW mode by default. */
    rig_debug(RIG_DEBUG_TRACE, "%s: cfmakeraw\n", __func__);
    cfmakeraw(&options);
#endif

    /*
     * Set the baud rates to requested values
     */
    switch (rp->parm.serial.rate)
    {
    case 150:
        speed = B150;       /* yikes... */
        break;

    case 300:
        speed = B300;       /* yikes... */
        break;

    case 600:
        speed = B600;
        break;

    case 1200:
        speed = B1200;
        break;

    case 2400:
        speed = B2400;
        break;

    case 4800:
        speed = B4800;
        break;

    case 9600:
        speed = B9600;
        break;

    case 19200:
        speed = B19200;
        break;

    case 38400:
        speed = B38400;
        break;

    case 57600:
        speed = B57600;     /* cool.. */
        break;

    case 115200:
        speed = B115200;    /* awesome! */
        break;

#ifdef B230400

    case 230400:
        speed = B230400;    /* super awesome! */
        break;
#endif

#ifdef B500000

    case 500000:
        speed = B500000;    /* extra super awesome! */
        break;
#endif

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported rate specified: %d\n",
                  __func__,
                  rp->parm.serial.rate);
        CLOSE(fd);

        return (-RIG_ECONF);
    }

    /* TODO */
    rig_debug(RIG_DEBUG_TRACE, "%s: cfsetispeed=%d,0x%04x\n", __func__,
              (int)rp->parm.serial.rate, (int)speed);
    cfsetispeed(&options, speed);
    rig_debug(RIG_DEBUG_TRACE, "%s: cfsetospeed=%d,0x%04x\n", __func__,
              (int)rp->parm.serial.rate, (int)speed);
    cfsetospeed(&options, speed);

    /*
     * Enable the receiver and set local mode...
     */
    options.c_cflag |= (CLOCAL | CREAD);

    /*
     * close doesn't change modem signals
     */
    options.c_cflag &= ~HUPCL;

    /*
     * Set data to requested values.
     *
     */
    rig_debug(RIG_DEBUG_TRACE, "%s: data_bits=%d\n", __func__,
              rp->parm.serial.data_bits);

    switch (rp->parm.serial.data_bits)
    {
    case 7:
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS7;
        break;

    case 8:
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported serial_data_bits specified: %d\n",
                  __func__,
                  rp->parm.serial.data_bits);
        CLOSE(fd);

        return (-RIG_ECONF);
        break;
    }

    /*
     * Set stop bits to requested values.
     *
     */
    switch (rp->parm.serial.stop_bits)
    {
    case 1:
        options.c_cflag &= ~CSTOPB;
        break;

    case 2:
        options.c_cflag |= CSTOPB;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported serial_stop_bits specified: %d\n",
                  __func__,
                  rp->parm.serial.stop_bits);
        CLOSE(fd);

        return (-RIG_ECONF);
        break;
    }

    /*
     * Set parity to requested values.
     *
     */
    rig_debug(RIG_DEBUG_TRACE, "%s: parity=%d\n", __func__, rp->parm.serial.parity);

    switch (rp->parm.serial.parity)
    {
    case RIG_PARITY_NONE:
        options.c_cflag &= ~PARENB;
        break;

    case RIG_PARITY_EVEN:
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        break;

    case RIG_PARITY_ODD:
        options.c_cflag |= PARENB;
        options.c_cflag |= PARODD;
        break;
        /* CMSPAR is not POSIX */
#ifdef CMSPAR

    case RIG_PARITY_MARK:
        options.c_cflag |= PARENB | CMSPAR;
        options.c_cflag |= PARODD;
        break;

    case RIG_PARITY_SPACE:
        options.c_cflag |= PARENB | CMSPAR;
        options.c_cflag &= ~PARODD;
        break;
#endif

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported serial_parity specified: %d\n",
                  __func__,
                  rp->parm.serial.parity);
        CLOSE(fd);

        return (-RIG_ECONF);
        break;
    }


    /*
     * Set flow control to requested mode
     *
     */
    switch (rp->parm.serial.handshake)
    {
    case RIG_HANDSHAKE_NONE:
        rig_debug(RIG_DEBUG_TRACE, "%s: Handshake=None\n", __func__);
        options.c_cflag &= ~CRTSCTS;
        options.c_iflag &= ~IXON;
        break;

    case RIG_HANDSHAKE_XONXOFF:
        rig_debug(RIG_DEBUG_TRACE, "%s: Handshake=Xon/Xoff\n", __func__);
        options.c_cflag &= ~CRTSCTS;
        options.c_iflag |= IXON;        /* Enable Xon/Xoff software handshaking */
        break;

    case RIG_HANDSHAKE_HARDWARE:
        rig_debug(RIG_DEBUG_TRACE, "%s: Handshake=Hardware\n", __func__);
        options.c_cflag |= CRTSCTS;     /* Enable Hardware handshaking */
        options.c_iflag &= ~IXON;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported flow_control specified: %d\n",
                  __func__,
                  rp->parm.serial.handshake);
        CLOSE(fd);

        return (-RIG_ECONF);
        break;
    }

    /*
     * Choose raw input, no preprocessing please ..
     */
#if defined(HAVE_TERMIOS_H) || defined(HAVE_TERMIO_H)
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    /*
     * Choose raw output, no preprocessing please ..
     */
    options.c_oflag &= ~OPOST;

#else   /* sgtty */
    sg.sg_flags = RAW;
#endif

    /*
     * VTIME in deciseconds, rp->timeout in milliseconds
     */
    options.c_cc[VTIME] = (rp->timeout + 99) / 100;
    options.c_cc[VMIN] = 1;

    /*
     * Flush serial port
     */
    tcflush(fd, TCIFLUSH);

    /*
     * Finally, set the new options for the port...
     */
#if defined(HAVE_TERMIOS_H)

    rig_debug(RIG_DEBUG_TRACE, "%s: tcsetattr TCSANOW\n", __func__);

    if (tcsetattr(fd, TCSANOW, &options) == -1)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: tcsetattr failed: %s\n",
                  __func__,
                  strerror(errno));
        CLOSE(fd);

        return (-RIG_ECONF);      /* arg, so close! */
    }

#elif defined(HAVE_TERMIO_H)

    rig_debug(RIG_DEBUG_TRACE, "%s: IOCTL TCSETA\n", __func__);

    if (IOCTL(fd, TCSETA, &options) == -1)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: ioctl(TCSETA) failed: %s\n",
                  __func__,
                  strerror(errno));
        CLOSE(fd);

        return (-RIG_ECONF);      /* arg, so close! */
    }

#else

    rig_debug(RIG_DEBUG_TRACE, "%s: IOCTL TIOCSETP\n", __func__);

    /* sgtty */
    if (IOCTL(fd, TIOCSETP, &sg) == -1)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: ioctl(TIOCSETP) failed: %s\n",
                  __func__,
                  strerror(errno));
        CLOSE(fd);

        return (-RIG_ECONF);      /* arg, so close! */
    }

#endif

    // Store a copy of the original options for this FD, to be restored on close.
    term_backup = calloc(1, sizeof(term_options_backup_t));
    term_backup-> fd = fd;
#if defined(HAVE_TERMIOS_H) || defined(HAVE_TERMIO_H)
    memcpy(&term_backup->options, &orig_options, sizeof(orig_options));
#elif defined(HAVE_SGTTY_H)
    memcpy(&term_backup->sg, &orig_sg, sizeof(orig_sg));
#endif

    // insert at head of list
    term_backup->next = term_options_backup_head;
    term_options_backup_head = term_backup;

    return (RIG_OK);
}


/**
 * \brief Flush all characters waiting in RX buffer.
 * \param p
 * \return RIG_OK
 */
int HAMLIB_API serial_flush(hamlib_port_t *p)
{
    int len;
    int timeout_save;
    unsigned char buf[4096];

    if (p->fd == uh_ptt_fd || p->fd == uh_radio_fd || p->flushx)
    {
        /*
         * Catch microHam case:
         * if fd corresponds to a microHam device drain the line
         * (which is a socket) by reading until it is empty.
         */
        int n, nbytes = 0;

        rig_debug(RIG_DEBUG_TRACE, "%s: flushing\n", __func__);

        while ((n = read(p->fd, buf, sizeof(buf))) > 0)
        {
            nbytes += n;
            //int i;

            //for (i = 0; i < n; ++i) { printf("0x%02x(%c) ", buf[i], isprint(buf[i]) ? buf[i] : '~'); }

            /* do nothing */
        }

        rig_debug(RIG_DEBUG_TRACE, "read flushed %d bytes\n", nbytes);


        return (RIG_OK);
    }

    timeout_save = p->timeout;
    p->timeout = 1;

    do
    {
        // we pass an empty stopset so read_string can determine
        // the appropriate stopset for async data
        char stopset[1];
        len = read_string(p, buf, sizeof(buf) - 1, stopset, 0, 1, 1);

        if (len > 0)
        {
            int i, binary = 0;

            for (i = 0; i < len; ++i)
            {
                if (!isprint(buf[i])) { binary = 1; }
            }

            if (binary)
            {
                int bytes = len * 3 + 1;
                char *hbuf = calloc(bytes, 1);

                for (i = 0; i < len; ++i) { SNPRINTF(&hbuf[i * 3], bytes - (i * 3),  "%02X ", buf[i]); }

                rig_debug(RIG_DEBUG_WARN, "%s: flush hex:%s\n", __func__, hbuf);
                free(hbuf);
            }
            else
            {
                rig_debug(RIG_DEBUG_WARN, "%s: flush string:%s\n", __func__, buf);
            }
        }
    }
    while (len > 0);

    p->timeout = timeout_save;
    //rig_debug(RIG_DEBUG_VERBOSE, "tcflush%s\n", "");
    //tcflush(p->fd, TCIFLUSH);
    return (RIG_OK);
}


/**
 * \brief Open serial port
 * \param p
 * \return fd
 */
int ser_open(hamlib_port_t *p)
{
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called port=%s\n", __func__, p->pathname);

    if (!strncmp(p->pathname, "uh-rig", 6))
    {
        /*
         * This should not happen: ser_open is only used for
         * DTR-only serial ports (ptt_pathname != rig_pathname).
         */
        ret = -1;
    }
    else
    {
        if (!strncmp(p->pathname, "uh-ptt", 6))
        {
            /*
             * Use microHam device for doing PTT. Although a valid file
             * descriptor is returned, it is not used for anything
             * but must be remembered in a global variable:
             * If it is tried later to set/unset DTR on this fd, we know
             * that we cannot use ioctl and must rather call our
             * PTT set/unset service routine.
             */
            ret = uh_open_ptt();
            uh_ptt_fd = ret;
        }
        else
        {
            /*
             * pathname is not uh_rig or uh_ptt: simply open()
             */
            int i = 1;

            do // some serial ports fail to open 1st time
            {
                ret = OPEN(p->pathname, O_RDWR | O_NOCTTY | O_NDELAY);

                if (ret == -1) // some serial ports fail to open 1st time
                {
                    rig_debug(RIG_DEBUG_WARN, "%s(%d): open failed#%d\n", __func__, __LINE__, i);
                    hl_usleep(500 * 1000);
                    ret = OPEN(p->pathname, O_RDWR | O_NOCTTY | O_NDELAY);
                }
            }
            while (++i <= 4 && ret == -1);
        }
    }

    p->fd = ret;
    return (ret);
}


/**
 * \brief Close serial port
 * \param p fd
 * \return RIG_OK or < 0
 */
int ser_close(hamlib_port_t *p)
{
    int rc;
    term_options_backup_t *term_backup, *term_backup_prev;

    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /*
     * For microHam devices, do not close the
     * socket via close but call a service routine
     * (which might decide to keep the socket open).
     * However, unset p->fd and uh_ptt_fd/uh_radio_fd.
     */
    if (p->fd == uh_ptt_fd)
    {
        uh_close_ptt();
        uh_ptt_fd = -1;
        p->fd = -1;
        return (0);
    }

    if (p->fd == uh_radio_fd)
    {
        uh_close_radio();
        uh_radio_fd = -1;
        p->fd = -1;
        return (0);
    }

    // Find backup termios options to restore before closing
    term_backup = term_options_backup_head;
    term_backup_prev = term_options_backup_head;

    while (term_backup)
    {
        if (term_backup->fd == p->fd)
        {
            // Found matching. Remove from list
            if (term_backup == term_options_backup_head)
            {
                term_options_backup_head = term_backup->next;
            }
            else
            {
                term_backup_prev->next = term_backup->next;
            }

            break;
        }

        term_backup_prev = term_backup;
        term_backup = term_backup->next;
    }

    // Restore backup termios
    if (term_backup)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: restoring options\n", __func__);
#if defined(HAVE_TERMIOS_H)

        if (tcsetattr(p->fd, TCSANOW, &term_backup->options) == -1)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: tcsetattr restore failed: %s\n",
                      __func__,
                      strerror(errno));
        }

#elif defined(HAVE_TERMIO_H)

        if (IOCTL(p->fd, TCSETA, &term_backup->options) == -1)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: ioctl(TCSETA) restore failed: %s\n",
                      __func__,
                      strerror(errno));
        }

#else

        /* sgtty */
        if (IOCTL(p->fd, TIOCSETP, &term_backup->sg) == -1)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: ioctl(TIOCSETP) restore failed: %s\n",
                      __func__,
                      strerror(errno));
        }

#endif

        free(term_backup);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: no options for fd to restore\n", __func__);
    }

    rc = CLOSE(p->fd);
    p->fd = -1;
    return (rc);
}


/**
 * \brief Set Request to Send (RTS) bit
 * \param p
 * \param state true/false
 * \return RIG_OK or < 0
 */
int HAMLIB_API ser_set_rts(hamlib_port_t *p, int state)
{
    unsigned int y = TIOCM_RTS;
    int rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: RTS=%d\n", __func__, state);

    // ignore this for microHam ports
    if (p->fd == uh_ptt_fd || p->fd == uh_radio_fd)
    {
        return (RIG_OK);
    }

#if defined(TIOCMBIS) && defined(TIOCMBIC)
    rc = IOCTL(p->fd, state ? TIOCMBIS : TIOCMBIC, &y);
#else
    rc = IOCTL(p->fd, TIOCMGET, &y);

    if (rc >= 0)
    {
        if (state)
        {
            y |= TIOCM_RTS;
        }
        else
        {
            y &= ~TIOCM_RTS;
        }

        rc = IOCTL(p->fd, TIOCMSET, &y);
    }

#endif

    if (rc < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Cannot change RTS - %s\n",
                  __func__,
                  strerror(errno));
        return (-RIG_EIO);
    }

    return (RIG_OK);
}


/**
 * \brief Get RTS bit
 * \param p supposed to be &rig->state.rigport
 * \param state non-NULL
 */
int HAMLIB_API ser_get_rts(hamlib_port_t *p, int *state)
{
    int retcode;
    unsigned int y;

    // cannot do this for microHam ports
    if (p->fd == uh_ptt_fd || p->fd == uh_radio_fd)
    {
        return (-RIG_ENIMPL);
    }

    retcode = IOCTL(p->fd, TIOCMGET, &y);
    *state = (y & TIOCM_RTS) == TIOCM_RTS;

    return (retcode < 0 ? -RIG_EIO : RIG_OK);
}


/**
 * \brief Set Data Terminal Ready (DTR) bit
 * \param p
 * \param state true/false
 * \return RIG_OK or < 0
 */
int HAMLIB_API ser_set_dtr(hamlib_port_t *p, int state)
{
    unsigned int y = TIOCM_DTR;
    int rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: DTR=%d\n", __func__, state);

    // silently ignore on microHam RADIO channel,
    // but (un)set ptt on microHam PTT channel.
    if (p->fd == uh_radio_fd)
    {
        return (RIG_OK);
    }

    if (p->fd == uh_ptt_fd)
    {
        uh_set_ptt(state);
        return (RIG_OK);
    }

#if defined(TIOCMBIS) && defined(TIOCMBIC)
    rc = IOCTL(p->fd, state ? TIOCMBIS : TIOCMBIC, &y);
#else
    rc = IOCTL(p->fd, TIOCMGET, &y);

    if (rc >= 0)
    {
        if (state)
        {
            y |= TIOCM_DTR;
        }
        else
        {
            y &= ~TIOCM_DTR;
        }

        rc = IOCTL(p->fd, TIOCMSET, &y);
    }

#endif

    if (rc < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Cannot change DTR - %s\n",
                  __func__,
                  strerror(errno));
        return (-RIG_EIO);
    }

    return (RIG_OK);
}


/**
 * \brief Get DTR bit
 * \param p supposed to be &rig->state.rigport
 * \param state non-NULL
 */
int HAMLIB_API ser_get_dtr(hamlib_port_t *p, int *state)
{
    int retcode;
    unsigned int y;

    // cannot do this for the RADIO port, return PTT state for the PTT port
    if (p->fd == uh_ptt_fd)
    {
        *state = uh_get_ptt();
        return (RIG_OK);
    }

    if (p->fd == uh_radio_fd)
    {
        return (-RIG_ENIMPL);
    }

    retcode = IOCTL(p->fd, TIOCMGET, &y);
    *state = (y & TIOCM_DTR) == TIOCM_DTR;

    return (retcode < 0 ? -RIG_EIO : RIG_OK);
}


/**
 * \brief Set Break
 * \param p
 * \param state (ignored?)
 * \return RIG_OK or < 0
 */
int HAMLIB_API ser_set_brk(hamlib_port_t *p, int state)
{
    // ignore this for microHam ports
    if (p->fd == uh_ptt_fd || p->fd == uh_radio_fd)
    {
        return (RIG_OK);
    }

#if defined(TIOCSBRK) && defined(TIOCCBRK)
    return (IOCTL(p->fd, state ? TIOCSBRK : TIOCCBRK, 0) < 0 ?
            -RIG_EIO : RIG_OK);
#else
    return (-RIG_ENIMPL);
#endif
}


/**
 * \brief Get Carrier (CI?) bit
 * \param p supposed to be &rig->state.rigport
 * \param state non-NULL
 */
int HAMLIB_API ser_get_car(hamlib_port_t *p, int *state)
{
    int retcode;
    unsigned int y;

    // cannot do this for microHam ports
    if (p->fd == uh_ptt_fd || p->fd == uh_radio_fd)
    {
        return (-RIG_ENIMPL);
    }

    retcode = IOCTL(p->fd, TIOCMGET, &y);
    *state = (y & TIOCM_CAR) == TIOCM_CAR;

    return (retcode < 0 ? -RIG_EIO : RIG_OK);
}


/**
 * \brief Get Clear to Send (CTS) bit
 * \param p supposed to be &rig->state.rigport
 * \param state non-NULL
 */
int HAMLIB_API ser_get_cts(hamlib_port_t *p, int *state)
{
    int retcode;
    unsigned int y;

    // cannot do this for microHam ports
    if (p->fd == uh_ptt_fd || p->fd == uh_radio_fd)
    {
        return (-RIG_ENIMPL);
    }

    retcode = IOCTL(p->fd, TIOCMGET, &y);
    *state = (y & TIOCM_CTS) == TIOCM_CTS;

    return (retcode < 0 ? -RIG_EIO : RIG_OK);
}


/**
 * \brief Get Data Set Ready (DSR) bit
 * \param p supposed to be &rig->state.rigport
 * \param state non-NULL
 */
int HAMLIB_API ser_get_dsr(hamlib_port_t *p, int *state)
{
    int retcode;
    unsigned int y;

    // cannot do this for microHam ports
    if (p->fd == uh_ptt_fd || p->fd == uh_radio_fd)
    {
        return (-RIG_ENIMPL);
    }

    retcode = IOCTL(p->fd, TIOCMGET, &y);
    *state = (y & TIOCM_DSR) == TIOCM_DSR;

    return (retcode < 0 ? -RIG_EIO : RIG_OK);
}

/** @} */
