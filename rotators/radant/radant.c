/*
 *  Hamlib Rotator backend - Radant
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *  Copyright (c) 2014 by Alexander Schultze <alexschultze@gmail.com>
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
#include <string.h>  /* String function definitions */

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "idx_builtin.h"

#include "radant.h"

//#define EASYCOMM3_LEVELS ROT_LEVEL_SPEED

/* ************************************************************************* */
/**
 *  radant_transaction
 *
 *  Assumes rot!=NULL and cmdstr!=NULL
 *
 *  cmdstr   - string to send to rotator
 *  data     - buffer for reply string
 *  data_len - (input) Maximum size of buffer
 *             (output) Number of bytes read.
 */
static int
radant_transaction(ROT *rot, const char *cmdstr, char *data, size_t data_len)
{
    struct rot_state *rs;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %s\n", __func__, cmdstr);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rs = &rot->state;
    rig_flush(&rs->rotport);
    retval = write_block(&rs->rotport, (unsigned char *) cmdstr, strlen(cmdstr));

    if (retval != RIG_OK)
    {
        goto transaction_quit;
    }

    if (data == NULL)
    {
        return RIG_OK;    /* don't want a reply */
    }

    retval = read_string(&rs->rotport, (unsigned char *) data, data_len, "\n", 1, 0,
                         1);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s read_string failed with status %d\n", __func__,
                  retval);
        goto transaction_quit;
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s read_string: %s\n", __func__, data);
        retval = RIG_OK;
    }

transaction_quit:
    return retval;
}

/* ************************************************************************* */

static int
radant_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[32];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);


    SNPRINTF(cmdstr, sizeof(cmdstr), "Q%.1f %1.f\r", az, el);

    retval = radant_transaction(rot, cmdstr, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TODO: Error processing */
    return RIG_OK;
}

static int
radant_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char cmdstr[4], ackbuf[16];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    SNPRINTF(cmdstr, sizeof(cmdstr), "Y\r");

    retval = radant_transaction(rot, cmdstr, ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s got error: %d\n", __func__, retval);
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s got response: %s\n", __func__, ackbuf);
    retval = sscanf(ackbuf, "OK%f %f\r", az, el);

    if (retval != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown response (%s)\n", __func__, ackbuf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

static int
radant_rot_stop(ROT *rot)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    retval = radant_transaction(rot, "S\r", NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TODO: error processing */

    return RIG_OK;
}

/* ************************************************************************* */
/*
 * Radant rotator capabilities.
 */

/** Radant implement set and get position function and stop, but
 *  there is no velocity and other configurations functions
 */
const struct rot_caps radant_rot_caps =
{
    ROT_MODEL(ROT_MODEL_RADANT),
    .model_name =     "AZ-1/AZV-1",
    .mfg_name =       "Radant",
    .version =        "20210508.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .priv =  NULL,    /* priv */

    .set_position =  radant_rot_set_position,
    .get_position = radant_rot_get_position,
    .stop =   radant_rot_stop,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(radant)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&radant_rot_caps);
    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
