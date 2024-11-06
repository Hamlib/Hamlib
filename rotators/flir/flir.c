/*
 *  Hamlib FLIR PTU rotor backend - main file
 *  Copyright (c) 2022 by Andreas Mueller (DC1MIL)
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <math.h>
#include <sys/time.h>

#include "hamlib/rotator.h"
#include "register.h"
#include "idx_builtin.h"
#include "serial.h"

#include "flir.h"

#define FLIR_FUNC 0
#define FLIR_LEVEL ROT_LEVEL_SPEED
#define FLIR_PARM 0

#define FLIR_STATUS (ROT_STATUS_MOVING | ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_LEFT | ROT_STATUS_MOVING_RIGHT | \
        ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_UP | ROT_STATUS_MOVING_DOWN | \
        ROT_STATUS_LIMIT_UP | ROT_STATUS_LIMIT_DOWN | ROT_STATUS_LIMIT_LEFT | ROT_STATUS_LIMIT_RIGHT)

struct flir_priv_data
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

    char info[256];

    struct ext_list *ext_funcs;
    struct ext_list *ext_levels;
    struct ext_list *ext_parms;

    char *magic_conf;

    double resolution_pp;
    double resolution_tp;
};

static int flir_request(ROT *rot, char *request, char *response,
                        int resp_size)
{
    int return_value = -RIG_EINVAL;
    hamlib_port_t *rotp = ROTPORT(rot);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_flush(rotp);

    if (request)
    {
        return_value = write_block(rotp, (unsigned char *)request,
                                   strlen(request));

        if (return_value != RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s request not OK\n", __func__);
            return return_value;
        }
    }

    //Is a direct response expected?
    if (response != NULL)
    {
        int retry_read = 0;
        int read_char;

        while (retry_read < rotp->retry)
        {
            memset(response, 0, (size_t)resp_size);
            read_char = read_string(rotp, (unsigned char *)response,
                                    resp_size,
                                    "\r\n", sizeof("\r\n"), 0, 1);

            if (read_char > 0)
            {
                if (response[0] == '*')
                {
                    rig_debug(RIG_DEBUG_VERBOSE, "accepted command %s\n", request);
                    return RIG_OK;
                }
                else
                {
                    rig_debug(RIG_DEBUG_VERBOSE, "NOT accepted command %s\n", request);
                    return -RIG_ERJCTED;
                }

            }

            retry_read++;
        }

        response[0] = 0;
        rig_debug(RIG_DEBUG_VERBOSE, "timeout for command %s\n", request);
        return -RIG_ETIMEOUT;
    }

    return return_value;
};

static int flir_init(ROT *rot)
{
    struct flir_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ROTSTATE(rot)->priv = (struct flir_priv_data *)
                          calloc(1, sizeof(struct flir_priv_data));

    if (!ROTSTATE(rot)->priv)
    {
        return -RIG_ENOMEM;
    }

    priv = ROTSTATE(rot)->priv;

    priv->az = priv->el = 0;

    priv->target_az = priv->target_el = 0;

    priv->magic_conf = strdup("ROTATOR");

    priv->resolution_pp = 92.5714;
    priv->resolution_tp = 46.2857;

    return RIG_OK;
}

static int flir_cleanup(ROT *rot)
{
    struct flir_priv_data *priv = (struct flir_priv_data *)
                                  ROTSTATE(rot)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    free(priv->ext_funcs);
    free(priv->ext_levels);
    free(priv->ext_parms);
    free(priv->magic_conf);
    free(ROTSTATE(rot)->priv);

    ROTSTATE(rot)->priv = NULL;

    return RIG_OK;
}

static int flir_open(ROT *rot)
{
    struct flir_priv_data *priv;
    char return_str[MAXBUF];
    double resolution_pp, resolution_tp;
    int return_value = RIG_OK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = ROTSTATE(rot)->priv;

    // Disable ECHO
    return_value = flir_request(rot, "ED\n", NULL, MAXBUF);

    if (return_value != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ED: %s\n", __func__, rigerror(return_value));
        return return_value;
    }

    // Disable Verbose Mode
    return_value = flir_request(rot, "FT\n", return_str, MAXBUF);

    if (return_value != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: FT: %s\n", __func__, rigerror(return_value));
        return return_value;
    }

    // Get PAN resolution in arcsecs
    if (flir_request(rot, "PR\n", return_str, MAXBUF) == RIG_OK)
    {
        sscanf(return_str, "* %lf", &resolution_pp);
        rig_debug(RIG_DEBUG_VERBOSE, "PAN resolution: %lf arcsecs per position\n",
                  resolution_pp);
        priv->resolution_pp = resolution_pp;
    }
    else
    {
        return_value = -RIG_EPROTO;
    }

    // Get TILT resolution in arcsecs
    if (flir_request(rot, "TR\n", return_str, MAXBUF) == RIG_OK)
    {
        sscanf(return_str, "* %lf", &resolution_tp);
        rig_debug(RIG_DEBUG_VERBOSE, "TILT resolution: %lf arcsecs per position\n",
                  resolution_tp);
        priv->resolution_tp = resolution_tp;
    }
    else
    {
        return_value = -RIG_EPROTO;
    }

    return return_value;
}

static int flir_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int flir_set_conf(ROT *rot, hamlib_token_t token, const char *val)
{
    return -RIG_ENIMPL;
}

static int flir_get_conf(ROT *rot, hamlib_token_t token, char *val)
{
    return -RIG_ENIMPL;
}

static int flir_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    int32_t t_pan_positions, t_tilt_positions;
    char return_str[MAXBUF];
    char cmd_str[MAXBUF];
    struct flir_priv_data *priv = (struct flir_priv_data *)
                                  ROTSTATE(rot)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    priv->target_az = az;
    priv->target_el = el;

    t_pan_positions = (az * 3600) / priv->resolution_pp;
    t_tilt_positions = - ((90.0 - el) * 3600) / priv->resolution_tp;

    sprintf(cmd_str, "PP%d TP%d\n", t_pan_positions, t_tilt_positions);

    return flir_request(rot, cmd_str, return_str, MAXBUF);
}

/*
 * Get position of rotor
 */
