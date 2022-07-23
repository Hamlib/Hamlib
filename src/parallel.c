/*
 *  Hamlib Interface - parallel communication low-level support
 *  Copyright (c) 2000-2010 by Stephane Fillod
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
 * \brief Parallel Port IO
 * \file parallel.c
 */
#include <hamlib/config.h>

#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/types.h>

#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#ifdef HAVE_WINDOWS_H
#  include <windows.h>
#  include "par_nt.h"
#endif

#ifdef HAVE_WINIOCTL_H
#  include <winioctl.h>
#endif

#ifdef HAVE_WINBASE_H
#  include <winbase.h>
#endif

#include <hamlib/rig.h>
#include "parallel.h"

#ifdef HAVE_LINUX_PPDEV_H
#  include <linux/ppdev.h>
#  include <linux/parport.h>
#endif

#ifdef HAVE_DEV_PPBUS_PPI_H
#  include <dev/ppbus/ppi.h>
#  include <dev/ppbus/ppbconf.h>
#endif

//! @cond Doxygen_Suppress
/*
 * These control port bits are active low.
 * We toggle them so that this weirdness doesn't get propagated
 * through our interface.
 */
#define CP_ACTIVE_LOW_BITS  0x0B

/*
 * These status port bits are active low.
 * We toggle them so that this weirdness doesn't get propagated
 * through our interface.
 */
#define SP_ACTIVE_LOW_BITS  0x80
//! @endcond

/*
   Pinout table of parallel port from http://en.wikipedia.org/wiki/Parallel_port#Pinouts

   Pin No (DB25)  Signal name     Direction  Register-bit Inverted
        1         *Strobe         In/Out     Control-0    Yes
        2         Data0           Out        Data-0       No
        3         Data1           Out        Data-1       No
        4         Data2           Out        Data-2       No
        5         Data3           Out        Data-3       No
        6         Data4           Out        Data-4       No
        7         Data5           Out        Data-5       No
        8         Data6           Out        Data-6       No
        9         Data7           Out        Data-7       No
        10        *Ack            In         Status-6     No
        11        Busy            In         Status-7     Yes
        12        Paper-Out       In         Status-5     No
        13        Select          In         Status-4     No
        14        Linefeed        In/Out     Control-1    Yes
        15        *Error          In         Status-3     No
        16        *Reset          In/Out     Control-2    No
        17        *Select-Printer In/Out     Control-3    Yes

        * means low true, e.g., *Strobe.
 */


/**
 * \brief Open Parallel Port
 * \param port
 * \return file descriptor
 *
 * TODO: to be called before exiting: atexit(parport_cleanup)
 * void parport_cleanup() { ioctl(fd, PPRELEASE); }
 */
int par_open(hamlib_port_t *port)
{
    int fd;
#ifdef HAVE_LINUX_PPDEV_H
    int mode;
#endif

#if defined (__WIN64__) || defined(__WIN32__)
    // cppcheck-suppress *
    HANDLE handle;
#endif

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!port->pathname[0])
    {
        return -RIG_EINVAL;
    }

#ifdef HAVE_LINUX_PPDEV_H
    /* TODO: open with O_NONBLOCK ? */
    fd = open(port->pathname, O_RDWR);

    if (fd < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: opening device \"%s\": %s\n",
                  __func__,
                  port->pathname,
                  strerror(errno));
        return -RIG_EIO;
    }

    mode = IEEE1284_MODE_COMPAT;

    if (ioctl(fd, PPSETMODE, &mode) != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: PPSETMODE \"%s\": %s\n",
                  __func__,
                  port->pathname,
                  strerror(errno));
        close(fd);
        return -RIG_EIO;
    }

#elif defined(HAVE_DEV_PPBUS_PPI_H)

    fd = open(port->pathname, O_RDWR);

    if (fd < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: opening device \"%s\": %s\n",
                  __func__,
                  port->pathname,
                  strerror(errno));
        return -RIG_EIO;
    }

