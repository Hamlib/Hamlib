/*
 i  Hamlib Dummy backend - main file
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

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <math.h>
#include <sys/time.h>
#include <errno.h>

#include "hamlib/rotator.h"
#include "dummy_common.h"
#include "rig.h"
#include "register.h"
#include "idx_builtin.h"
#include "misc.h"
#include "iofunc.h"
#include "rot_pstrotator.h"
#include "rotlist.h"
#include "network.h"

#define PSTROTATOR_ROT_FUNC 0
#define PSTROTATOR_ROT_LEVEL ROT_LEVEL_SPEED
#define PSTROTATOR_ROT_PARM 0

#define PSTROTATOR_ROT_STATUS (ROT_STATUS_MOVING | ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_LEFT | ROT_STATUS_MOVING_RIGHT | \
        ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_UP | ROT_STATUS_MOVING_DOWN | \
        ROT_STATUS_LIMIT_UP | ROT_STATUS_LIMIT_DOWN | ROT_STATUS_LIMIT_LEFT | ROT_STATUS_LIMIT_RIGHT)

static int simulating = 0;  // do we need rotator emulation for debug?

struct pstrotator_rot_priv_data
{
    azimuth_t az;
    elevation_t el;

    struct timeval tv;  /* time last az/el update */
    azimuth_t target_az;
    elevation_t target_el;
    rot_status_t status;

    setting_t funcs;
    value_t levels[RIG_SETTING_MAX];
    value_t parms[RIG_SETTING_MAX];

    struct ext_list *ext_funcs;
    struct ext_list *ext_levels;
    struct ext_list *ext_parms;

    char *magic_conf;

//    hamlib_port_t port2; // the reply port for PSTRotator which is port+1
    int sockfd2; // the reply port for PSTRotator which is port+1
};

static const struct confparams pstrotator_ext_levels[] =
{
    {
        TOK_EL_ROT_MAGICLEVEL, "MGL", "Magic level", "Magic level, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
    },
    {
        TOK_EL_ROT_MAGICFUNC, "MGF", "Magic func", "Magic function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    {
        TOK_EL_ROT_MAGICOP, "MGO", "Magic Op", "Magic Op, as an example",
        NULL, RIG_CONF_BUTTON
    },
    {
        TOK_EL_ROT_MAGICCOMBO, "MGC", "Magic combo", "Magic combo, as an example",
        "VALUE1", RIG_CONF_COMBO, { .c = { .combostr = { "VALUE1", "VALUE2", "NONE", NULL } } }
    },
    { RIG_CONF_END, NULL, }
};

static const struct confparams pstrotator_ext_funcs[] =
{
    {
        TOK_EL_ROT_MAGICEXTFUNC, "MGEF", "Magic ext func", "Magic ext function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    { RIG_CONF_END, NULL, }
};

static const struct confparams pstrotator_ext_parms[] =
{
    {
        TOK_EP_ROT_MAGICPARM, "MGP", "Magic parm", "Magic parameter, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
    },
    { RIG_CONF_END, NULL, }
};

/* cfgparams are configuration item generally used by the backend's open() method */
static const struct confparams pstrotator_cfg_params[] =
{
    {
        TOK_CFG_ROT_MAGICCONF, "mcfg", "Magic conf", "Magic parameter, as an example",
        "ROTATOR", RIG_CONF_STRING, { }
    },
    { RIG_CONF_END, NULL, }
};

static int write_transaction(ROT *rot, char *cmd)
{

    int try = rot->caps->retry;

    int retval = -RIG_EPROTO;

    hamlib_port_t *rp = ROTPORT(rot);

    // This shouldn't ever happen...but just in case
    // We need to avoid an empty write as rotctld replies with blank line
    if (strlen(cmd) == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: len==0??\n", __func__);
        return (retval);
    }

    // appears we can lose sync if we don't clear things out
    // shouldn't be anything for us now anyways
    rig_flush(rp);

    while (try-- >= 0 && retval != RIG_OK)
        {
            char cmd2[64];

            if (strchr(cmd, '\r') == NULL)
            {
                sprintf(cmd2, "%s\r", cmd);
            }

            retval = write_block(rp, (unsigned char *) cmd, strlen(cmd));

            if (retval  < 0)
            {
                return (-RIG_EIO);
            }
        }

    return RIG_OK;
}

static int pstrotator_rot_init(ROT *rot)
{
    struct pstrotator_rot_priv_data *priv;
    struct rot_state *rs = ROTSTATE(rot);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rs->priv = (struct pstrotator_rot_priv_data *)
                      calloc(1, sizeof(struct pstrotator_rot_priv_data));

    if (!rs->priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rs->priv;

    priv->ext_funcs = alloc_init_ext(pstrotator_ext_funcs);

    if (!priv->ext_funcs)
    {
        return -RIG_ENOMEM;
    }

    priv->ext_levels = alloc_init_ext(pstrotator_ext_levels);

    if (!priv->ext_levels)
    {
        return -RIG_ENOMEM;
    }

    priv->ext_parms = alloc_init_ext(pstrotator_ext_parms);

    if (!priv->ext_parms)
    {
        return -RIG_ENOMEM;
    }

    ROTPORT(rot)->type.rig = RIG_PORT_UDP_NETWORK;

    priv->az = priv->el = 0;

    priv->target_az = priv->target_el = 0;

    priv->magic_conf = strdup("ROTATOR");

    strcpy(ROTPORT(rot)->pathname, "192.0.0.1:12000");

    return RIG_OK;
}

static int pstrotator_rot_cleanup(ROT *rot)
{
    struct rot_state *rs = ROTSTATE(rot);
    struct pstrotator_rot_priv_data *priv = (struct pstrotator_rot_priv_data *)
                                            rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    free(priv->ext_funcs);
    free(priv->ext_levels);
    free(priv->ext_parms);
    free(priv->magic_conf);
    free(rs->priv);

    rs->priv = NULL;

    return RIG_OK;
}

static void set_timeout(int fd, int sec, int usec)
{
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: sec=%d, usec=%d, timeout = %.6lf\n", __func__,
              sec, usec, sec + usec / 1e6);

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
                   sizeof(timeout)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt failed: %s\n", __func__,
                  strerror(errno));
    }
}

