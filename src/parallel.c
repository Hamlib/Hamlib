/*
 *  Hamlib Interface - parallel communication low-level support
 *  Copyright (c) 2000-2005 by Stephane Fillod
 *
 *	$Id: parallel.c,v 1.5 2005-10-27 20:34:16 fillods Exp $
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

#include "hamlib/rig.h"
#include "parallel.h"

#ifdef HAVE_LINUX_PPDEV_H
#include <linux/ppdev.h>
#include <linux/parport.h>
#endif

#ifdef HAVE_DEV_PPBUS_PPI_H
#include <dev/ppbus/ppi.h>
#include <dev/ppbus/ppbconf.h>
#endif

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


/*
 * TODO: to be called before exiting: atexit(parport_cleanup)
 * void parport_cleanup() { ioctl(fd, PPRELEASE); }
 */

int par_open(hamlib_port_t *port)
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

#elif defined(HAVE_DEV_PPBUS_PPI_H)

	fd = open(port->pathname, O_RDWR);
	if (fd < 0) {
		rig_debug(RIG_DEBUG_ERR, "Opening device \"%s\": %s\n", port->pathname, strerror(errno));
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

int par_close(hamlib_port_t *port)
{
#ifdef HAVE_LINUX_PPDEV_H
#elif defined(HAVE_DEV_PPBUS_PPI_H)
#elif defined(WIN32)
	CloseHandle((HANDLE)(port->fd));
	return RIG_OK;
#endif
	return close(port->fd);
}

int HAMLIB_API par_write_data(hamlib_port_t *port, unsigned char data)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	status = ioctl(port->fd, PPWDATA, &data);
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
	int status;
	status = ioctl(port->fd, PPISDATA, &data);
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

int HAMLIB_API par_read_data(hamlib_port_t *port, unsigned char *data)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	status = ioctl(port->fd, PPRDATA, data);
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
	int status;
	status = ioctl(port->fd, PPIGDATA, &data);
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


int HAMLIB_API par_write_control(hamlib_port_t *port, unsigned char control)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	unsigned char ctrl = control ^ CP_ACTIVE_LOW_BITS;
	status = ioctl(port->fd, PPWCONTROL, &ctrl);
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
	int status;
	unsigned char ctrl = control ^ CP_ACTIVE_LOW_BITS;
	status = ioctl(port->fd, PPISCTRL, &ctrl);
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

int HAMLIB_API par_read_control(hamlib_port_t *port, unsigned char *control)
{
#ifdef HAVE_LINUX_PPDEV_H
	int status;
	unsigned char ctrl;
	status = ioctl(port->fd, PPRCONTROL, &ctrl);
	*control = ctrl ^ CP_ACTIVE_LOW_BITS;
	return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
	int status;
	unsigned char ctrl;
	status = ioctl(port->fd, PPIGCTRL, &ctrl);
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


int HAMLIB_API par_read_status(hamlib_port_t *port, unsigned char *status)
{
#ifdef HAVE_LINUX_PPDEV_H
	int ret;
	unsigned char sta;
	ret = ioctl(port->fd, PPRSTATUS, &sta);
	*status = sta ^ SP_ACTIVE_LOW_BITS;
	return ret == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
	int ret;
	unsigned char sta;
	ret = ioctl(port->fd, PPIGSTATUS, &sta);
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


int HAMLIB_API par_lock(hamlib_port_t *port)
{
#ifdef HAVE_LINUX_PPDEV_H
	if (ioctl(port->fd, PPCLAIM) < 0) {
		rig_debug(RIG_DEBUG_ERR, "Claiming device \"%s\": %s\n", port->pathname, strerror(errno));
		return -RIG_EIO;
	}
	return RIG_OK;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
#elif defined(WIN32)
	return RIG_OK;
#endif
	return -RIG_ENIMPL;
}

int HAMLIB_API par_unlock(hamlib_port_t *port)
{
#ifdef HAVE_LINUX_PPDEV_H
	if (ioctl(port->fd, PPRELEASE) < 0) {
		rig_debug(RIG_DEBUG_ERR, "Releasing device \"%s\": %s\n", port->pathname, strerror(errno));
		return -RIG_EIO;
	}
	return RIG_OK;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
#elif defined(WIN32)
	return RIG_OK;
#endif
	return -RIG_ENIMPL;
}


int par_ptt_set(hamlib_port_t *p, ptt_t pttx)
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
int par_ptt_get(hamlib_port_t *p, ptt_t *pttx)
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
int par_dcd_get(hamlib_port_t *p, dcd_t *dcdx)
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

