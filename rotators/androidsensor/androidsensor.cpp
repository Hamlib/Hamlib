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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rotator.h"
#include "register.h"

#include "androidsensor.h"
#include "ndkimu.cpp"

/* ************************************************************************* */

static NdkImu *imu;

static int
androidsensor_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);
    // To-Do
    return RIG_OK;
}

static int
androidsensor_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called: ", __func__);

    imu->update();

    float accData[3], magData[3];
    imu->getAccData(accData);
    imu->getMagData(magData);

    float R[9] = {0}, I[9] = {0}, orientation[3] = {0};
    bool status = imu->getRotationMatrix(R, 9, I, 9, accData, magData);
    if(status)
        imu->getOrientation(R, 9, orientation);

    *az = (orientation[0] / M_PI * 180 );
    *el = (orientation[1] / M_PI * 180 ) * -1;

    rig_debug(RIG_DEBUG_TRACE, "%f %f\n", az, el);

    return RIG_OK;
}

static int
androidsensor_rot_stop(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);
    // To-Do
    return RIG_OK;
}

static int
androidsensor_rot_init(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called, make new NdkImu\n", __func__);
    imu = new NdkImu();
    return RIG_OK;
}

static int
androidsensor_rot_cleanup(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called, delete imu\n", __func__);
    delete imu;
    return RIG_OK;
}

/* ************************************************************************* */

const struct rot_caps androidsensor_rot_caps =
{
    ROT_MODEL(ROT_MODEL_ANDROIDSENSOR),
    .model_name =     "ACC/MAG",
    .mfg_name =       "Android Sensor",
    .version =        "20210925.0",
    .copyright =      "LGPL",
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_NONE,

    .min_az =     -180.0,
    .max_az =     180.0,
    .min_el =     -180.0,
    .max_el =     180.0,

    .priv =  NULL,    /* priv */

    .set_position = androidsensor_rot_set_position,
    .get_position = androidsensor_rot_get_position,
    .stop =         androidsensor_rot_stop,
    .rot_init =     androidsensor_rot_init,
    .rot_cleanup =  androidsensor_rot_cleanup,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(androidsensor)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&androidsensor_rot_caps);
    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
