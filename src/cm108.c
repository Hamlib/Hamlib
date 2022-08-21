/*
 *  Hamlib Interface - CM108 HID communication low-level support
 *  Copyright (c) 2000-2012 by Stephane Fillod
 *  Copyright (c) 2011 by Andrew Errington
 *  CM108 detection code Copyright (c) Thomas Sailer used with permission
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

/**
 * \addtogroup rig_internal
 * @{
 */

/**
 * \file cm108.c
 * \brief CM108 GPIO support.
 *
 * CM108 Audio chips found on many USB audio interfaces have controllable
 * General Purpose Input/Output pins.
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

#ifdef HAVE_LINUX_HIDRAW_H
#  include <linux/hidraw.h>
#endif

#include <hamlib/rig.h>
#include "cm108.h"


/**
 * \brief Open CM108 HID port (/dev/hidraw<i>X</i>).
 *
 * \param port The port structure.
 *
 * \return File descriptor, otherwise a **negative value** if an error
 * occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_EINVAL The port pathname is empty or no CM108 device detected.
 * \retval RIG_EIO The `open`(2) system call returned a **negative value**.
 */
int cm108_open(hamlib_port_t *port)
{
    int fd;
#ifdef HAVE_LINUX_HIDRAW_H
    struct hidraw_devinfo hiddevinfo;
#endif

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!port->pathname[0])
    {
        return -RIG_EINVAL;
    }

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

#ifdef HAVE_LINUX_HIDRAW_H
    // CM108 detection copied from Thomas Sailer's soundmodem code
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: checking for cm108 (or compatible) device\n",
              __func__);

    if (!ioctl(fd, HIDIOCGRAWINFO, &hiddevinfo)
            && ((hiddevinfo.vendor == 0x0d8c
                 // CM108/108B/109/119/119A/119B
                 && ((hiddevinfo.product >= 0x0008
                      && hiddevinfo.product <= 0x000f)
                     || hiddevinfo.product == 0x0012
                     || hiddevinfo.product == 0x013a
                     || hiddevinfo.product == 0x013c
                     || hiddevinfo.product == 0x0013))
                // SSS1621/23
                || (hiddevinfo.vendor == 0x0c76
                    && (hiddevinfo.product == 0x1605
                        || hiddevinfo.product == 0x1607
                        || hiddevinfo.product == 0x160b))))
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: cm108 compatible device detected\n",
                  __func__);
    }
    else
    {
        close(fd);
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: no cm108 (or compatible) device detected\n",
                  __func__);
        return -RIG_EINVAL;
    }

#endif

    port->fd = fd;
    return fd;
}


/**
 * \brief Close a CM108 HID port.
 *
 * \param port The port structure
 *
 * \return Zero if the port was closed successfully, otherwise -1 if an error
 * occurred (in which case, the system `errno`(3) is set appropriately).
 *
 * \sa The `close`(2) system call.
 */
int cm108_close(hamlib_port_t *port)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return close(port->fd);
}


/**
 * \brief Set or unset the Push To Talk bit on a CM108 GPIO.
 *
 * \param p The port structure.
 * \param pttx RIG_PTT_ON --> Set PTT, else unset PTT.
 *
 * \return RIG_OK on success, otherwise a **negative value** if an error
 * occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Setting or unsetting the PTT was successful.
 * \retval RIG_EINVAL The file descriptor is invalid or the PTT type is
 * unsupported.
 * \retval RIG_EIO The `write`(2) system call returned a **negative value**.
 */
int cm108_ptt_set(hamlib_port_t *p, ptt_t pttx)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // For a CM108 USB audio device PTT is wired up to one of the GPIO
    // pins.  Usually this is GPIO3 (bit 2 of the GPIO register) because it
    // is on the corner of the chip package (pin 13) so it's easily accessible.
    // Some CM108 chips are epoxy-blobbed onto the PCB, so no GPIO
    // pins are accessible.  The SSS1623 chips have a different pinout, so
    // we make the GPIO bit number configurable.

    switch (p->type.ptt)
    {

    case RIG_PTT_CM108:
    {
        ssize_t nw;
        char out_rep[] =
        {
            0x00, // report number
            // HID output report
            0x00,
            (pttx == RIG_PTT_ON) ? (1 << p->parm.cm108.ptt_bitnum) : 0, // set GPIO
            1 << p->parm.cm108.ptt_bitnum, // Data direction register (1=output)
              0x00
        };

        // Build a packet for CM108 HID to turn GPIO bit on or off.
        // Packet is 4 bytes, preceded by a 'report number' byte
        // 0x00 report number
        // Write data packet (from CM108 documentation)
        // byte 0: 00xx xxxx     Write GPIO
        // byte 1: xxxx dcba     GPIO3-0 output values (1=high)
        // byte 2: xxxx dcba     GPIO3-0 data-direction register (1=output)
        // byte 3: xxxx xxxx     SPDIF

        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: bit number %d to state %d\n",
                  __func__,
                  p->parm.cm108.ptt_bitnum,
                  (pttx == RIG_PTT_ON) ? 1 : 0);

        if (p->fd == -1)
        {
            return -RIG_EINVAL;
        }

        // Send the HID packet
        nw = write(p->fd, out_rep, sizeof(out_rep));

        if (nw < 0)
        {
            return -RIG_EIO;
        }

        return RIG_OK;
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
 * \brief Get the state of Push To Talk from a CM108 GPIO.
 *
 * \param p The port structure.
 * \param pttx Return value (must be non NULL).
 *
 * \return RIG_OK on success, otherwise a **negative value** if an error
 * occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Getting the PTT state was successful.
 * \retval RIG_ENIMPL Getting the state is not yet implemented.
 * \retval RIG_ENAVAIL Getting the state is not available for this PTT type.
 */
int cm108_ptt_get(hamlib_port_t *p, ptt_t *pttx)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (p->type.ptt)
    {
    case RIG_PTT_CM108:
    {
        return -RIG_ENIMPL;
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


#ifdef XXREMOVEXX
// Not referenced anywhere
/**
 * \brief get Data Carrier Detect (squelch) from CM108 GPIO
 * \param p
 * \param dcdx return value (Must be non NULL)
 * \return RIG_OK or < 0 error
 */
int cm108_dcd_get(hamlib_port_t *p, dcd_t *dcdx)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // On the CM108 and compatible chips the squelch line on the radio is
    // wired to Volume Down input pin.  The state of this pin is reported
    // in HID messages from the CM108 device, but I am not sure how
    // to query this state on demand.

    switch (p->type.dcd)
    {
    case RIG_DCD_CM108:
    {
        return -RIG_ENIMPL;
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
#endif

/** @} */
