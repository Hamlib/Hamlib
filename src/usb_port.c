/*
 *  Hamlib Interface - USB communication low-level support
 *  Copyright (c) 2000-2011 by Stephane Fillod
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
 * \brief USB IO
 * \file usb_port.c
 *
 * doc todo: deal with defined(HAVE_LIBUSB)... quashing the doc process.
 */

#include <hamlib/config.h>



#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <errno.h>
#include <sys/types.h>

#include <hamlib/rig.h>

#ifdef HAVE_LIBUSB_H
#  include <libusb.h>
#elif defined HAVE_LIBUSB_1_0_LIBUSB_H
#  include <libusb-1.0/libusb.h>
#endif

#include "usb_port.h"

/*
 * Compile only if libusb is available
 */
#if defined(HAVE_LIBUSB) && (defined(HAVE_LIBUSB_H) || defined(HAVE_LIBUSB_1_0_LIBUSB_H))

/**
 * \brief Find and open USB device
 * \param port
 * \return usb_handle
 */
static libusb_device_handle *find_and_open_device(const hamlib_port_t *port)
{
    libusb_device_handle *udh = NULL;
    libusb_device *dev, **devs;
    struct libusb_device_descriptor desc;
    char    string[256];
    int i, r;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: looking for device %04x:%04x...",
              __func__,
              port->parm.usb.vid,
              port->parm.usb.pid);

    r = libusb_get_device_list(NULL, &devs);

    if (r < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: failed getting usb device list: %s",
                  __func__,
                  libusb_error_name(r));

        return NULL;
    }

    for (i = 0; (dev = devs[i]) != NULL; i++)
    {

        libusb_get_device_descriptor(dev, &desc);

        rig_debug(RIG_DEBUG_VERBOSE,
                  " %04x:%04x,",
                  desc.idVendor,
                  desc.idProduct);

        if (desc.idVendor == port->parm.usb.vid
                && desc.idProduct == port->parm.usb.pid)
        {

            /* we need to open the device in order to query strings */
            r = libusb_open(dev, &udh);

            if (r < 0)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "%s: Warning: Cannot open USB device: %s\n",
                          __func__,
                          libusb_error_name(r));

                continue;
            }

            /* now check whether the names match: */
            if (port->parm.usb.vendor_name)
            {

                string[0] = '\0';
                r = libusb_get_string_descriptor_ascii(udh,
                                                       desc.iManufacturer,
                                                       (unsigned char *)string,
                                                       sizeof(string));

                if (r < 0)
                {
                    rig_debug(RIG_DEBUG_WARN,
                              "Warning: cannot query manufacturer for USB device: %s\n",
                              libusb_error_name(r));

                    libusb_close(udh);
                    continue;
                }

                rig_debug(RIG_DEBUG_VERBOSE, " vendor >%s<", string);

                if (strcmp(string, port->parm.usb.vendor_name) != 0)
                {
                    rig_debug(RIG_DEBUG_WARN,
                              "%s: Warning: Vendor name string mismatch!\n",
                              __func__);

                    libusb_close(udh);
                    continue;
                }
            }

            if (port->parm.usb.product)
            {

                string[0] = '\0';
                r = libusb_get_string_descriptor_ascii(udh,
                                                       desc.iProduct,
                                                       (unsigned char *)string,
                                                       sizeof(string));

                if (r < 0)
                {
                    rig_debug(RIG_DEBUG_WARN,
                              "Warning: cannot query product for USB device: %s\n",
                              libusb_error_name(r));

                    libusb_close(udh);
                    continue;
                }

                rig_debug(RIG_DEBUG_VERBOSE, " product >%s<", string);

                if (strcmp(string, port->parm.usb.product) != 0)
                {

                    /* Now testing with strncasecmp() for case insensitive
                     * match.  Updating firmware on FUNcube Dongle to v18f resulted
                     * in product string changing from "FunCube Dongle" to
                     * "FUNcube Dongle".  As new dongles are shipped with
                     * older firmware, both product strings are valid.  Sigh...
                     */
                    if (strncasecmp(string,
                                    port->parm.usb.product,
                                    sizeof(port->parm.usb.product) - 1) != 0)
                    {

                        rig_debug(RIG_DEBUG_WARN,
                                  "%s: Warning: Product string mismatch!\n",
                                  __func__);

                        libusb_close(udh);
                        continue;
                    }
                }
            }

            libusb_free_device_list(devs, 1);

            rig_debug(RIG_DEBUG_VERBOSE, "%s", " -> found\n");

            return udh;
        }
    }

    libusb_free_device_list(devs, 1);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", " -> not found\n");

    return NULL;        /* not found */
}