static int pstrotator_rot_open(ROT *rot)
{
    struct pstrotator_rot_priv_data *priv;
    int port = 0;
    int n1, n2, n3, n4;
    int sockfd;
    struct sockaddr_in clientAddr;
    struct rot_state *rs = ROTSTATE(rot);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct pstrotator_rot_priv_data *)rs->priv;
    //priv->port2 = rs->rotport;
    //priv->port2.type.rig = RIG_PORT_UDP_NETWORK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: pathname=%s\n", __func__,
              ROTPORT(rot)->pathname);
    sscanf(ROTPORT(rot)->pathname, "%d.%d.%d.%d:%d", &n1, &n2, &n3, &n4, &port);
    //sprintf(priv->port2.pathname, "%d.%d.%d.%d:%d", n1, n2, n3, n4, port+1);
    //rig_debug(RIG_DEBUG_VERBOSE, "%s: port2 pathname=%s\n", __func__, priv->port2.pathname);

    //network_open(&priv->port2, port+1);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: socket failed: %s\n", __func__, strerror(errno));
        return -RIG_EINTERNAL;
    }

    // Bind socket to client address
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(12001);

    if (bind(sockfd, (const struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bind failed: %s\n", __func__, strerror(errno));
        return -RIG_EINTERNAL;
    }

    priv->sockfd2 = sockfd;
    set_timeout(priv->sockfd2, 1, 0);

    return RIG_OK;
}

static int pstrotator_rot_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int pstrotator_set_conf(ROT *rot, hamlib_token_t token, const char *val)
{
    struct pstrotator_rot_priv_data *priv;

    priv = (struct pstrotator_rot_priv_data *)ROTSTATE(rot)->priv;

    switch (token)
    {
    case TOK_CFG_ROT_MAGICCONF:
        if (val)
        {
            free(priv->magic_conf);
            priv->magic_conf = strdup(val);
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


static int pstrotator_get_conf2(ROT *rot, hamlib_token_t token, char *val,
                                int val_len)
{
    struct pstrotator_rot_priv_data *priv;

    priv = (struct pstrotator_rot_priv_data *)ROTSTATE(rot)->priv;

    switch (token)
    {
    case TOK_CFG_ROT_MAGICCONF:
        SNPRINTF(val, val_len, "%s", priv->magic_conf);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int pstrotator_get_conf(ROT *rot, hamlib_token_t token, char *val)
{
    return pstrotator_get_conf2(rot, token, val, 128);
}



static int pstrotator_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    struct pstrotator_rot_priv_data *priv = (struct pstrotator_rot_priv_data *)
                                            ROTSTATE(rot)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    if (simulating)
    {
        priv->target_az = az;
        priv->target_el = el;
        gettimeofday(&priv->tv, NULL);
    }
    else
    {
        char cmd[64];
        sprintf(cmd, "<PST><AZIMUTH>%f.2</AZIMUTH></PST>", az);
        write_transaction(rot, cmd);
        sprintf(cmd, "<PST><ELEVATION>%f.2</ELEVATION></PST>", el);
        write_transaction(rot, cmd);
        priv->az = az;
        priv->el = el;
    }


    return RIG_OK;
}

void readPacket(int sockfd, char *buf, int buf_len, int expected)
{
    struct sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(serverAddr);

    buf[0] = 0;

    if (expected)
    {
        set_timeout(sockfd, 1, 0);
    }
    else
    {
        set_timeout(sockfd, 0, 0);
    }

    ssize_t n = recvfrom(sockfd, buf, buf_len, 0, (struct sockaddr *)&serverAddr,
                         &addrLen);

    if (n < 0)
    {
#ifdef _WIN32
        int err = WSAGetLastError();

        if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT)
        {
            if (expected)
                rig_debug(RIG_DEBUG_ERR,
                          "%s: recvfrom timed out. Is PSTRotator Setup/UDP Control enabled?\n", __func__);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: recvfrom error %d: %s\n", __func__, err,
                      strerror(errno));
        }

#else

        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            if (expected)
                rig_debug(RIG_DEBUG_ERR,
                          "%s: recvfrom timed out. Is PSTRotator Setup/UDP Control checked?\n", __func__);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: recvfrom error: %s\n", __func__, strerror(errno));
        }

#endif
        n = 0;
    }

    buf[n] = '\0'; // Null-terminate the received data
    strtok(buf, "\r\n"); // get rid of CRs and such

    if (n > 0) { rig_debug(RIG_DEBUG_VERBOSE, "%s: buf=%s\n", __func__, buf); }
}

/*
 * Get position of rotor, simulating slow rotation
 */
static int pstrotator_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    struct pstrotator_rot_priv_data *priv = (struct pstrotator_rot_priv_data *)
                                            ROTSTATE(rot)->priv;
    char buf[64];
    int n = 0;
    fd_set rfds, efds;
    int select_result;
    struct timeval timeout;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    write_transaction(rot, "<PST>AZ?</PST>");
    write_transaction(rot, "<PST>EL?</PST>");

    do
    {
        //read_string(&priv->port2, (unsigned char*)buf, sizeof(buf), stopset, stopset_len, 1, 1);
        buf[0] = 0;

        // if moving we need to keep polling for updates until there are none
        if (n == 2)
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;

            FD_ZERO(&rfds);
            FD_SET(priv->sockfd2, &rfds);
            efds = rfds;
            select_result = select(priv->sockfd2, &rfds, NULL, &efds, &timeout);

            if (select_result == 0)
            {
                //rig_debug(RIG_DEBUG_VERBOSE, "%s: timeout\n", __func__);
                break;
            }
            else
            {
                rig_debug(RIG_DEBUG_VERBOSE, "%s: select_result=%d\n", __func__, select_result);
                readPacket(priv->sockfd2, buf, sizeof(buf), 0);
            }
        }
        else
        {
            readPacket(priv->sockfd2, buf, sizeof(buf), 1);
        }

        dump_hex((unsigned char *)buf, strlen(buf));
        n += sscanf(buf, "AZ:%g", &priv->az);
        n += sscanf(buf, "EL:%g", &priv->el);
        if (n > 2) n = 2;
    }
    while (strlen(buf) > 0);

    *az = priv->az;
    *el = priv->el;

    return RIG_OK;
}


