/*
 *  Hamlib Rotator backend - IF-100 parallel port
 *  Copyright (c) 2011 by Stephane Fillod
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

#include <math.h>

#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif

#include <hamlib/rotator.h>
#include "parallel.h"
#include "misc.h"
#include "register.h"

static int
if100_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    hamlib_port_t *port = &rot->state.rotport;
    int retval;
    int az_i;
    int el_i;
    int dataout, i;
    double az_scale, el_scale;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    az_scale = 255. / (rot->state.max_az - rot->state.min_az);
    el_scale = 255. / 180;

    az_i = (int)round((az - rot->state.min_az) * az_scale);
    el_i = (int)round(el * el_scale);

    rig_debug(RIG_DEBUG_TRACE, "%s output az: %d el: %d\n", __func__, az_i, el_i);
    dataout = ((el_i & 0xff) << 8) + (az_i & 0xff);

#define DAT0  0x01
#define CLK   0x02
#define TRACK 0x08

    rig_debug(RIG_DEBUG_TRACE,
              "%s: shifting dataout 0x%04x to parallel port\n",
              __func__, dataout);

    retval = par_lock(port);

    if (retval != RIG_OK)
    {
        return retval;
    }

    for (i = 0; i < 16; i++)
    {
        if (dataout & 0x8000)
        {
            par_write_data(port, TRACK | DAT0);
            par_write_data(port, TRACK | DAT0 | CLK);
            par_write_data(port, TRACK | DAT0);
        }
        else
        {
            par_write_data(port, TRACK);
            par_write_data(port, TRACK | CLK);
            par_write_data(port, TRACK);
        }

        dataout = (dataout << 1) & 0xffff;
    }

    par_write_data(port, TRACK);
    par_unlock(port);

    return RIG_OK;
}


/** IF-100 implements essentially only the set position function.
 */
const struct rot_caps if100_rot_caps =
{
    ROT_MODEL(ROT_MODEL_IF100),
    .model_name =       "IF-100",
    .mfg_name =         "AMSAT",
    .version =          "20110531.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_BETA,
    .rot_type =         ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_PARALLEL,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          200,
    .retry =            3,

    .min_az =           0,
    .max_az =           360,
    .min_el =           0,
    .max_el =           180,

    .set_position =     if100_set_position,
};


/* ************************************************************************* */

DECLARE_INITROT_BACKEND(amsat)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&if100_rot_caps);

    return RIG_OK;
}