/**
 * \brief Open hamlib_port of USB device
 * \param port
 * \return status
 */
int usb_port_open(hamlib_port_t *port)
{
    static char pathname[HAMLIB_FILPATHLEN];
    libusb_device_handle *udh;
    char *p, *q;
    int r;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* init default libusb-1.0 library contexte, if needed */
    r = libusb_init(NULL);

    if (r < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: libusb_init failed: %s\n",
                  __func__,
                  libusb_error_name(r));

        return -RIG_EIO;
    }

    //libusb_set_debug(NULL, 1);

    /* Extract VID/PID/Vendor/Product name from pathname. */
    /* Duplicate the string since we may modify it. */
    strncpy(pathname, port->pathname, sizeof pathname);
    pathname[HAMLIB_FILPATHLEN - 1] = '\0';

    p = pathname;
    q = strchr(p, ':');

    if (q)
    {
        ++q;
        port->parm.usb.vid = strtol(q, NULL, 16);
        p = q;
        q = strchr(p, ':');

        if (q)
        {
            ++q;
            port->parm.usb.pid = strtol(q, NULL, 16);
            p = q;
            q = strchr(p, ':');

            if (q)
            {
                ++q;
                port->parm.usb.vendor_name = q;
                p = q;
                q = strchr(p, ':');

                if (q)
                {
                    *q++ = '\0';
                    port->parm.usb.product = q;
                }
            }
        }
    }

    udh = find_and_open_device(port);

    if (udh == 0)
    {
        libusb_exit(NULL);
        return -RIG_EIO;
    }

    /* Try to detach ftdi_sio kernel module.
     * This should be performed only for devices using
     * USB-serial converters (like FTDI chips), for other
     * devices this may cause problems, so do not do it.
     */
    (void)libusb_set_auto_detach_kernel_driver(udh, port->parm.usb.iface);

    if (port->parm.usb.iface >= 0)
    {

#ifdef _WIN32

        /* Is it still needed with libusb-1.0 ? */
        if (port->parm.usb.conf >= 0
                && (r = libusb_set_configuration(udh, port->parm.usb.conf)) < 0)
        {

            rig_debug(RIG_DEBUG_ERR,
                      "%s: libusb_set_configuration: failed conf %d: %s\n",
                      __func__,
                      port->parm.usb.conf,
                      libusb_error_name(r));

            libusb_close(udh);
            libusb_exit(NULL);

            return -RIG_EIO;
        }

#endif

        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: claiming %d\n",
                  __func__,
                  port->parm.usb.iface);

        r = libusb_claim_interface(udh, port->parm.usb.iface);

        if (r < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s:libusb_claim_interface: failed interface %d: %s\n",
                      __func__,
                      port->parm.usb.iface,
                      libusb_error_name(r));

            libusb_close(udh);
            libusb_exit(NULL);

            return -RIG_EIO;
        }

#if 0
        r = libusb_set_interface_alt_setting(udh, port->parm.usb.iface,
                                             port->parm.usb.alt);

        if (r < 0)
        {
            fprintf(stderr,
                    "%s:usb_set_alt_interface: failed: %s\n",
                    __func__,
                    libusb_error_name(r));

            libusb_release_interface(udh, port->parm.usb.iface);
            libusb_close(udh);
            libusb_exit(NULL);

            return -RIG_EIO;
        }

#endif

    }

    port->handle = (void *) udh;

    return RIG_OK;
}


/**
 * \brief Close hamlib_port of USB device
 * \param port
 * \return status
 */
int usb_port_close(hamlib_port_t *port)
{

    libusb_device_handle *udh = port->handle;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    libusb_release_interface(udh, port->parm.usb.iface);

    libusb_close(udh);

    libusb_exit(NULL);

    return RIG_OK;
}

#else

//! @cond Doxygen_Suppress
int usb_port_open(hamlib_port_t *port)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}
//! @endcond


//! @cond Doxygen_Suppress
int usb_port_close(hamlib_port_t *port)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}
//! @endcond

#endif  /* defined(HAVE_LIBUSB) && defined(HAVE_LIBUSB_H) */

/** @} */
