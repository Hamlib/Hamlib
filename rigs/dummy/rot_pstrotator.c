/*
 i  Hamlib PSTRotator backend
 *  Copyright (c) 2024 Michael Black W9MDB
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

#include "hamlib/config.h"
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

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
        ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_UP | ROT_STATUS_MOVING_DOWN )

struct pstrotator_rot_priv_data
{
    azimuth_t az;
    elevation_t el;

    struct timeval tv;  /* time last az/el update */
    azimuth_t target_az;
    elevation_t target_el;
    rot_status_t status;

    int sockfd2; // the reply port for PSTRotator which is port+1

    pthread_t threadid;

    int receiving; // true if we are receiving az/el data
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

static void set_timeout(int fd, int sec, int usec)
{
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    //rig_debug(RIG_DEBUG_VERBOSE, "%s: sec=%d, usec=%d, timeout = %.6lf\n", __func__,
    //          sec, usec, sec + usec / 1e6);

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
                   sizeof(timeout)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt failed: %s\n", __func__,
                  strerror(errno));
    }
}

static void readPacket(int sockfd, char *buf, int buf_len, int expected)
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
#if 0

            if (expected)
                rig_debug(RIG_DEBUG_ERR,
                          "%s: recvfrom timed out. Is PSTRotator Setup/UDP Control enabled?\n", __func__);

#endif
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

    //if (n > 0) { rig_debug(RIG_DEBUG_VERBOSE, "%s: buf=%s\n", __func__, buf); }
}

#if 0
typedef struct pstrotator_handler_args_sw
{
    int port; // port for reading PstRotator messages -- always +1 from base port
} pstrotator_handler_args;

typedef struct pstrotator_handler_priv_data_s
{
    pthread_t thread_id;
    pstrotator_handler_args args;
} pstrotator_handler_priv_data;
#endif

static void *pstrotator_handler_start(void *arg)
{
    ROT *rot = (ROT *)arg;
    struct rot_state *rs = STATE(rot);
    struct pstrotator_rot_priv_data *priv = rs->priv;
    pstrotator_handler_priv_data *pstrotator_handler_priv;

    rs->pstrotator_handler_priv_data = calloc(1,
                                       sizeof(pstrotator_handler_priv_data));

    if (rs->pstrotator_handler_priv_data == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: priv is NULL?\n", __func__);
        return NULL;
    }

    pstrotator_handler_priv = (pstrotator_handler_priv_data *)
                              rs->pstrotator_handler_priv_data;
    pstrotator_handler_priv->args.rot = rot;
    pstrotator_handler_priv->pstrotator_handler_thread_run = 1;
    priv->receiving = 0;

    while (pstrotator_handler_priv->pstrotator_handler_thread_run)
    {
        int az = 0, el = 0;
        char buf[256];
        readPacket(priv->sockfd2, buf, sizeof(buf), 1);

        if (strlen(buf) == 0)
        {
            hl_usleep(20 * 1000);
            continue;
        }

        //dump_hex((unsigned char *)buf, strlen(buf));
        int n = sscanf(buf, "AZ:%g", &priv->az);
        n += sscanf(buf, "EL:%g", &priv->el);

        if (n > 0) { priv->receiving = 1; }

        if (priv->az != az && priv->el != el) { priv->status = ROT_STATUS_MOVING; }
        else if (priv->az <  az) { priv->status = ROT_STATUS_MOVING_LEFT; }
        else if (priv->az >  az) { priv->status = ROT_STATUS_MOVING_RIGHT; }
        else if (priv->el <  el) { priv->status = ROT_STATUS_MOVING_DOWN; }
        else if (priv->el >  el) { priv->status = ROT_STATUS_MOVING_UP; }
        else { priv->status = ROT_STATUS_NONE; }

        //if (n > 0) rig_debug(RIG_DEBUG_CACHE, "%s: az=%.1f, el=%.1f\n", __func__, priv->az, priv->el);
    }

    return NULL;
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

    ROTPORT(rot)->type.rig = RIG_PORT_UDP_NETWORK;

    priv->az = priv->el = 0;

    priv->target_az = priv->target_el = 0;
    priv->sockfd2 = -1;

    strcpy(ROTPORT(rot)->pathname, "192.168.56.1:12000");

    return RIG_OK;
}