#elif defined(__WIN64__) || defined(__WIN32__)
    handle = CreateFile(port->pathname, GENERIC_READ | GENERIC_WRITE,
                        0, NULL, OPEN_EXISTING, 0, NULL);

    if (handle == INVALID_HANDLE_VALUE)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: opening device \"%s\"\n",
                  __func__,
                  port->pathname);
        CloseHandle(handle);
        return -RIG_EIO;
    }
    else
    {
        fd = _open_osfhandle((intptr_t)handle, _O_APPEND | _O_RDONLY);

        if (fd == -1)
        {
            return -RIG_EIO;
        }
    }

#else
    port->fd = fd = 0;
    return -RIG_ENIMPL;
#endif
    port->fd = fd;
    return fd;
}


/**
 * \brief Close Parallel Port
 * \param port
 */
int par_close(hamlib_port_t *port)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

#ifdef HAVE_LINUX_PPDEV_H
#elif defined(HAVE_DEV_PPBUS_PPI_H)
#elif defined(__WIN64__) || defined(__WIN32__)
    _close(port->fd);

    return RIG_OK;
#endif
    return close(port->fd);
}


/**
 * \brief Send data on Parallel port
 * \param port
 * \param data
 */
int HAMLIB_API par_write_data(hamlib_port_t *port, unsigned char data)
{
#ifdef HAVE_LINUX_PPDEV_H
    int status;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    status = ioctl(port->fd, PPWDATA, &data);
    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
    int status;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    status = ioctl(port->fd, PPISDATA, &data);
    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(__WIN64__) || defined(__WIN32__)
    unsigned int dummy = 0;

    intptr_t handle;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    handle = _get_osfhandle(port->fd);

    if (handle != (intptr_t)INVALID_HANDLE_VALUE)
    {
        if (!(DeviceIoControl((HANDLE)handle, NT_IOCTL_DATA, &data, sizeof(data),
                              NULL, 0, (LPDWORD)&dummy, NULL)))
        {
            rig_debug(RIG_DEBUG_ERR, "%s: DeviceIoControl failed!\n", __func__);
            return -RIG_EIO;
        }
    }

    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif
}


/**
 * \brief Receive data on Parallel port
 * \param port
 * \param data
 */
int HAMLIB_API par_read_data(hamlib_port_t *port, unsigned char *data)
{
#ifdef HAVE_LINUX_PPDEV_H
    int status;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    status = ioctl(port->fd, PPRDATA, data);
    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
    int status;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    status = ioctl(port->fd, PPIGDATA, &data);
    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(__WIN64__) || defined(__WIN32__)
    unsigned char ret = 0;
    unsigned int dummy = 0;

    intptr_t handle;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    handle = _get_osfhandle(port->fd);

    if (handle != (intptr_t)INVALID_HANDLE_VALUE)
    {
        if (!(DeviceIoControl((HANDLE)handle,
                              NT_IOCTL_STATUS,
                              NULL,
                              0,
                              &ret,
                              sizeof(ret),
                              (LPDWORD)&dummy,
                              NULL)))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: DeviceIoControl failed!\n",
                      __func__);

            return -RIG_EIO;
        }
    }

    *data = ret ^ S1284_INVERTED;
    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif
}


/**
 * \brief Set control data for Parallel Port
 * \param port
 * \param control
 */
int HAMLIB_API par_write_control(hamlib_port_t *port, unsigned char control)
{
#ifdef HAVE_LINUX_PPDEV_H
    int status;
    unsigned char ctrl = control ^ CP_ACTIVE_LOW_BITS;
    status = ioctl(port->fd, PPWCONTROL, &ctrl);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: ioctl(PPWCONTROL) failed: %s\n",
                  __func__,
                  strerror(errno));
    }

    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
    int status;
    unsigned char ctrl = control ^ CP_ACTIVE_LOW_BITS;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    status = ioctl(port->fd, PPISCTRL, &ctrl);
    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(__WIN64__) || defined(__WIN32__)
    unsigned char ctr = control;
    unsigned char dummyc;
    unsigned int dummy = 0;
    const unsigned char wm = (C1284_NSTROBE
                              | C1284_NAUTOFD
                              | C1284_NINIT
                              | C1284_NSELECTIN);
    intptr_t handle;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (ctr & 0x20)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: use ieee1284_data_dir to change data line direction!\n",
                  __func__);
    }

    /* Deal with inversion issues. */
    ctr ^= wm & C1284_INVERTED;
    ctr = (ctr & ~wm) ^ (ctr & wm);

    handle = _get_osfhandle(port->fd);

    if (handle != (intptr_t)INVALID_HANDLE_VALUE)
    {
        if (!(DeviceIoControl((HANDLE)handle,
                              NT_IOCTL_CONTROL,
                              &ctr,
                              sizeof(ctr),
                              &dummyc,
                              sizeof(dummyc),
                              (LPDWORD)&dummy,
                              NULL)))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: frob_control: DeviceIoControl failed!\n",
                      __func__);
            return -RIG_EIO;
        }
    }

    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif
}


