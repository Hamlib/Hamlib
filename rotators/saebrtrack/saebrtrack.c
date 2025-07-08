/*
 * Hamlib Rotator backend - SAEBRTrack
 * Copyright (c) 2023 by Matthew J Wolf
 * Contributed by Matthew J Wolf <mwolf at speciosus.net>
 *
 *  Hamlib Rotator backend - Easycom
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

#include <stdio.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rotator.h"
#include "serial.h"
#include "register.h"

#include "saebrtrack.h"

/* ************************************************************************* */
/**
 *  saebrtrack_transaction
 *
 *  Assumes rot!=NULL and cmdstr!=NULL
 *
 *  cmdstr   - string to send to rotator
 *  data     - buffer for reply string
 *  data_len - (input) Maximum size of buffer
 *             (output) Number of bytes read.
 */
static int
saebrtrack_transaction(ROT *rot, const char *cmdstr, char *data,
                       size_t data_len)
{
    hamlib_port_t *rotp = ROTPORT(rot);
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %s\n", __func__, cmdstr);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rig_flush(rotp);
    retval = write_block(rotp, (unsigned char *) cmdstr, strlen(cmdstr));

    if (retval != RIG_OK)
    {
        goto transaction_quit;
    }

    if (data == NULL)
    {
        return RIG_OK;    /* don't want a reply */
    }

    retval = read_string(rotp, (unsigned char *) data, data_len,
                         "\n", 1, 0, 1);

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
saebrtrack_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[64];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    /*  Non Easycom standard */
    SNPRINTF(cmdstr, sizeof(cmdstr), "AZ%05.1f EL%05.1f UP000 XXX DN000 XXX\n", az,
             el);

    retval = saebrtrack_transaction(rot, cmdstr, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TODO: Error processing */
    return RIG_OK;
}

/*
 * Get Info
 * returns the model name string
 */
// cppcheck-suppress constParameterCallback
static const char *saebrtrack_rot_get_info(ROT *rot)
{
    const struct rot_caps *rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return (const char *) - RIG_EINVAL;
    }

    rc = rot->caps;

    return rc->model_name;
}


/* ************************************************************************* */
/*
 * saebrtrack rotator capabilities.
 */

/** saebrtrackI implement essentially only the set position function, but
 *  I included the stop command too.  The radio control tags is only included
 *  as dummy entries because the spec require them.
 */
const struct rot_caps saebrtrack_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SAEBRTRACK),
    .model_name =     "SAEBRTrack",
    .mfg_name =       "Hamlib",
    .version =        "20200810.0",
    .copyright =   "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  19200,
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

    .set_position =  saebrtrack_rot_set_position,
    .get_info =  saebrtrack_rot_get_info,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(saebrtrack)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&saebrtrack_rot_caps);
    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
