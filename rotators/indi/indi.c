/*
 *  Hamlib Rotator backend - INDI integration
 *  Copyright (c) 2020 by Norbert Varga HA2NON <nonoo@nonoo.hu>
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

#include "indi_wrapper.h"

#include <hamlib/rotator.h>
#include <register.h>

const struct rot_caps indi_rot_caps =
{
    ROT_MODEL(ROT_MODEL_INDI),
    .model_name =       "INDI",
    .mfg_name =         "INDI",
    .version =          "0.1",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rot_type =         ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          200,
    .retry =            3,

    .min_az =           0,
    .max_az =           360,
    .min_el =           -90,
    .max_el =           90,

    .set_position =     indi_wrapper_set_position,
    .get_position =     indi_wrapper_get_position,
    .stop =             indi_wrapper_stop,
    .park =             indi_wrapper_park,
    .move =             indi_wrapper_move,
    .get_info =         indi_wrapper_get_info,
    .rot_open =         indi_wrapper_open,
    .rot_close =        indi_wrapper_close,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(indi)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&indi_rot_caps);

    return RIG_OK;
}
