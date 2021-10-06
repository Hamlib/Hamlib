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

struct androidsensor_rot_priv_data
{
    NdkImu *imu;
    azimuth_t target_az;
    elevation_t target_el;
};

static int
androidsensor_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);
    struct androidsensor_rot_priv_data *priv = (struct androidsensor_rot_priv_data *)rot->state.priv;

    priv->target_az = az;
    priv->target_el = el;
    return RIG_OK;
}

static int
androidsensor_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called: ", __func__);
    struct androidsensor_rot_priv_data *priv = (struct androidsensor_rot_priv_data *)rot->state.priv;

    priv->imu->update();

    float accData[3], magData[3];
    priv->imu->getAccData(accData);
    priv->imu->getMagData(magData);

    float R[9] = {0}, I[9] = {0}, orientation[3] = {0};
    bool status = priv->imu->getRotationMatrix(R, 9, I, 9, accData, magData);
    if(status)
        priv->imu->getOrientation(R, 9, orientation);

    *az = (orientation[0] / M_PI * 180 );
    *el = (orientation[1] / M_PI * 180 ) * -1;

    rig_debug(RIG_DEBUG_TRACE, "%f %f\n", *az, *el);

    return RIG_OK;
}

static int
androidsensor_rot_init(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called, make new NdkImu\n", __func__);
    rot->state.priv = (struct androidsensor_rot_priv_data *)malloc(sizeof(struct androidsensor_rot_priv_data));
    struct androidsensor_rot_priv_data *priv = (struct androidsensor_rot_priv_data *)rot->state.priv;

    if (!rot->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv->imu = new NdkImu();
    priv->target_az = 0;
    priv->target_el = 0;
    return RIG_OK;
}

static int
androidsensor_rot_cleanup(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called, delete imu\n", __func__);
    struct androidsensor_rot_priv_data *priv = (struct androidsensor_rot_priv_data *)rot->state.priv;

    delete priv->imu;
    free(rot->state.priv);
    return RIG_OK;
}

/* ************************************************************************* */

const struct rot_caps androidsensor_rot_caps =
{
    ROT_MODEL(ROT_MODEL_ANDROIDSENSOR),
    .model_name =     "ACC/MAG",
    .mfg_name =       "Android Sensor",
    .version =        "20211006.0",
    .copyright =      "LGPL",
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_NONE,

    .min_az =     -180.0,
    .max_az =     180.0,
    .min_el =     -180.0,
    .max_el =     180.0,

    .priv =  NULL,    /* priv */

    .rot_init =     androidsensor_rot_init,
    .rot_cleanup =  androidsensor_rot_cleanup,
    .set_position = androidsensor_rot_set_position,
    .get_position = androidsensor_rot_get_position,
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
