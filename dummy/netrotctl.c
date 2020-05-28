/*
 *  Hamlib Netrotctl backend - main file
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <errno.h>

#include "hamlib/rotator.h"
#include "iofunc.h"
#include "misc.h"
#include "network.h"
#include "serial.h"

#include "rot_dummy.h"

#define CMD_MAX 32
#define BUF_MAX 64

/*
 * Helper function with protocol return code parsing
 */
static int netrotctl_transaction(ROT *rot, char *cmd, int len, char *buf)
{
    int ret;

    /* flush anything in the read buffer before command is sent */
    if (rot->state.rotport.type.rig == RIG_PORT_NETWORK
            || rot->state.rotport.type.rig == RIG_PORT_UDP_NETWORK)
    {
        network_flush(&rot->state.rotport);
    }
    else
    {
        serial_flush(&rot->state.rotport);
    }

    ret = write_block(&rot->state.rotport, cmd, len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret < 0)
    {
        return ret;
    }

    if (!memcmp(buf, NETROTCTL_RET, strlen(NETROTCTL_RET)))
    {
        return atoi(buf + strlen(NETROTCTL_RET));
    }

    return ret;
}

static int netrotctl_open(ROT *rot)
{
    int ret, len;
    struct rot_state *rs = &rot->state;
    int prot_ver;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    len = sprintf(cmd, "\\dump_state\n");

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    prot_ver = atoi(buf);
#define ROTCTLD_PROT_VER 0

    if (prot_ver < ROTCTLD_PROT_VER)
    {
        return -RIG_EPROTO;
    }

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    rs->min_az = atof(buf);

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    rs->max_az = atof(buf);

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    rs->min_el = atof(buf);

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    rs->max_el = atof(buf);

    return RIG_OK;
}

static int netrotctl_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* clean signoff, no read back */
    write_block(&rot->state.rotport, "q\n", 2);

    return RIG_OK;
}

static int netrotctl_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %f %f\n", __func__,
              az, el);

    len = sprintf(cmd, "P %f %f\n", az, el);

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrotctl_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "p\n");

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *az = atof(buf);

    ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *el = atof(buf);

    return RIG_OK;
}


static int netrotctl_stop(ROT *rot)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "S\n");

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrotctl_park(ROT *rot)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "K\n");

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrotctl_reset(ROT *rot, rot_reset_t reset)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "R %d\n", reset);

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrotctl_move(ROT *rot, int direction, int speed)
{
    int ret, len;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "M %d %d\n", direction, speed);

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static const char *netrotctl_get_info(ROT *rot)
{
    int ret, len;
    char cmd[CMD_MAX];
    static char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    len = sprintf(cmd, "_\n");

    ret = netrotctl_transaction(rot, cmd, len, buf);

    if (ret < 0)
    {
        return NULL;
    }

    buf [ret] = '\0';

    return buf;
}



/*
 * NET rotctl capabilities.
 */

const struct rot_caps netrotctl_caps =
{
    ROT_MODEL(ROT_MODEL_NETROTCTL),
    .model_name =     "NET rotctl",
    .mfg_name =       "Hamlib",
    .version =        "0.3",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_NETWORK,
    .timeout = 2000,
    .retry =   3,

    .min_az =     -180.,
    .max_az =     180.,
    .min_el =     0.,
    .max_el =     90.,

    .priv =  NULL,    /* priv */

    /* .rot_init =     netrotctl_init, */
    /* .rot_cleanup =  netrotctl_cleanup, */
    .rot_open =     netrotctl_open,
    .rot_close =    netrotctl_close,

    .set_position =     netrotctl_set_position,
    .get_position =     netrotctl_get_position,
    .park =     netrotctl_park,
    .stop =     netrotctl_stop,
    .reset =    netrotctl_reset,
    .move =     netrotctl_move,

    .get_info =      netrotctl_get_info,
};