/**
 * \brief Read control data for Parallel Port
 * \param port
 * \param control
 */
int HAMLIB_API par_read_control(hamlib_port_t *port, unsigned char *control)
{

#ifdef HAVE_LINUX_PPDEV_H
    int status;
    unsigned char ctrl;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    status = ioctl(port->fd, PPRCONTROL, &ctrl);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: ioctl(PPRCONTROL) failed: %s\n",
                  __func__,
                  strerror(errno));
    }

    *control = ctrl ^ CP_ACTIVE_LOW_BITS;
    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
    int status;
    unsigned char ctrl;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    status = ioctl(port->fd, PPIGCTRL, &ctrl);
    *control = ctrl ^ CP_ACTIVE_LOW_BITS;
    return status == 0 ? RIG_OK : -RIG_EIO;
#elif defined(__WIN64__) || defined(__WIN32__)
    unsigned char ret = 0;
    unsigned int dummy = 0;

    intptr_t handle;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    handle = _get_osfhandle(port->fd);

    if (handle != (intptr_t)INVALID_HANDLE_VALUE)
    {
        if (!(DeviceIoControl((HANDLE)handle,
                              NT_IOCTL_CONTROL,
                              NULL,
                              0,
                              &ret,
                              sizeof(ret),
                              (LPDWORD)&dummy,
                              NULL)))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: DeviceIoControl failed!\n",
                      __func__);

            return -RIG_EIO;
        }
    }

    *control = ret ^ S1284_INVERTED;

    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif
}


/**
 * \brief Get parallel port status
 * \param port
 * \param status
 * \return RIG_OK or < 0 for error
 */
int HAMLIB_API par_read_status(hamlib_port_t *port, unsigned char *status)
{

#ifdef HAVE_LINUX_PPDEV_H
    int ret;
    unsigned char sta;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    ret = ioctl(port->fd, PPRSTATUS, &sta);
    *status = sta ^ SP_ACTIVE_LOW_BITS;
    return ret == 0 ? RIG_OK : -RIG_EIO;

#elif defined(HAVE_DEV_PPBUS_PPI_H)
    int ret;
    unsigned char sta;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    ret = ioctl(port->fd, PPIGSTATUS, &sta);
    *status = sta ^ SP_ACTIVE_LOW_BITS;
    return ret == 0 ? RIG_OK : -RIG_EIO;

#elif defined(__WIN64__) || defined(__WIN32__)
    unsigned char ret = 0;
    unsigned int dummy = 0;

    intptr_t handle;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    handle = _get_osfhandle(port->fd);

    if (handle != (intptr_t)INVALID_HANDLE_VALUE)
    {
        if (!(DeviceIoControl((HANDLE)handle,
                              NT_IOCTL_STATUS,
                              NULL,
                              0,
                              &ret,
                              sizeof(ret),
                              (LPDWORD)&dummy,
                              NULL)))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: DeviceIoControl failed!\n",
                      __func__);

            return -RIG_EIO;
        }
    }

    *status = ret ^ S1284_INVERTED;

    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif
}


/**
 * \brief Get a lock on the Parallel Port
 * \param port
 * \return RIG_OK or < 0
 */
int HAMLIB_API par_lock(hamlib_port_t *port)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