static int pstrotator_rot_cleanup(ROT *rot)
{
    struct rot_state *rs = ROTSTATE(rot);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    free(rs->priv);

    rs->priv = NULL;

    return RIG_OK;
}

static int pstrotator_rot_open(ROT *rot)
{
    struct pstrotator_rot_priv_data *priv;
    int port = 0;
    int n1, n2, n3, n4;
    int sockfd;
    int retval;
    struct sockaddr_in clientAddr;
    struct rot_state *rs = ROTSTATE(rot);
    pthread_attr_t attr;

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
        close(sockfd);
        return -RIG_EINTERNAL;
    }

    priv->sockfd2 = sockfd;
    set_timeout(priv->sockfd2, 1, 0);

    pthread_attr_init(&attr);
    retval = pthread_create(&priv->threadid, &attr, pstrotator_handler_start, rot);

    if (retval != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s; pthread_create error: %s\n", __func__,
                  strerror(errno));
        return -RIG_EINTERNAL;
    }

    return RIG_OK;
}

static int pstrotator_rot_close(ROT *rot)
{
    struct pstrotator_rot_priv_data *priv;
    priv = (struct pstrotator_rot_priv_data *)ROTSTATE(rot)->priv;
    pstrotator_handler_priv_data *pstrotator_handler_priv;
    pstrotator_handler_priv = (pstrotator_handler_priv_data *)
                              ROTSTATE(rot)->pstrotator_handler_priv_data;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    pstrotator_handler_priv->pstrotator_handler_thread_run = 0;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: waiting for thread to stop\n", __func__);
    pthread_join(priv->threadid, NULL);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: thread stopped\n", __func__);
    priv->threadid = 0;

    if (priv->sockfd2 != -1)
    {
        close(priv->sockfd2);
        priv->sockfd2 = -1;
    }
    return RIG_OK;
}

#if 0
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
#endif


#if 0
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
#endif

#if 0
static int pstrotator_get_conf(ROT *rot, hamlib_token_t token, char *val)
{
    return pstrotator_get_conf2(rot, token, val, 128);
}
#endif



static int pstrotator_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    struct pstrotator_rot_priv_data *priv = (struct pstrotator_rot_priv_data *)
                                            ROTSTATE(rot)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    char cmd[64];
    sprintf(cmd, "<PST><AZIMUTH>%f.2</AZIMUTH></PST>", az);
    write_transaction(rot, cmd);
    sprintf(cmd, "<PST><ELEVATION>%f.2</ELEVATION></PST>", el);
    write_transaction(rot, cmd);
    priv->az = az;
    priv->el = el;

    return RIG_OK;
}


/*
 * Get position of rotor, simulating slow rotation
 */
static int pstrotator_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    struct pstrotator_rot_priv_data *priv = (struct pstrotator_rot_priv_data *)
                                            ROTSTATE(rot)->priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    write_transaction(rot, "<PST>AZ?</PST>");
    write_transaction(rot, "<PST>EL?</PST>");

    hl_usleep(10 * 1000);
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
    .version =        "20240613.0",
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

    //.level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .has_status = PSTROTATOR_ROT_STATUS,

    .rot_init =     pstrotator_rot_init,
    .rot_cleanup =  pstrotator_rot_cleanup,
    .rot_open =     pstrotator_rot_open,
    .rot_close =    pstrotator_rot_close,

    .set_position =     pstrotator_rot_set_position,
    .get_position =     pstrotator_rot_get_position,
    .park =     pstrotator_rot_park,

    .get_info = pstrotator_rot_get_info,
    .get_status = pstrotator_rot_get_status,
};
