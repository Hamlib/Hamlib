/*
 *  Hamlib Interface - serial communication low-level support
 *  Copyright (c) 2000-2004 by Stephane Fillod and Frank Singleton
 *  Parts of the PTT handling are derived from soundmodem, an excellent
 *  ham packet softmodem written by Thomas Sailer, HB9JNX.
 *
 *	$Id: serial.c,v 1.42 2004-10-02 20:18:16 fillods Exp $
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

/* for CTS/RTS and DTR/DSR control under Win32 --SF */
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#include "par_nt.h"
#endif
#ifdef HAVE_WINIOCTL_H
#include <winioctl.h>
#endif
#ifdef HAVE_WINBASE_H
#include <winbase.h>
#endif

#if defined(WIN32) && !defined(HAVE_TERMIOS_H)
#include "win32termios.h"
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

#ifdef HAVE_LINUX_PPDEV_H
#include <linux/ppdev.h>
#include <linux/parport.h>

/* 
 * These control port bits are active low.
 * We toggle them so that this weirdness doesn't get get propagated
 * through our interface.
 */
#define CP_ACTIVE_LOW_BITS	0x0B

/*
 * These status port bits are active low.
 * We toggle them so that this weirdness doesn't get get propagated
 * through our interface.
 */
#define SP_ACTIVE_LOW_BITS	0x80

#endif


/*
 * Open serial port using rig.state data
 *
 *
 * input: ptr to rig.state structure, with serial port
 *        populated. (eg: "/dev/ttyS1").
 * 
 * returns:
 *
 * 0 if OK or negative value on error.
 *
 */

int HAMLIB_API serial_open(port_t *rp) {

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
    
    rig_debug(RIG_DEBUG_ERR, "serial_open: Unable to open %s - %s\n", 
						rp->pathname, strerror(errno));
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


int HAMLIB_API serial_setup(port_t *rp)
{
  int fd;
  /* There's a lib replacement for termios under Mingw */
#if defined(HAVE_TERMIOS_H) || defined(WIN32)
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
  
#if defined(HAVE_TERMIOS_H) || defined(WIN32)
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
  case 300:
	speed = B300;		/* yikes... */
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
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported rate specified: %d\n", 
					rp->parm.serial.rate);
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
    rig_debug(RIG_DEBUG_ERR,"open_serial: unsupported serial_data_bits "
					"specified: %d\n", rp->parm.serial.data_bits);
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
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported serial_stop_bits "
					"specified: %d\n",
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
  default:
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported serial_parity "
					"specified: %d\n",
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
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported flow_control "
					"specified: %d\n",
					rp->parm.serial.handshake);
	CLOSE(fd);
    return -RIG_ECONF;
    break;
  }

  /*
   * Choose raw input, no preprocessing please ..
   */

#if defined(HAVE_TERMIOS_H) || defined(HAVE_TERMIO_H) || defined(WIN32)
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

  /*
   * Choose raw output, no preprocessing please ..
   */

  options.c_oflag &= ~OPOST;

#else	/* sgtty */
  sg.sg_flags = RAW;
#endif


  /*
   * Flush serial port
   */

  tcflush(fd, TCIFLUSH);

  /*
   * Finally, set the new options for the port...
   */
  
#if defined(HAVE_TERMIOS_H) || defined(WIN32)
  if (tcsetattr(fd, TCSANOW, &options) == -1) {
		rig_debug(RIG_DEBUG_ERR, "open_serial: tcsetattr failed: %s\n", 
					strerror(errno));
		CLOSE(fd);
		return -RIG_ECONF;		/* arg, so close! */
  }
#elif defined(HAVE_TERMIO_H)
  if (IOCTL(fd, TCSETA, &options) == -1) {
		rig_debug(RIG_DEBUG_ERR, "open_serial: ioctl(TCSETA) failed: %s\n", 
					strerror(errno));
		CLOSE(fd);
		return -RIG_ECONF;		/* arg, so close! */
  }
#else	/* sgtty */
  if (IOCTL(fd, TIOCSETP, &sg) == -1) {
		rig_debug(RIG_DEBUG_ERR, "open_serial: ioctl(TIOCSETP) failed: %s\n", 
					strerror(errno));
		CLOSE(fd);
		return -RIG_ECONF;		/* arg, so close! */
  }
#endif

  return RIG_OK;
}


/*
 * Flush all characters waiting in RX buffer.
 */

int HAMLIB_API serial_flush( port_t *p )
{
  tcflush(p->fd, TCIFLUSH);

  return RIG_OK;
}

int ser_open(port_t *p)
{
	return (p->fd = OPEN(p->pathname, O_RDWR | O_NOCTTY | O_NDELAY));
}

int ser_close(port_t *p)
{
	return CLOSE(p->fd);
}

int HAMLIB_API ser_set_rts(port_t *p, int state)
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