#ifdef HAVE_LINUX_PPDEV_H

    if (ioctl(port->fd, PPCLAIM) < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: claiming device \"%s\": %s\n",
                  __func__,
                  port->pathname,
                  strerror(errno));
        return -RIG_EIO;
    }

    return RIG_OK;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
    return RIG_OK;
#elif defined(__WIN64__) || defined(__WIN32__)
    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif
}


/**
 * \brief Release lock on Parallel Port
 * \param port
 * \return RIG_OK or < 0
 */
int HAMLIB_API par_unlock(hamlib_port_t *port)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

#ifdef HAVE_LINUX_PPDEV_H

    if (ioctl(port->fd, PPRELEASE) < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: releasing device \"%s\": %s\n",
                  __func__,
                  port->pathname,
                  strerror(errno));
        return -RIG_EIO;
    }

    return RIG_OK;
#elif defined(HAVE_DEV_PPBUS_PPI_H)
    return RIG_OK;
#elif defined(__WIN64__) || defined(__WIN32__)
    return RIG_OK;
#else
    return -RIG_ENIMPL;
#endif
}


#ifndef PARPORT_CONTROL_STROBE
#  define PARPORT_CONTROL_STROBE 0x1
#endif

#ifndef PARPORT_CONTROL_INIT
#  define PARPORT_CONTROL_INIT 0x4
#endif


/**
 * \brief Set or unset Push to talk bit on Parallel Port
 * \param p
 * \param pttx RIG_PTT_ON --> Set PTT
 * \return RIG_OK or < 0 error
 */
int par_ptt_set(hamlib_port_t *p, ptt_t pttx)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (p->type.ptt)
    {
    case RIG_PTT_PARALLEL:
    {
        unsigned char ctl;
        int status;

        par_lock(p);
        status = par_read_control(p, &ctl);

        if (status != RIG_OK)
        {
            return status;
        }

        /* Enable CW & PTT - /STROBE bit (pin 1) */
        ctl &= ~PARPORT_CONTROL_STROBE;

        /* TODO: kill parm.parallel.pin? */

        /* PTT keying - /INIT bit (pin 16) (inverted) */
        if (pttx == RIG_PTT_ON)
        {
            ctl |= PARPORT_CONTROL_INIT;
        }
        else
        {
            ctl &= ~PARPORT_CONTROL_INIT;
        }

        status = par_write_control(p, ctl);
        par_unlock(p);
        return status;
    }

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported PTT type %d\n",
                  __func__,
                  p->type.ptt);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/**
 * \brief Get state of Push to Talk from Parallel Port
 * \param p
 * \param pttx return value (must be non NULL)
 * \return RIG_OK or < 0 error
 */
int par_ptt_get(hamlib_port_t *p, ptt_t *pttx)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (p->type.ptt)
    {
    case RIG_PTT_PARALLEL:
    {
        unsigned char ctl;
        int status;

        par_lock(p);
        status = par_read_control(p, &ctl);
        par_unlock(p);

        if (status == RIG_OK)
        {
            *pttx = (ctl & PARPORT_CONTROL_INIT) &&
                    !(ctl & PARPORT_CONTROL_STROBE) ?
                    RIG_PTT_ON : RIG_PTT_OFF;
        }

        return status;
    }

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported PTT type %d\n",
                  __func__,
                  p->type.ptt);
        return -RIG_ENAVAIL;
    }

    return RIG_OK;
}


/**
 * \brief get Data Carrier Detect (squelch) from Parallel Port
 * \param p
 * \param dcdx return value (Must be non NULL)
 * \return RIG_OK or < 0 error
 */
int par_dcd_get(hamlib_port_t *p, dcd_t *dcdx)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (p->type.dcd)
    {
    case RIG_DCD_PARALLEL:
    {
        unsigned char reg;
        int status;

        status = par_read_data(p, &reg);

        if (status == RIG_OK)
        {
            *dcdx = (reg & (1 << p->parm.parallel.pin)) ?
                    RIG_DCD_ON : RIG_DCD_OFF;
        }

        return status;
    }

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported DCD type %d\n",
                  __func__,
                  p->type.dcd);
        return -RIG_ENAVAIL;
    }

    return RIG_OK;
}

/** @} */
