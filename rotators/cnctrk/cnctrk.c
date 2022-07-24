/*
 *  Hamlib Rotator backend - LinuxCNC no hardware port
 *  Copyright (c) 2015 by Robert Freeman
 *  Adapted from AMSAT code by Stephane Fillod
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

#include "hamlib/rotator.h"
#include "misc.h"
#include "register.h"

char axcmd[512];

static int
cnctrk_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    int retval;

    retval = system("/usr/bin/axis-remote --ping");

    if (retval != 0)
    {
        return retval;
    }

    SNPRINTF(axcmd, sizeof(axcmd),
             "/usr/bin/axis-remote --mdi 'G00 X %6.2f Y %6.2f' \n", az, el);
    return system(axcmd);
}


/** CNCTRK implements essentially only the set position function.
    it assumes there is a LinuxCNC running with the Axis GUI */
const struct rot_caps cnctrk_rot_caps =
{
    ROT_MODEL(ROT_MODEL_CNCTRK),
    .model_name =     "CNCTRK",
    .mfg_name =       "CNCTRK",
    .version =        "20220109.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .min_az =     0,
    .max_az =     360,
    .min_el =     -20,
    .max_el =     180,

    .set_position =  cnctrk_set_position,
};


/* ************************************************************************* */

DECLARE_INITROT_BACKEND(cnctrk)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&cnctrk_rot_caps);

    return RIG_OK;
}
