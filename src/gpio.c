/*
 *  Hamlib Interface - gpio support
 *  Copyright (c) 2016 by Jeroen Vreeken
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gpio.h"


int gpio_open(hamlib_port_t *port, int output, int on_value)
{
    char pathname[HAMLIB_FILPATHLEN * 2];
    FILE *fexp, *fdir;
    int fd;
    char *dir;

    port->parm.gpio.on_value = on_value;

    SNPRINTF(pathname, HAMLIB_FILPATHLEN, "/sys/class/gpio/export");
    fexp = fopen(pathname, "w");

    if (!fexp)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "Export GPIO%s (using %s): %s\n",
                  port->pathname,
                  pathname,
                  strerror(errno));
        return -RIG_EIO;
    }

    fprintf(fexp, "%s\n", port->pathname);
    fclose(fexp);

    SNPRINTF(pathname,
             sizeof(pathname),
             "/sys/class/gpio/gpio%s/direction",
             port->pathname);
    fdir = fopen(pathname, "w");

    if (!fdir)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "GPIO%s direction (using %s): %s\n",
                  port->pathname,
                  pathname,
                  strerror(errno));
        return -RIG_EIO;
    }

    dir = output ? "out" : "in";
    rig_debug(RIG_DEBUG_VERBOSE, "Setting direction of GPIO%s to %s\n",
              port->pathname, dir);
    fprintf(fdir, "%s\n", dir);
    fclose(fdir);

    SNPRINTF(pathname,
             sizeof(pathname),
             "/sys/class/gpio/gpio%s/value",
             port->pathname);
    fd = open(pathname, O_RDWR);

    if (fd < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "GPIO%s opening value file %s: %s\n",
                  port->pathname,
                  pathname,
                  strerror(errno));
        return -RIG_EIO;
    }

    port->fd = fd;
    return fd;
}


int gpio_close(hamlib_port_t *port)
{
    return close(port->fd);
}


int gpio_ptt_set(hamlib_port_t *port, ptt_t pttx)
{
    char *val;
    port->parm.gpio.value = pttx != RIG_PTT_OFF;

    if ((port->parm.gpio.value && port->parm.gpio.on_value)
            || (!port->parm.gpio.value && !port->parm.gpio.on_value))
    {
        val = "1\n";
    }
    else
    {
        val = "0\n";
    }

    if (write(port->fd, val, strlen(val)) <= 0)
    {
        return -RIG_EIO;
    }

    return RIG_OK;
}


int gpio_ptt_get(hamlib_port_t *port, ptt_t *pttx)
{
    if (port->parm.gpio.value)
    {
        *pttx = RIG_PTT_ON;
    }
    else
    {
        *pttx = RIG_PTT_OFF;
    }

    return RIG_OK;
}

int gpio_dcd_get(hamlib_port_t *port, dcd_t *dcdx)
{
    char val;
    int port_value;

    lseek(port->fd, 0, SEEK_SET);

    if (read(port->fd, &val, sizeof(val)) <= 0)
    {
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "DCD GPIO pin value: %c\n", val);

    port_value = val - '0';

    if (port_value == port->parm.gpio.on_value)
    {
        *dcdx = RIG_DCD_ON;
    }
    else
    {
        *dcdx = RIG_DCD_OFF;
    }

    return RIG_OK;
}

