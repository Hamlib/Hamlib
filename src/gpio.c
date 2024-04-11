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

#include "gpio.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gpiod.h>

#ifndef GPIOD_PATH
// This is what's used on the Raspberry Pi 4; I'm not sure about others
#define GPIO_CHIP_NAME "gpiochip0"
#endif

#define GPIO_CHIP_CONSUMER "Hamlib"

int gpio_open(hamlib_port_t *port, int output, int on_value)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;

    port->parm.gpio.on_value = on_value;

    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);

    if (!chip)
    {
        rig_debug(RIG_DEBUG_ERR, "Failed to open GPIO chip %s: %s\n", GPIO_CHIP_NAME, strerror(errno));
        return -RIG_EIO;
    }

    line = gpiod_chip_get_line(chip, atoi(port->pathname));
    if (!line)
    {
        rig_debug(RIG_DEBUG_ERR, "Failed to acquire GPIO%s: %s\n", port->pathname, strerror(errno));
        gpiod_chip_close(chip);
        return -RIG_EIO;
    }

    if ((output && gpiod_line_request_output(line, GPIO_CHIP_CONSUMER, 0) < 0) ||
        (!output && gpiod_line_request_input(line, GPIO_CHIP_CONSUMER) < 0))
    {
        rig_debug(RIG_DEBUG_ERR, "Failed to set GPIO%s to %s mode: %s\n",
                port->pathname, (output ? "OUTPUT" : "INPUT"), strerror(errno));
        gpiod_line_release(line);
        gpiod_chip_close(chip);
        return -RIG_EIO;
    }

    port->gpio = line;

    return RIG_OK;
}

int gpio_close(hamlib_port_t *port)
{
    gpiod_line_close_chip((struct gpiod_line*)port->gpio);
    return RIG_OK;
}

int gpio_ptt_set(hamlib_port_t *port, ptt_t pttx)
{
    int result = 0;
    port->parm.gpio.value = pttx != RIG_PTT_OFF;

    if ((port->parm.gpio.value && port->parm.gpio.on_value) ||
      (!port->parm.gpio.value && !port->parm.gpio.on_value))
    {
        result = gpiod_line_set_value((struct gpiod_line*)port->gpio, 1);
    }
    else
    {
        result = gpiod_line_set_value((struct gpiod_line*)port->gpio, 0);
    }

    if (result)
    {
        rig_debug(RIG_DEBUG_ERR, "Failed to set the value of GPIO%s: %s\n", port->pathname, strerror(errno));
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
    int val = gpiod_line_get_value((struct gpiod_line*)port->gpio);
    if (val < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "Failed to read the value of GPIO%s: %s\n", port->pathname, strerror(errno));
    }

    rig_debug(RIG_DEBUG_VERBOSE, "DCD GPIO pin value: %c\n", val);

    if (val == port->parm.gpio.on_value)
    {
        *dcdx = RIG_DCD_ON;
    }
    else
    {
        *dcdx = RIG_DCD_OFF;
    }

    return RIG_OK;
}
