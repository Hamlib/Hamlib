/*
 *  Hamlib Rotator backend - Fodtrack parallel port
 *  Copyright (c) 2001-2010 by Stephane Fillod
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

#include <stdlib.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_PARALLEL
#ifdef HAVE_LINUX_PARPORT_H
#include <linux/parport.h>
#endif
#endif

#include "hamlib/rotator.h"
#include "parallel.h"
#include "misc.h"
#include "register.h"

#include "fodtrack.h"

#ifndef CP_ACTIVE_LOW_BITS
#define CP_ACTIVE_LOW_BITS 0x0B
#endif

/* ************************************************************************* */

/** outputs an direction to the interface */
static int setDirection(hamlib_port_t *port, unsigned char outputvalue,
                        int direction)
{
    unsigned char outputstatus;

    par_lock(port);

    // set the data bits
    par_write_data(port, outputvalue);

    // autofd=true --> azimuth otherwise elevation
    if (direction)
    {
        outputstatus = PARPORT_CONTROL_AUTOFD;
    }
    else
    {
        outputstatus = 0;
    }

    par_write_control(port, outputstatus ^ CP_ACTIVE_LOW_BITS);
    // and now the strobe impulse
    hl_usleep(1);

    if (direction)
    {
        outputstatus = PARPORT_CONTROL_AUTOFD | PARPORT_CONTROL_STROBE;
    }
    else
    {
        outputstatus = PARPORT_CONTROL_STROBE;
    }

    par_write_control(port, outputstatus ^ CP_ACTIVE_LOW_BITS);
    hl_usleep(1);

    if (direction)
    {
        outputstatus = PARPORT_CONTROL_AUTOFD;
    }
    else
    {
        outputstatus = 0;
    }

    par_write_control(port, outputstatus ^ CP_ACTIVE_LOW_BITS);

    par_unlock(port);

    return RIG_OK;
}

static int
fodtrack_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    int retval;
    hamlib_port_t *pport;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    pport = &rot->state.rotport;

    retval = setDirection(pport, el / (float)rot->state.max_el * 255.0, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = setDirection(pport, az / (float)rot->state.max_az * 255.0, 1);

    if (retval != RIG_OK)
    {
        return retval;
    }


    return RIG_OK;
}


/* ************************************************************************* */
/*
 * Fodtrack rotator capabilities.
 */

/** Fodtrack implement essentially only the set position function.
 */
const struct rot_caps fodtrack_rot_caps =
{
    ROT_MODEL(ROT_MODEL_FODTRACK),
    .model_name =     "Fodtrack",
    .mfg_name =       "XQ2FOD",
    .version =        "20200107.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_PARALLEL,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .min_az =     0,
    .max_az =     450,
    .min_el =     0,
    .max_el =     180,

    .priv =  NULL,    /* priv */

    .set_position =  fodtrack_set_position,
};


/* ************************************************************************* */

DECLARE_INITROT_BACKEND(fodtrack)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&fodtrack_rot_caps);

    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
