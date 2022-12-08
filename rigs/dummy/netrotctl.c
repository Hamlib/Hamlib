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

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>  /* String function definitions */
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
    rig_flush(&rot->state.rotport);

    ret = write_block(&rot->state.rotport, (unsigned char *) cmd, len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_string(&rot->state.rotport, (unsigned char *) buf, BUF_MAX, "\n",
                      sizeof("\n"), 0, 1);

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
    int ret;
    struct rot_state *rs = &rot->state;
    int prot_ver;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    SNPRINTF(cmd, sizeof(cmd), "\\dump_state\n");

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    prot_ver = atoi(buf);
#define ROTCTLD_PROT_VER 1

    ret = read_string(&rot->state.rotport, (unsigned char *) buf, BUF_MAX, "\n",
                      sizeof("\n"), 0, 1);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (prot_ver == 0) { return (RIG_OK); }

    // Prot 1 is tag=value format and should cover any needed additions
    do
    {
        char setting[32], value[1024];

        ret = read_string(&rot->state.rotport, (unsigned char *) buf, BUF_MAX, "\n",
                          sizeof("\n"), 0, 1);

        if (ret <= 0)
        {
            return (ret < 0) ? ret : -RIG_EPROTO;
        }

        // ignore the rot_model

        if (strncmp(buf, "done", 4) == 0) { return (RIG_OK); }

        if (sscanf(buf, "%31[^=]=%1023[^\t\n]", setting, value) == 2)
        {
            if (strcmp(setting, "min_az") == 0)
            {
                rs->min_az = rot->caps->min_az = atof(value);
            }
            else if (strcmp(setting, "max_az") == 0)
            {
                rs->max_az = rot->caps->max_az = atof(value);
            }
            else if (strcmp(setting, "min_el") == 0)
            {
                rs->min_el = rot->caps->min_el = atof(value);
            }
            else if (strcmp(setting, "max_el") == 0)
            {
                rs->max_el = rot->caps->max_el = atof(value);
            }
            else if (strcmp(setting, "south_zero") == 0)
            {
                rs->south_zero = atoi(value);
            }
            else if (strcmp(setting, "rot_type") == 0)
            {
                if (strcmp(value, "AzEl") == 0) { rot->caps->rot_type = ROT_TYPE_AZEL; }
                else if (strcmp(value, "Az") == 0) { rot->caps->rot_type = ROT_TYPE_AZIMUTH; }
                else if (strcmp(value, "El") == 0) { rot->caps->rot_type = ROT_TYPE_ELEVATION; }
            }
            else
            {
                // not an error -- just a warning for backward compatibility
                rig_debug(RIG_DEBUG_ERR, "%s: unknown setting='%s'\n", __func__, buf);
            }
        }
    }
    while (1);


    return RIG_OK;
}

static int netrotctl_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* clean signoff, no read back */
    write_block(&rot->state.rotport, (unsigned char *) "q\n", 2);

    return RIG_OK;
}

static int netrotctl_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %f %f\n", __func__,
              az, el);

    SNPRINTF(cmd, sizeof(cmd), "P %f %f\n", az, el);

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

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
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "p\n");

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *az = atof(buf);

    ret = read_string(&rot->state.rotport, (unsigned char *) buf, BUF_MAX, "\n",
                      sizeof("\n"), 0, 1);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *el = atof(buf);

    return RIG_OK;
}


static int netrotctl_stop(ROT *rot)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "S\n");

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

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
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "K\n");

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

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
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "R %d\n", reset);

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

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
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "M %d %d\n", direction, speed);

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

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
    int ret;
    char cmd[CMD_MAX];
    static char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "_\n");

    ret = netrotctl_transaction(rot, cmd, strlen(cmd), buf);

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

struct rot_caps netrotctl_caps =
{
    ROT_MODEL(ROT_MODEL_NETROTCTL),
    .model_name =     "NET rotctl",
    .mfg_name =       "Hamlib",
    .version =        "20221110.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
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

