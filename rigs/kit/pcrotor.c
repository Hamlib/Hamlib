/*
 *  Hamlib Rotator backend - PcRotor/WA6UFQ parallel port
 *  Copyright (c) 2001-2008 by Stephane Fillod
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

#include "hamlib/rotator.h"
#include "parallel.h"
#include "misc.h"


/* ************************************************************************* */
//pcrotor_set_position(ROT *rot, azimuth_t az, elevation_t el)

#define PCROTOR_POWER 0x20
#define PCROTOR_CW    0x40
#define PCROTOR_CCW   0x80
#define PCROTOR_MASK (PCROTOR_CCW|PCROTOR_CW|PCROTOR_POWER)

static int setDirection(hamlib_port_t *port, unsigned char outputvalue)
{
    int ret;

    par_lock(port);

    /* set the data bits.
     * Should we read before write to not trample the lower significant bits?
     */
    ret = par_write_data(port, outputvalue);

    par_unlock(port);

    return ret;
}


static int
pcrotor_stop(ROT *rot)
{
    /* CW=0, CCW=0, Power-up=0 */
    return setDirection(&rot->state.rotport, 0);
}

static int
pcrotor_move(ROT *rot, int direction, int speed)
{
    unsigned char outputvalue;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %d %d\n", __func__, direction, speed);

    switch (direction)
    {
    case ROT_MOVE_CCW:
        outputvalue = PCROTOR_POWER | PCROTOR_CCW;
        break;

    case ROT_MOVE_CW:
        outputvalue = PCROTOR_POWER | PCROTOR_CCW;
        break;

    case 0:
        /* Stop */
        outputvalue = 0;
        break;

    default:
        return -RIG_EINVAL;
    }

    return setDirection(&rot->state.rotport, outputvalue);
}


/* ************************************************************************* */
/*
 * PcRotor rotator capabilities.
 *
 * Control Interface schematics from, courtersy of Bob Hillard WA6UFQ:
 *   http://www.dxzone.com/cgi-bin/dir/jump2.cgi?ID=11173
 *
 * DB25-7=Data-5= Power up/Sleep
 * DB25-8=Data-6= CW
 * DB25-9=Data-7= CCW
 *
 * There's no feedback.
 */

/** Fodtrack implement essentially only the set position function.
 *
 */
const struct rot_caps pcrotor_caps =
{
    ROT_MODEL(ROT_MODEL_PCROTOR),
    .model_name =     "PcRotor",
    .mfg_name =       "WA6UFQ",
    .version =        "20081013.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_PARALLEL,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .min_az =     0,
    .max_az =     360,
    .min_el =     0,
    .max_el =     0,

    .priv =  NULL,    /* priv */

    .move =  pcrotor_move,
    .stop =  pcrotor_stop,
    //.set_position =  pcrotor_set_position,
    //.get_position =  pcrotor_get_position,
};

/* end of file */
