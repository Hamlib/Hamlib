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

#ifdef HAVE_CONFIG_H
#include "config.h"
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
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h> /* POSIX terminal control definitions */
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#else	/* sgtty */
#ifdef HAVE_SGTTY_H
#include <sgtty.h>
#endif
#endif
#endif

#if defined(WIN32) && !defined(HAVE_TERMIOS_H)
#include "win32termios.h"
#define HAVE_TERMIOS_H	1	/* we have replacement */
#else
#define OPEN open
#define CLOSE close
#define IOCTL ioctl
#endif

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"

#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif

/**
 * \brief Open serial port using rig.state data
 * \param rp port data structure (must spec port id eg /dev/ttyS1)
 * \return RIG_OK or < 0 if error
 */
int HAMLIB_API serial_open(hamlib_port_t *rp) {

  int fd;				/* File descriptor for the port */
  int err;

  if (!rp)
		  return -RIG_EINVAL;

  /*
   * Open in Non-blocking mode. Watch for EAGAIN errors!
   */
  fd = OPEN(rp->pathname, O_RDWR | O_NOCTTY | O_NDELAY);

  if (fd == -1) {

    /* Could not open the port. */

    rig_debug(RIG_DEBUG_ERR, "%s: Unable to open %s - %s\n",
						__func__, rp->pathname, strerror(errno));
    return -RIG_EIO;
  }

  rp->fd = fd;

  err = serial_setup(rp);
  if (err != RIG_OK) {
		  CLOSE(fd);
		  return err;
  }

  return RIG_OK;
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
  speed_t speed;			/* serial comm speed */
  struct termios options;
#elif defined(HAVE_TERMIO_H)
  struct termio options;
#elif defined(HAVE_SGTTY_H)
  struct sgttyb sg;
#else
#error "No term control supported!"
#endif

  if (!rp)
		  return -RIG_EINVAL;

  fd = rp->fd;

  /*
   * Get the current options for the port...
   */

#if defined(HAVE_TERMIOS_H)
  tcgetattr(fd, &options);
#elif defined(HAVE_TERMIO_H)
  IOCTL (fd, TCGETA, &options);
#else	/* sgtty */
  IOCTL (fd, TIOCGETP, &sg);
#endif

#ifdef HAVE_CFMAKERAW
  cfmakeraw(&options); /* Set serial port to RAW mode by default. */
#endif

  /*
   * Set the baud rates to requested values
   */

  switch(rp->parm.serial.rate) {
  case 150:
	speed = B150;		/* yikes... */
    break;
  case 300:
	speed = B300;		/* yikes... */
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
	speed = B57600;		/* cool.. */
    break;
  case 115200:
	speed = B115200;	/* awsome! */
    break;
  default:
    rig_debug(RIG_DEBUG_ERR, "%s: unsupported rate specified: %d\n",
					__func__, rp->parm.serial.rate);
	CLOSE(fd);
    return -RIG_ECONF;
  }
  /* TODO */
  cfsetispeed(&options, speed);
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

  switch(rp->parm.serial.data_bits) {
  case 7:
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS7;
    break;
  case 8:
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    break;
  default:
    rig_debug(RIG_DEBUG_ERR, "%s: unsupported serial_data_bits "
					"specified: %d\n", __func__, rp->parm.serial.data_bits);
	CLOSE(fd);
    return -RIG_ECONF;
    break;
  }

/*
 * Set stop bits to requested values.
 *
 */

  switch(rp->parm.serial.stop_bits) {
  case 1:
    options.c_cflag &= ~CSTOPB;
    break;
  case 2:
    options.c_cflag |= CSTOPB;
    break;

  default:
    rig_debug(RIG_DEBUG_ERR, "%s: unsupported serial_stop_bits "
					"specified: %d\n", __func__,
					rp->parm.serial.stop_bits);
	CLOSE(fd);
    return -RIG_ECONF;
    break;
  }

/*
 * Set parity to requested values.
 *
 */

  switch(rp->parm.serial.parity) {
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
    rig_debug(RIG_DEBUG_ERR, "%s: unsupported serial_parity "
					"specified: %d\n", __func__,
					rp->parm.serial.parity);
	CLOSE(fd);
    return -RIG_ECONF;
    break;
  }


/*
 * Set flow control to requested mode
 *
 */

  switch(rp->parm.serial.handshake) {
  case RIG_HANDSHAKE_NONE:
    options.c_cflag &= ~CRTSCTS;
    options.c_iflag &= ~IXON;
    break;
  case RIG_HANDSHAKE_XONXOFF:
    options.c_cflag &= ~CRTSCTS;
    options.c_iflag |= IXON;		/* Enable Xon/Xoff software handshaking */
    break;
  case RIG_HANDSHAKE_HARDWARE:
    options.c_cflag |= CRTSCTS;		/* Enable Hardware handshaking */
    options.c_iflag &= ~IXON;
    break;
  default:
    rig_debug(RIG_DEBUG_ERR, "%s: unsupported flow_control "
					"specified: %d\n", __func__,
					rp->parm.serial.handshake);
	CLOSE(fd);
    return -RIG_ECONF;
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

#else	/* sgtty */
  sg.sg_flags = RAW;
#endif

  /*
   * VTIME in deciseconds, rp->timeout in miliseconds
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
  if (tcsetattr(fd, TCSANOW, &options) == -1) {
		rig_debug(RIG_DEBUG_ERR, "%s: tcsetattr failed: %s\n",
					__func__, strerror(errno));
		CLOSE(fd);
		return -RIG_ECONF;		/* arg, so close! */
  }
#elif defined(HAVE_TERMIO_H)
  if (IOCTL(fd, TCSETA, &options) == -1) {
		rig_debug(RIG_DEBUG_ERR, "%s: ioctl(TCSETA) failed: %s\n",
					__func__, strerror(errno));
		CLOSE(fd);
		return -RIG_ECONF;		/* arg, so close! */
  }
#else	/* sgtty */
  if (IOCTL(fd, TIOCSETP, &sg) == -1) {
		rig_debug(RIG_DEBUG_ERR, "%s: ioctl(TIOCSETP) failed: %s\n",
					__func__, strerror(errno));
		CLOSE(fd);
		return -RIG_ECONF;		/* arg, so close! */
  }
#endif

  return RIG_OK;
}