/*
 * assumes state not NULL
 * p is supposed to be &rig->state.rigport
 */
int HAMLIB_API ser_get_rts(port_t *p, int *state)
{
  int retcode;
  unsigned int y;

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_RTS) == TIOCM_RTS;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
}

int HAMLIB_API ser_set_dtr(port_t *p, int state)
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

int HAMLIB_API ser_get_dtr(port_t *p, int *state)
{
  int retcode;
  unsigned int y;

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_DTR) == TIOCM_DTR;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
}

int HAMLIB_API ser_set_brk(port_t *p, int state)
{
#if defined(TIOCSBRK) && defined(TIOCCBRK)
	return IOCTL(p->fd, state ? TIOCSBRK : TIOCCBRK, 0 ) < 0 ?
			-RIG_EIO : RIG_OK;
#else
	return -RIG_ENIMPL;
#endif
}

/*
 * assumes state not NULL
 * p is supposed to be &rig->state.rigport
 */
int HAMLIB_API ser_get_car(port_t *p, int *state)
{
  int retcode;
  unsigned int y;

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_CAR) == TIOCM_CAR;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
}

int HAMLIB_API ser_get_cts(port_t *p, int *state)
{
  int retcode;
  unsigned int y;

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_CTS) == TIOCM_CTS;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
}

int HAMLIB_API ser_get_dsr(port_t *p, int *state)
{
  int retcode;
  unsigned int y;

  retcode = IOCTL(p->fd, TIOCMGET, &y);
  *state = (y & TIOCM_DSR) == TIOCM_DSR;

  return retcode < 0 ? -RIG_EIO : RIG_OK;
}



/*
 * TODO: to be called before exiting: atexit(parport_cleanup)
 * void parport_cleanup() { ioctl(fd, PPRELEASE); }
 */

int par_open(port_t *port)
{
	int fd;
	int mode;

	if (!port->pathname)
		return -RIG_EINVAL;

#ifdef HAVE_LINUX_PPDEV_H
	fd = open(port->pathname, O_RDWR);
	if (fd < 0) {
		rig_debug(RIG_DEBUG_ERR, "Opening device \"%s\": %s\n", port->pathname, strerror(errno));
		return -RIG_EIO;
	}
	mode = IEEE1284_MODE_COMPAT;
	if (ioctl (fd, PPSETMODE, &mode) != 0) {
		rig_debug(RIG_DEBUG_ERR, "PPSETMODE \"%s\": %s\n", port->pathname, strerror(errno));
		close(fd);
		return -RIG_EIO;
	}

#elif defined(WIN32)
	fd = (int)CreateFile(port->pathname, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);
	if (fd == (int)INVALID_HANDLE_VALUE) {
		rig_debug(RIG_DEBUG_ERR, "Opening device \"%s\"\n", port->pathname);
		CloseHandle((HANDLE)fd);
		return -RIG_EIO;
	}
#else
	return -RIG_ENIMPL;
#endif
	port->fd = fd;
	return fd;
}

int par_close(port_t *port)
{
#ifdef HAVE_LINUX_PPDEV_H
#elif defined(WIN32)
	CloseHandle((HANDLE)(port->fd));
	return RIG_OK;
#endif
	return close(port->fd);
}

int HAMLIB_API par_write_data(port_t *port, unsigned char data)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	status = ioctl(port->fd, PPWDATA, &data);
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(WIN32)
	unsigned int dummy;

	if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_DATA, &data, sizeof(data), 
			NULL, 0, (LPDWORD)&dummy, NULL))) {
		rig_debug(RIG_DEBUG_ERR, "%s: DeviceIoControl failed!\n", __FUNCTION__);
	}
#endif
	return -RIG_ENIMPL;
}

int HAMLIB_API par_read_data(port_t *port, unsigned char *data)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	status = ioctl(port->fd, PPRDATA, data);
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(WIN32)
	char ret;
	unsigned int dummy;

	if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_STATUS, NULL, 0, &ret, 
          		sizeof(ret), (LPDWORD)&dummy, NULL))) {
		rig_debug(RIG_DEBUG_ERR, "%s: DeviceIoControl failed!\n", __FUNCTION__);
	}

  	return ret ^ S1284_INVERTED;
#endif
	return -RIG_ENIMPL;
}


int HAMLIB_API par_write_control(port_t *port, unsigned char control)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	unsigned char ctrl = control ^ CP_ACTIVE_LOW_BITS;
	status = ioctl(port->fd, PPWCONTROL, &ctrl);
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(WIN32)
  unsigned char ctr = control;
  unsigned char dummyc;
  unsigned int dummy;
  const unsigned char wm = (C1284_NSTROBE |
			    C1284_NAUTOFD |
			    C1284_NINIT |
			    C1284_NSELECTIN);

  if (ctr & 0x20)
    {
      rig_debug (RIG_DEBUG_WARN, "use ieee1284_data_dir to change data line direction!\n");
    }

  /* Deal with inversion issues. */
  ctr ^= wm & C1284_INVERTED;
  ctr = (ctr & ~wm) ^ (ctr & wm);
  if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_CONTROL, &ctr, 
          sizeof(ctr), &dummyc, sizeof(dummyc), (LPDWORD)&dummy, NULL))) {
      rig_debug(RIG_DEBUG_ERR,"frob_control: DeviceIoControl failed!\n");
  }
  return RIG_OK;
