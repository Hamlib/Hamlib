/*
 *  Hamlib Sat/El backend - main file
 *  Copyright (c) 2021 Joshua Lynch
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

#include <strings.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "satel.h"


static int satel_rot_open(ROT *rot)
{
    #define RES_BUF_SIZE 256
    char buf[RES_BUF_SIZE];
    int ret;
    struct rot_state *rs;


    
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    rs = &rot->state;

    rig_flush(&rs->rotport);

    // check if we're connected to the rotator
    ret = write_block(&rs->rotport, "?", 1);
    if (ret != RIG_OK)
        return ret;

    ret = read_string(&rs->rotport, buf, RES_BUF_SIZE, "\n", 1);
    if (ret < 0)
        return ret;

    ret = strncasecmp("SatEL", buf, 5); 
    if (ret != 0)
        return RIG_EIO;

    // yep, now enable motion
    ret = write_block(&rs->rotport, "g", 1);
    if (ret != RIG_OK)
        return ret;

    
    return RIG_OK;
}

static int satel_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
#define BUF_CMD_SIZE 20
    char buf[BUF_CMD_SIZE];
    struct rot_state *rs;

    
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    
    rs = &rot->state;

    rig_flush(&rs->rotport);

    snprintf(buf, BUF_CMD_SIZE, "p%03d %03d\r\n", (int)az, (int)el);
    return write_block(&rs->rotport, buf, strlen(buf));
}

static int satel_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    #define RES_BUF_SIZE 256
    char buf[RES_BUF_SIZE];
    char *p;
    int ret;
    struct rot_state *rs;


    
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    
    rs = &rot->state;

    rig_flush(&rs->rotport);
    
    ret = write_block(&rs->rotport, "z", 1);
    if (ret != RIG_OK)
        return ret;


    // skip header information
    for (int i = 0; i < 3; i++)
    {
        ret = read_string(&rs->rotport, buf, RES_BUF_SIZE, "\n", 1);
        if (ret < 0)
            return ret;
    }

    
    // read azimuth line
    ret = read_string(&rs->rotport, buf, RES_BUF_SIZE, "\n", 1);
    if (ret < 0)
        return ret;

    p = buf + 10;
    p[3] = '\0';
    *az = strtof(p, NULL);

    rig_debug(RIG_DEBUG_VERBOSE, "AZIMUTH %f[%s]", *az, p);
    
    
    // read elevation line
    ret = read_string(&rs->rotport, buf, RES_BUF_SIZE, "\n", 1);
    if (ret < 0)
        return ret;

    p = buf + 12;
    p[3] = '\0';
    *el = strtof(p, NULL);

    rig_debug(RIG_DEBUG_VERBOSE, "ELEVATION %f[%s]", *el, p);
    
    // skip trailer information
    for (int i = 0; i < 2; i++)
    {
        ret = read_string(&rs->rotport, buf, RES_BUF_SIZE, "\n", 1);
        if (ret < 0)
            return ret;
    }
    

    return RIG_OK;
}


static int satel_rot_stop(ROT *rot)
{
    struct rot_state *rs;

    
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    
    rs = &rot->state;

    rig_flush(&rs->rotport);
    
    return write_block(&rs->rotport, "*", 1);
}

static const char *satel_rot_get_info(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "Satel rotator";
}

/*
 * Satel rotator capabilities.
 */
const struct rot_caps satel_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SATEL),
    .model_name       = "SatEL",
    .mfg_name         = "SatEL",
    .version          = "20210123.0",
    .copyright        = "LGPL",
    .status           = RIG_STATUS_ALPHA,
    .rot_type         = ROT_TYPE_AZEL,
    .port_type        = RIG_PORT_SERIAL,
    .serial_rate_max  = 9600,
    .serial_rate_min  = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 250,
    .post_write_delay = 0,
    .timeout          = 1000,
    .retry            = 0,
    .min_az           = 0.,
    .max_az           = 360.,
    .min_el           = 0.,
    .max_el           = 90.,
    .rot_open         = satel_rot_open,
    .get_position     = satel_rot_get_position,
    .set_position     = satel_rot_set_position,
    .stop             = satel_rot_stop,
    .get_info         = satel_rot_get_info,
    .priv             = NULL,   /* priv */
};

DECLARE_INITROT_BACKEND(satel)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&satel_rot_caps);

    return RIG_OK;
}
