/*
 * Hamlib Rotator backend - SPID Rot2Prog
 * Copyright (c) 2009 by Norvald H. Ryeng, LA6YKA
 *
 * $Id$
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "spid.h"

#define TOK_AZRES 1
#define TOK_ELRES 2

struct spid_priv_data {
    int az_resolution;
    int el_resolution;
};

static int spid_rot_init(ROT *rot)
{
    struct spid_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    if (!rot || !rot->caps)
        return -RIG_EINVAL;

    priv = (struct spid_priv_data*)malloc(sizeof(struct spid_priv_data));
    if (!priv) {
        return -RIG_ENOMEM;
    }

    rot->state.priv = (void*)priv;

    priv->az_resolution = 0;
    priv->el_resolution = 0;

    return RIG_OK;
}

static int spid_rot_cleanup(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    if (!rot)
        return -RIG_EINVAL;

    if (rot->state.priv)
        free(rot->state.priv);
    rot->state.priv = NULL;

    return RIG_OK;
}

static int spid_get_conf(ROT *rot, token_t token, char *val)
{
    struct spid_priv_data *priv = (struct spid_priv_data*)rot->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s called %d\n", __FUNCTION__, token);

    switch(token) {
        case TOK_AZRES:
            sprintf(val, "%d", priv->az_resolution);
            break;
        case TOK_ELRES:
            sprintf(val, "%d", priv->el_resolution);
            break;
        default:
            return -RIG_EINVAL;
    }
    return RIG_OK;
}

static int spid_set_conf(ROT *rot, token_t token, const char *val)
{
    struct spid_priv_data *priv = (struct spid_priv_data*)rot->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s called %d %s\n", __FUNCTION__, token, val);

    switch(token) {
        case TOK_AZRES:
            priv->az_resolution = atoi(val);
            break;
        case TOK_ELRES:
            priv->el_resolution = atoi(val);
            break;
        default:
            return -RIG_EINVAL;
    }
    return RIG_OK;
}

static int spid_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    struct rot_state *rs;
    rs = &rot->state;
    struct spid_priv_data *priv = (struct spid_priv_data*)rs->priv;
    int retval;
    int retry_read = 0;
    char cmdstr[13];
    unsigned int u_az, u_el;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __FUNCTION__, az, el);

    if (!priv->az_resolution || !priv->el_resolution) {
        do {
            retval = write_block(&rs->rotport, "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1F\x20", 13);
            if (retval != RIG_OK) {
                return retval;
            }

            memset(cmdstr, 0, 12);
            retval = read_block(&rs->rotport, cmdstr, 12);
        } while (retval < 0 && retry_read++ < rot->state.rotport.retry);
        if (retval < 0)
            return retval;
    } else {
        cmdstr[5] = priv->az_resolution;    /* PH  */
        cmdstr[10] = priv->el_resolution;   /* PV  */
    }

    u_az = cmdstr[5]*(360 + az);
    u_el = cmdstr[10]*(360 + el);

    cmdstr[0] = 0x57;                       /* S   */
    cmdstr[1] = 0x30 + u_az/1000;           /* H1  */
    cmdstr[2] = 0x30 + (u_az % 1000) / 100; /* H2  */
    cmdstr[3] = 0x30 + (u_az % 100) / 10;   /* H3  */
    cmdstr[4] = 0x30 + (u_az % 10);         /* H4  */

    cmdstr[6] = 0x30 + u_el / 1000;         /* V1  */
    cmdstr[7] = 0x30 + (u_el % 1000) / 100; /* V2  */
    cmdstr[8] = 0x30 + (u_el % 100) / 10;   /* V3  */
    cmdstr[9] = 0x30 + (u_el % 10);         /* V4  */

    cmdstr[11] = 0x2F;                      /* K   */
    cmdstr[12] = 0x20;                      /* END */

    retval = write_block(&rs->rotport, cmdstr, 13);
    if (retval != RIG_OK) {
        return retval;
    }

    return RIG_OK;
}

static int spid_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    struct rot_state *rs;
    rs = &rot->state;
    int retval;
    int retry_read = 0;
    char posbuf[12];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    do {
        retval = write_block(&rs->rotport, "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1F\x20", 13);
        if (retval != RIG_OK) {
            return retval;
        }

        memset(posbuf, 0, 12);
        retval = read_block(&rs->rotport, posbuf, 12);
    } while (retval < 0 && retry_read++ < rot->state.rotport.retry);
    if (retval < 0)
        return retval;

    *az  = posbuf[1] * 100;
    *az += posbuf[2] * 10;
    *az += posbuf[3];
    *az += posbuf[4] / 10.0;
    *az -= 360;

    *el  = posbuf[6] * 100;
    *el += posbuf[7] * 10;
    *el += posbuf[8];
    *el += posbuf[9] / 10.0;
    *el -= 360;

    rig_debug(RIG_DEBUG_TRACE, "%s: (az, el) = (%.1f, %.1f)\n",
		   __FUNCTION__, *az, *el);

    return RIG_OK;
}

static int spid_rot_stop(ROT *rot)
{
    struct rot_state *rs;
    int retval;
    rs = &rot->state;
    int retry_read = 0;
    char posbuf[12];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    do {
        retval = write_block(&rs->rotport, "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0F\x20", 13);
        if (retval != RIG_OK) {
            return retval;
        }

        memset(posbuf, 0, 12);
        retval = read_block(&rs->rotport, posbuf, 12);
    } while (retval < 0 && retry_read++ < rot->state.rotport.retry);
    if (retval < 0)
        return retval;

    return RIG_OK;
}

const struct confparams spid_cfg_params[] = {
    { TOK_AZRES, "az_resolution", "Azimuth resolution", "Number of pulses per degree, 0 = auto sense",
      "0", RIG_CONF_NUMERIC, { .n = { 0, 0xff, 1 } }
    },
    { TOK_ELRES, "el_resolution", "Eleveation resolution", "Number of pulses per degree, 0 = auto sense",
      "0", RIG_CONF_NUMERIC, { .n = { 0, 0xff, 1 } }
    },
    { RIG_CONF_END, NULL, }
};

const struct rot_caps spid_rot_caps = {
    .rot_model =         ROT_MODEL_SPID,
    .model_name =        "Rot2Prog",
    .mfg_name =          "SPID",
    .version =           "0.1",
    .copyright =         "LGPL",
    .status =            RIG_STATUS_UNTESTED,
    .rot_type =          ROT_TYPE_OTHER,
    .port_type =         RIG_PORT_SERIAL,
    .serial_rate_min =   600,
    .serial_rate_max =   1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =       0,
    .post_write_delay =  0,
    .timeout =           400,
    .retry =             3,

    .min_az =            -180.0,
    .max_az =            540.0,
    .min_el =            -20.0,
    .max_el =            210.0,

    .cfgparams =         spid_cfg_params,
    .get_conf =          spid_get_conf,
    .set_conf =          spid_set_conf,

    .rot_init =          spid_rot_init,
    .rot_cleanup =       spid_rot_cleanup,
    .get_position =      spid_rot_get_position,
    .set_position =      spid_rot_set_position,
    .stop =              spid_rot_stop,
};

DECLARE_INITROT_BACKEND(spid)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

    rot_register(&spid_rot_caps);

    return RIG_OK;
}