/**
 * \brief Flush all characters waiting in RX buffer.
 * \param p
 * \return RIG_OK
 */
int HAMLIB_API serial_flush(hamlib_port_t *p )
{
  tcflush(p->fd, TCIFLUSH);

  return RIG_OK;
}

/**
 * \brief Open serial port
 * \param p
 * \return fd
 */
int ser_open(hamlib_port_t *p)
{
	return (p->fd = OPEN(p->pathname, O_RDWR | O_NOCTTY | O_NDELAY));
}

/**
 * \brief Close serial port
 * \param p fd
 * \return RIG_OK or < 0
 */
int ser_close(hamlib_port_t *p)
{
	return CLOSE(p->fd);
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

#if defined(TIOCMBIS) && defined(TIOCMBIC)
	return IOCTL(p->fd, state ? TIOCMBIS : TIOCMBIC, &y) < 0 ?
			-RIG_EIO : RIG_OK;
#else
	if (IOCTL(p->fd, TIOCMGET, &y) < 0) {
		return -RIG_EIO;
	}
	if (state)
		y |= TIOCM_RTS;
	else
		y &= ~TIOCM_RTS;
	return IOCTL(p->fd, TIOCMSET, &y) < 0 ? -RIG_EIO : RIG_OK;
#endif
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

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_RTS) == TIOCM_RTS;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
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

#if defined(TIOCMBIS) && defined(TIOCMBIC)
	return IOCTL(p->fd, state ? TIOCMBIS : TIOCMBIC, &y) < 0 ?
			-RIG_EIO : RIG_OK;
#else
	if (IOCTL(p->fd, TIOCMGET, &y) < 0) {
		return -RIG_EIO;
	}
	if (state)
		y |= TIOCM_DTR;
	else
		y &= ~TIOCM_DTR;
	return IOCTL(p->fd, TIOCMSET, &y) < 0 ? -RIG_EIO : RIG_OK;
#endif
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

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_DTR) == TIOCM_DTR;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
}

/**
 * \brief Set Break
 * \param p
 * \param state (ignored?)
 * \return RIG_OK or < 0
 */
int HAMLIB_API ser_set_brk(hamlib_port_t *p, int state)
{
#if defined(TIOCSBRK) && defined(TIOCCBRK)
	return IOCTL(p->fd, state ? TIOCSBRK : TIOCCBRK, 0 ) < 0 ?
			-RIG_EIO : RIG_OK;
#else
	return -RIG_ENIMPL;
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

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_CAR) == TIOCM_CAR;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
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

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_CTS) == TIOCM_CTS;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
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

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_DSR) == TIOCM_DSR;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
}

/** @} */