static int pstrotator_rot_park(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    write_transaction(rot, "<PST><PARK>1</PARK></PST>");

    return RIG_OK;
}

static const char *pstrotator_rot_get_info(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "PSTRotator";
}

static int pstrotator_rot_get_status(ROT *rot, rot_status_t *status)
{
    const struct pstrotator_rot_priv_data *priv = (struct pstrotator_rot_priv_data
            *)
            ROTSTATE(rot)->priv;

    *status = priv->status;

    return RIG_OK;
}


/*
 * Dummy rotator capabilities.
 */
struct rot_caps pstrotator_caps =
{
    ROT_MODEL(ROT_MODEL_PSTROTATOR),
    .model_name =     "PstRotator",
    .mfg_name =       "YO3DMU",
    .version =        "20240607.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_UDP_NETWORK,
    .timeout =        1000,

    .min_az =     -180.,
    .max_az =     450.,
    .min_el =     0.,
    .max_el =     90.,

    .priv =  NULL,    /* priv */

    .has_get_func =   PSTROTATOR_ROT_FUNC,
    .has_set_func =   PSTROTATOR_ROT_FUNC,
    .has_get_level =  PSTROTATOR_ROT_LEVEL,
    .has_set_level =  ROT_LEVEL_SET(PSTROTATOR_ROT_LEVEL),
    .has_get_parm =    PSTROTATOR_ROT_PARM,
    .has_set_parm =    ROT_PARM_SET(PSTROTATOR_ROT_PARM),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .extlevels =    pstrotator_ext_levels,
    .extfuncs =     pstrotator_ext_funcs,
    .extparms =     pstrotator_ext_parms,
    .cfgparams =    pstrotator_cfg_params,

    .has_status = PSTROTATOR_ROT_STATUS,

    .rot_init =     pstrotator_rot_init,
    .rot_cleanup =  pstrotator_rot_cleanup,
    .rot_open =     pstrotator_rot_open,
    .rot_close =    pstrotator_rot_close,

    .set_conf =     pstrotator_set_conf,
    .get_conf =     pstrotator_get_conf,

    .set_position =     pstrotator_rot_set_position,
    .get_position =     pstrotator_rot_get_position,
    .park =     pstrotator_rot_park,

    .get_info = pstrotator_rot_get_info,
    .get_status = pstrotator_rot_get_status,
};
