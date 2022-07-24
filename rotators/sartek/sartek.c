/*
 * Hamlib backend library for the SARtek rotor command set.
 *
 * sartek.c - (C) Stephane Fillod 2003
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

#include <hamlib/config.h>

#include <stdio.h>
#include <string.h>             /* String function definitions */

#include "hamlib/rotator.h"
#include "serial.h"
#include "register.h"

#include "sartek.h"


/* *************************************
 *
 * Separate model capabilities
 *
 * *************************************
 */


/*
 * SARtek rotor capabilities
 *
 * Documentation from:
 *  http://www.hosenose.com/sartek/ifcspecs.htm
 */

const struct rot_caps sartek_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SARTEK1),
    .model_name =         "SARtek-1",
    .mfg_name =           "SARtek",
    .version =            "20061007.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_BETA,
    .rot_type =           ROT_TYPE_OTHER,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    1200,
    .serial_rate_max =    1200,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        0,
    .post_write_delay =   0,
    .timeout =            1000,
    .retry =              3,

    .min_az =             0,
    .max_az =             360,
    .min_el =             0,
    .max_el =             0,

    .priv =  NULL,    /* priv */

    .set_position =       sartek_rot_set_position,
    .stop =               sartek_rot_stop,
};



/* ************************************
 *
 * API functions
 *
 * ************************************
 */



/*
 * Set position
 * SARtek protocol supports azimuth only--elevation is ignored
 * Range is converted to an integer string, 0 to 360 degrees
 */

static int sartek_rot_set_position(ROT *rot, azimuth_t azimuth,
                                   elevation_t elevation)
{
    char cmdstr[8];
    int err;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (azimuth < 0 || azimuth > 360)
    {
        return -RIG_EINVAL;
    }

    if (azimuth < 2)
    {
        azimuth = 2;
    }
    else if (azimuth > 358)
    {
        azimuth = 358;
    }

    SNPRINTF(cmdstr, sizeof(cmdstr), "P%c", (int)((azimuth * 255) / 360));

    err = write_block(&rot->state.rotport, (unsigned char *) cmdstr,
                      strlen(cmdstr));

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}



/*
 * Stop rotation
 */

static int sartek_rot_stop(ROT *rot)
{
    int err;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    err = write_block(&rot->state.rotport, (unsigned char *) "P\0", 2);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}



/*
 * Initialize backend
 */

DECLARE_INITROT_BACKEND(sartek)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&sartek_rot_caps);

    return RIG_OK;
}

