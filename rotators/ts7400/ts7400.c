/*
 *  Hamlib ts7400 backend - main file
 *  Copyright (c) 2001-2009 by Stephane Fillod
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
#include <math.h>
#include <sys/time.h>

#include <hamlib/rotator.h>
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "ts7400.h"

struct ts7400_rot_priv_data
{
    azimuth_t az;
    elevation_t el;

    struct timeval tv;  /* time last az/el update */
    azimuth_t target_az;
    elevation_t target_el;
};



static int ts7400_rot_init(ROT *rot)
{
    struct ts7400_rot_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot->state.priv = (struct ts7400_rot_priv_data *)
                      calloc(1, sizeof(struct ts7400_rot_priv_data));

    if (!rot->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rot->state.priv;

    rot->state.rotport.type.rig = RIG_PORT_NONE;

    priv->az = priv->el = 0;

    priv->target_az = priv->target_el = 0;

    return RIG_OK;
}

static int ts7400_rot_cleanup(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rot->state.priv)
    {
        free(rot->state.priv);
    }

    rot->state.priv = NULL;

    return RIG_OK;
}

static int ts7400_rot_open(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int ts7400_rot_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int ts7400_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    struct ts7400_rot_priv_data *priv = (struct ts7400_rot_priv_data *)
                                        rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    priv->target_az = az;
    priv->target_el = el;

    gettimeofday(&priv->tv, NULL);

    return RIG_OK;
}


/*
 * Get position of rotor, simulating slow rotation
 */
static int ts7400_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    struct ts7400_rot_priv_data *priv = (struct ts7400_rot_priv_data *)
                                        rot->state.priv;
    struct timeval tv;
    unsigned elapsed; /* ms */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (priv->az == priv->target_az &&
            priv->el == priv->target_el)
    {
        *az = priv->az;
        *el = priv->el;
        return RIG_OK;
    }

    gettimeofday(&tv, NULL);

    elapsed = (tv.tv_sec - priv->tv.tv_sec) * 1000 +
              (tv.tv_usec - priv->tv.tv_usec) / 1000;

    /*
     * Simulate rotation speed of 360 deg per minute
     */
#define DEG_PER_MS (360./60/1000)

    if (fabs(priv->target_az - priv->az) / DEG_PER_MS <= elapsed)
    {
        /* target reached */
        priv->az = priv->target_az;
    }
    else
    {
        if (priv->az < priv->target_az)
        {
            priv->az += (azimuth_t)elapsed * DEG_PER_MS;
        }
        else
        {
            priv->az -= (azimuth_t)elapsed * DEG_PER_MS;
        }
    }

    if (fabs(priv->target_el - priv->el) / DEG_PER_MS <= elapsed)
    {
        /* target reached */
        priv->el = priv->target_el;
    }
    else
    {
        if (priv->el < priv->target_el)
        {
            priv->el += (elevation_t)elapsed * DEG_PER_MS;
        }
        else
        {
            priv->el -= (elevation_t)elapsed * DEG_PER_MS;
        }
    }

    *az = priv->az;
    *el = priv->el;

    priv->tv = tv;

    return RIG_OK;
}


static int ts7400_rot_stop(ROT *rot)
{
    struct ts7400_rot_priv_data *priv = (struct ts7400_rot_priv_data *)
                                        rot->state.priv;
    azimuth_t az;
    elevation_t el;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ts7400_rot_get_position(rot, &az, &el);

    priv->target_az = priv->az = az;
    priv->target_el = priv->el = el;

    return RIG_OK;
}


static int ts7400_rot_park(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Assume home is 0,0 */
    ts7400_rot_set_position(rot, 0, 0);

    return RIG_OK;
}

static int ts7400_rot_reset(ROT *rot, rot_reset_t reset)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int ts7400_rot_move(ROT *rot, int direction, int speed)
{
    struct ts7400_rot_priv_data *priv = (struct ts7400_rot_priv_data *)
                                        rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: Direction = %d, Speed = %d\n", __func__,
              direction, speed);

    switch (direction)
    {
    case ROT_MOVE_UP:
        return ts7400_rot_set_position(rot, priv->target_az, 90);

    case ROT_MOVE_DOWN:
        return ts7400_rot_set_position(rot, priv->target_az, 0);

    case ROT_MOVE_CCW:
        return ts7400_rot_set_position(rot, -180, priv->target_el);

    case ROT_MOVE_CW:
        return ts7400_rot_set_position(rot, 180, priv->target_el);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static const char *ts7400_rot_get_info(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "ts7400 rotator";
}



/*
 * ts7400 rotator capabilities.
 */

const struct rot_caps ts7400_rot_caps =
{
    ROT_MODEL(ROT_MODEL_TS7400),
    .model_name =     "ts7400",
    .mfg_name =       "LA7LKA",
    .version =        "20200113.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_NONE,

    .min_az =     -180.,
    .max_az =     180.,
    .min_el =     0.,
    .max_el =     90.,

    .priv =  NULL,    /* priv */

    .rot_init =     ts7400_rot_init,
    .rot_cleanup =  ts7400_rot_cleanup,
    .rot_open =     ts7400_rot_open,
    .rot_close =    ts7400_rot_close,

    .set_position =     ts7400_rot_set_position,
    .get_position =     ts7400_rot_get_position,
    .park =     ts7400_rot_park,
    .stop =     ts7400_rot_stop,
    .reset =    ts7400_rot_reset,
    .move =     ts7400_rot_move,

    .get_info =      ts7400_rot_get_info,
};

DECLARE_INITROT_BACKEND(ts7400)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&ts7400_rot_caps);

    return RIG_OK;
}