static int flir_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    int return_value = RIG_OK;
    char return_str[MAXBUF];
    int32_t pan_positions, tilt_positions;

    struct flir_priv_data *priv = (struct flir_priv_data *)
                                  ROTSTATE(rot)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    if (flir_request(rot, "PP\n", return_str, MAXBUF) == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "PP Return String: %s\n", return_str);
        sscanf(return_str, "* %d", &pan_positions);
        priv->az = (pan_positions * priv->resolution_pp) / 3600;
        *az = priv->az;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "PP Wrong Return String: %s\n", return_str);
        return_value = -RIG_EPROTO;
    }

    if (flir_request(rot, "TP\n", return_str, MAXBUF) == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "TP Return String: %s\n", return_str);
        sscanf(return_str, "* %d", &tilt_positions);
        priv->el = 90.0 + ((tilt_positions * priv->resolution_tp) / 3600);
        *el = priv->el;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "PP Wrong Return String: %s\n", return_str);
        return_value = -RIG_EPROTO;
    }

    return return_value;
}

static int flir_stop(ROT *rot)
{
    int return_value = RIG_OK;

    struct flir_priv_data *priv = (struct flir_priv_data *)
                                  ROTSTATE(rot)->priv;
    azimuth_t az;
    elevation_t el;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return_value = flir_request(rot, "H\n", NULL, MAXBUF);

    if (return_value != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: H: %s\n", __func__, rigerror(return_value));
        return return_value;
    }

    // Wait 2s until rotor has stopped (Needs to be refactored)
    hl_usleep(2000000);

    return_value = flir_get_position(rot, &az, &el);

    if (return_value != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: flrig_get_position: %s\n", __func__,
                  rigerror(return_value));
        return return_value;
    }

    priv->target_az = priv->az = az;
    priv->target_el = priv->el = el;

    return return_value;
}

static int flir_park(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Assume park Position is 0,90 */
    flir_set_position(rot, 0, 90);

    return RIG_OK;
}

static int flir_reset(ROT *rot, rot_reset_t reset)
{
    int return_value = RIG_OK;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (reset != 0)
    {
        return_value = flir_request(rot, "r\n", NULL, 0);

        // After Reset: Disable Hard Limits
        if (return_value == RIG_OK)
        {
            return_value = flir_request(rot, "LD\n", NULL, MAXBUF);
        }
    }

    return return_value;
}