#endif
	return -RIG_ENIMPL;
}

int HAMLIB_API par_read_control(port_t *port, unsigned char *control)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	unsigned char ctrl;
	status = ioctl(port->fd, PPRCONTROL, &ctrl);
	*control = ctrl ^ CP_ACTIVE_LOW_BITS;
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(WIN32)
	char ret;
	unsigned int dummy;

	if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_CONTROL, NULL, 0, &ret, 
			sizeof(ret), (LPDWORD)&dummy, NULL))) {
		rig_debug(RIG_DEBUG_ERR, "%s: DeviceIoControl failed!\n", __FUNCTION__);
	}

	*control = ret ^ S1284_INVERTED;
#endif
	return -RIG_ENIMPL;
}


int HAMLIB_API par_read_status(port_t *port, unsigned char *status)
{
#ifdef HAVE_LINUX_PPDEV_H
	int ret;
	unsigned char sta;
	ret = ioctl(port->fd, PPRSTATUS, &sta);
	*status = sta ^ SP_ACTIVE_LOW_BITS;
	return ret == 0 ? RIG_OK : -RIG_EIO;
#elif defined(WIN32)
	unsigned char ret;
	unsigned int dummy;

	if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_STATUS, NULL, 0, &ret, 
			sizeof(ret), (LPDWORD)&dummy, NULL))) {
		rig_debug(RIG_DEBUG_ERR, "%s: DeviceIoControl failed!\n", __FUNCTION__);
	}

	*status = ret ^ S1284_INVERTED;
#endif
	return -RIG_ENIMPL;
}


int HAMLIB_API par_lock(port_t *port)
{
#ifdef HAVE_LINUX_PPDEV_H
	if (ioctl(port->fd, PPCLAIM) < 0) {
		rig_debug(RIG_DEBUG_ERR, "Claiming device \"%s\": %s\n", port->pathname, strerror(errno));
		return -RIG_EIO;
	}
	return RIG_OK;
#elif defined(WIN32)
	return RIG_OK;
#endif
	return -RIG_ENIMPL;
}

int HAMLIB_API par_unlock(port_t *port)
{
#ifdef HAVE_LINUX_PPDEV_H
	if (ioctl(port->fd, PPRELEASE) < 0) {
		rig_debug(RIG_DEBUG_ERR, "Releasing device \"%s\": %s\n", port->pathname, strerror(errno));
		return -RIG_EIO;
	}
	return RIG_OK;
#elif defined(WIN32)
	return RIG_OK;
#endif
	return -RIG_ENIMPL;
}


int par_ptt_set(port_t *p, ptt_t pttx)
{
		switch(p->type.ptt) {
		case RIG_PTT_PARALLEL:
				{
					unsigned char reg;
					int status;

					status = par_read_data(p, &reg);
					if (status != RIG_OK)
						return status;
					if (pttx == RIG_PTT_ON)
						reg |=   1 << p->parm.parallel.pin;
					else
						reg &= ~(1 << p->parm.parallel.pin);

					return par_write_data(p, reg);
				}
		default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported PTT type %d\n", 
								p->type.ptt);
				return -RIG_EINVAL;
		}
		return RIG_OK;
}

/*
 * assumes pttx not NULL
 */
int par_ptt_get(port_t *p, ptt_t *pttx)
{
		switch(p->type.ptt) {
		case RIG_PTT_PARALLEL:
				{
					unsigned char reg;
					int status;

					status = par_read_data(p, &reg);
					*pttx = reg & (1<<p->parm.parallel.pin) ? 
							RIG_PTT_ON:RIG_PTT_OFF;
					return status;
				}
		default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported PTT type %d\n", 
								p->type.ptt);
				return -RIG_ENAVAIL;
		}
		return RIG_OK;
}

/*
 * assumes dcdx not NULL
 */
int par_dcd_get(port_t *p, dcd_t *dcdx)
{
		switch(p->type.dcd) {
		case RIG_DCD_PARALLEL:
				{
					unsigned char reg;
					int status;

					status = par_read_data(p, &reg);
					*dcdx = reg & (1<<p->parm.parallel.pin) ? 
							RIG_DCD_ON:RIG_DCD_OFF;
					return status;
				}
		default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported DCD type %d\n", 
								p->type.dcd);
				return -RIG_ENAVAIL;
		}
		return RIG_OK;
}

