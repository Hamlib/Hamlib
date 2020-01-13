/*
 *  Hamlib Ether6 backend - main file
 *  Copyright (c) 2001-2009 by Stephane Fillod
 *  Copyright (c) 2013 by Jonny Röker <Jonny.Roeker@t-online.de>
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

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include <hamlib/rotator.h>
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "ether6.h"

#define CMD_MAX 32
#define BUF_MAX 64

/*
 * Helper function with protocol return code parsing
 */
static int ether_transaction(ROT *rot, char *cmd, int len, char *buf)
{
    int ret;

    ret = write_block(&rot->state.rotport, cmd, len);
    rig_debug(RIG_DEBUG_VERBOSE, "function %s(1): ret=%d || send=%s\n", __func__,
              ret, cmd);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
    rig_debug(RIG_DEBUG_VERBOSE, "function %s(2): ret=%d || receive=%s\n", __func__,
              ret, buf);

    if (ret < 0)
    {
        return ret;
    }

    if (!memcmp(buf, ROTORCTL_RET, strlen(ROTORCTL_RET)))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "function %s(2a): receive=%s\n", __func__, buf);
        return RIG_OK;
    }

    if (!memcmp(buf, NETROTCTL_RET, strlen(NETROTCTL_RET)))
    {
        int rv = atoi(buf + strlen(NETROTCTL_RET));
        rig_debug(RIG_DEBUG_VERBOSE, "function %s(2): ret=%d || receive=%d\n", __func__,
                  ret, rv);
        return atoi(buf + strlen(NETROTCTL_RET));
    }

    return ret;
}

static int ether_rot_open(ROT *rot)
{
    int ret, len;
    int sval;
    float min_az, max_az, min_el, max_el;
    struct rot_state *rs = &rot->state;

    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    /* elevation not need */

    len = sprintf(cmd, "rotor state\n");
    /*-180/180 0/90*/

    ret = ether_transaction(rot, cmd, len, buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    sval = sscanf(buf, "%f/%f %f/%f", &min_az, &max_az, &min_el, &max_el);
    rs->min_az = min_az;
    rs->max_az = max_az;
    rs->min_el = min_el;
    rs->max_el = max_el;
    rig_debug(RIG_DEBUG_VERBOSE, "ret(%d)%f/%f %f/%f\n", sval, rs->min_az,
              rs->max_az, rs->min_el, rs->max_el);

    return RIG_OK;
}

static int ether_rot_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* clean signoff, no read back */
    write_block(&rot->state.rotport, "\n", 1);

    return RIG_OK;
}

static int ether_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %f %f\n", __func__,
              az, el);

    len = sprintf(cmd, "rotor move %d %d\n", (int)az, (int)el);

    ret = ether_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int ether_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    int ret, len, sval, speed, adv;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char mv[5];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "rotor status\n");

    ret = ether_transaction(rot, cmd, len, buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    // example "hold,az=87,el=0,v=8,ad0=346"
    sval = sscanf(buf, "%4s az=%f el=%f v=%d ad0=%d", mv, az, el, &speed, &adv);
    rig_debug(RIG_DEBUG_VERBOSE, "az=%f el=%f mv=%s ad(az)=%d\n", *az, *el, mv,
              adv);

    if (sval == 5)
    {
        return RIG_OK;
    }
    else
    {
        return -RIG_EPROTO;
    }
}

/**
* stop the rotor
*/
static int ether_rot_stop(ROT *rot)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "rotor stop\n");

    ret = ether_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


/**
* park the rotor
*/
static int ether_rot_park(ROT *rot)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "rotor park\n");

    ret = ether_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int ether_rot_reset(ROT *rot, rot_reset_t reset)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // orig len = sprintf(cmd, "R %d\n", reset);
    len = sprintf(cmd, "reset\n");

    ret = ether_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

/**
* call rotor cw or rotor ccw
* if direction value 0 turn cw and if direction value 1 turn ccw
*/
static int ether_rot_move(ROT *rot, int direction, int speed)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (direction == 0)
    {
        len = sprintf(cmd, "rotor cw %d\n", speed);
    }
    else
    {
        len = sprintf(cmd, "rotor ccw %d\n", speed);
    }

    ret = ether_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static const char *ether_rot_get_info(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "ip rotator via ethersex";
}



/*
 * Dummy rotator capabilities.
 */

const struct rot_caps ether6_rot_caps =
{
    .rot_model =      ROT_MODEL_ETHER6,
    .model_name =     "Ether6 (via ethernet)",
    .mfg_name =       "DG9OAA",
    .version =        "0.1",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rot_type =       ROT_FLAG_AZIMUTH,
    .port_type =      RIG_PORT_NETWORK,
    .timeout = 5000,
    .retry =   3,

    .min_az =     0.,
    .max_az =     360,
    .min_el =     0,
    .max_el =     90,

    .priv =  NULL,    /* priv */

    /* .rot_init     =  ether_rot_init, */
    /* .rot_cleanup  =  ether_rot_cleanup, */

    .rot_open     =  ether_rot_open,
    .rot_close    =  ether_rot_close,

    .set_position =  ether_rot_set_position,
    .get_position =  ether_rot_get_position,
    .park         =  ether_rot_park,
    .stop         =  ether_rot_stop,
    .reset        =  ether_rot_reset,
    .move         =  ether_rot_move,

    .get_info     =  ether_rot_get_info,
};

DECLARE_INITROT_BACKEND(ether6)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&ether6_rot_caps);

    return RIG_OK;
}