static int flir_move(ROT *rot, int direction, int speed)
{
    struct flir_priv_data *priv = (struct flir_priv_data *)
                                  ROTSTATE(rot)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: Direction = %d, Speed = %d\n", __func__,
              direction, speed);

    switch (direction)
    {
    case ROT_MOVE_UP:
        return flir_set_position(rot, priv->target_az, 90);

    case ROT_MOVE_DOWN:
        return flir_set_position(rot, priv->target_az, 0);

    case ROT_MOVE_CCW:
        return flir_set_position(rot, -180, priv->target_el);

    case ROT_MOVE_CW:
        return flir_set_position(rot, 180, priv->target_el);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static const char *flir_get_info(ROT *rot)
{
    char firmware_str[121];
    char info_str[101];

    struct flir_priv_data *priv = (struct flir_priv_data *)
                                  ROTSTATE(rot)->priv;

    sprintf(priv->info, "No Info");

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (flir_request(rot, "V\n", firmware_str, 120) != RIG_OK)
    {
        return "No Info available";
    }

    hl_usleep(500000);

    if (flir_request(rot, "O\n", info_str, 100) != RIG_OK)
    {
        return "No Info available";
    }

    sprintf(priv->info, "Firmware: %s\nPower: %s", firmware_str, info_str);

    return priv->info;
}

static int flir_set_func(ROT *rot, setting_t func, int status)
{
    return -RIG_ENIMPL;
}

static int flir_get_func(ROT *rot, setting_t func, int *status)
{
    return -RIG_ENIMPL;
}

static int flir_set_level(ROT *rot, setting_t level, value_t val)
{
    return -RIG_ENIMPL;
}

static int flir_get_level(ROT *rot, setting_t level, value_t *val)
{
    return -RIG_ENIMPL;
}

static int flir_set_ext_level(ROT *rot, hamlib_token_t token, value_t val)
{
    return -RIG_ENIMPL;
}

static int flir_get_ext_level(ROT *rot, hamlib_token_t token, value_t *val)
{
    return -RIG_ENIMPL;
}

static int flir_set_ext_func(ROT *rot, hamlib_token_t token, int status)
{
    return -RIG_ENIMPL;
}

static int flir_get_ext_func(ROT *rot, hamlib_token_t token, int *status)
{
    return -RIG_ENIMPL;
}

static int flir_set_parm(ROT *rot, setting_t parm, value_t val)
{
    return -RIG_ENIMPL;
}

static int flir_get_parm(ROT *rot, setting_t parm, value_t *val)
{
    return -RIG_ENIMPL;
}

static int flir_set_ext_parm(ROT *rot, hamlib_token_t token, value_t val)
{
    return -RIG_ENIMPL;
}

static int flir_get_ext_parm(ROT *rot, hamlib_token_t token, value_t *val)
{
    return -RIG_ENIMPL;
}

static int flir_get_status(ROT *rot, rot_status_t *status)
{
    const struct flir_priv_data *priv = (struct flir_priv_data *)
                                        ROTSTATE(rot)->priv;
    *status = priv->status;

    return RIG_OK;
}

/*
 * flir rotator capabilities.
 */
struct rot_caps flir_caps =
{
    ROT_MODEL(ROT_MODEL_FLIR),
    .model_name =     "PTU Serial",
    .mfg_name =       "FLIR",
    .version =        "20240818.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   9600,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =       0,
    .post_write_delay =  300,
    .timeout =           400,
    .retry =             3,

    .min_az =     -180.,
    .max_az =     180.,
    .min_el =     0.,
    .max_el =     90.,

    .priv =  NULL,    /* priv */

    .has_get_func =   FLIR_FUNC,
    .has_set_func =   FLIR_FUNC,
    .has_get_level =  FLIR_LEVEL,
    .has_set_level =  ROT_LEVEL_SET(FLIR_LEVEL),
    .has_get_parm =   FLIR_PARM,
    .has_set_parm =   ROT_PARM_SET(FLIR_PARM),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .has_status = FLIR_STATUS,

    .rot_init =     flir_init,
    .rot_cleanup =  flir_cleanup,
    .rot_open =     flir_open,
    .rot_close =    flir_close,

    .set_conf =     flir_set_conf,
    .get_conf =     flir_get_conf,

    .set_position =     flir_set_position,
    .get_position =     flir_get_position,
    .park =     flir_park,
    .stop =     flir_stop,
    .reset =    flir_reset,
    .move =     flir_move,

    .set_func = flir_set_func,
    .get_func = flir_get_func,
    .set_level = flir_set_level,
    .get_level = flir_get_level,
    .set_parm = flir_set_parm,
    .get_parm = flir_get_parm,

    .set_ext_func = flir_set_ext_func,
    .get_ext_func = flir_get_ext_func,
    .set_ext_level = flir_set_ext_level,
    .get_ext_level = flir_get_ext_level,
    .set_ext_parm = flir_set_ext_parm,
    .get_ext_parm = flir_get_ext_parm,

    .get_info = flir_get_info,
    .get_status = flir_get_status,
};

DECLARE_INITROT_BACKEND(flir)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&flir_caps);
    rot_register(&netrotctl_caps);

    return RIG_OK;
}
