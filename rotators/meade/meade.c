/*
 *  Hamlib Meade telescope rotor backend - main file
 *  Copyright (c) 2018 by Andreas Mueller (DC1MIL)
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
#include <math.h>
#include <sys/time.h>

#include <hamlib/rotator.h>
#include <num_stdio.h>

#include "serial.h"
#include "misc.h"
#include "register.h"

#include "meade.h"


struct meade_priv_data
{
    azimuth_t az;
    elevation_t el;

    struct timeval tv;    /* time last az/el update */
    azimuth_t target_az;
    elevation_t target_el;
    char product_name[32];
};

/**
 * Command list:
 * See https://www.meade.com/support/LX200CommandSet.pdf
 * and https://www.meade.com/support/TelescopeProtocol_2010-10.pdf for newer
 * Firmware Versions
 *
 * Not the full set of available commands is used, the list here shows
 * only the commands of the telescope used by hamlib
 *
 * All used Commands are supported by Meade Telescopes with LX-200 protocol
 * (e.g. DS-2000 with Autostar) and should also work with the LX16 and
 * LX200GPS.
 * Tested only with DS-2000 and AutoStar 494 together with Meade 506 i2c to
 * Serial cable. But should also work with other AutoStars and the regular
 * Serial Cable.
 *
 * | Command     | Attribute | Return value | Description              |
 * ---------------------------------------------------------------------
 * | :Me#        | -         | -            | Moves telescope east     |
 * | :Mn#        | -         | -            | Moves telescope north    |
 * | :Ms#        | -         | -            | Moves telescope south    |
 * | :Mw#        | -         | -            | Moves telescope west     |
 * | :AL#        | -         | -            | Set to Land mode         |
 * | :Sz DDD*MM# | D,M       | 1' == OK     | Set Target azimuth       |
 * | :SasDD*MM#  | s,D,M     | 1' == OK     | Set Target elevation     |
 * | :Mw#        | -         | -            | Moves telescope west     |
 * | :Q#         | -         | -            | Halt all slewing         |
 * | :SoDD#      | D         | '1' == OK    | Set minimal elevation    |
 * | :ShDD#      | D         | '1' == OK    | Set maximal elevation    |
 * | :MA#        | -         | '0' == OK    | GoTo Target              |
 * | :D#         | -         | 0x7F == YES  | Check if active movement |
 *
 */

/**
 * meade_transaction
 *
 * cmdstr - Command to be sent to the rig.
 * data - Buffer for reply string.  Can be NULL, indicating that no reply is
 *        is needed, but answer will still be read.
 * data_len - in: Size of buffer. It is the caller's responsibily to provide
 *            a large enough buffer for all possible replies for a command.
 *
 * returns:
 *   RIG_OK  -  if no error occurred.
 *   RIG_EIO  -  if an I/O error occurred while sending/receiving data.
 *   RIG_ETIMEOUT  -  if timeout expires without any characters received.
 */
static int meade_transaction(ROT *rot, const char *cmdstr,
                             char *data, size_t *data_len, size_t expected_return_length)
{
    struct rot_state *rs;
    int return_value;
    int retry_read = 0;

    rs = &rot->state;

    while (1)
    {
transaction:
        rig_flush(&rs->rotport);

        if (cmdstr)
        {
            return_value = write_block(&rs->rotport, (unsigned char *) cmdstr,
                                       strlen(cmdstr));

            if (return_value != RIG_OK)
            {
                *data_len = 0;
                return return_value;
            }
        }

        /* Not all commands will send a return value, so use data = NULL if no
           return value is expected, Strings end with '#' */
        if (data != NULL)
        {
            return_value = read_string(&rs->rotport, (unsigned char *) data,
                                       expected_return_length + 1,
                                       "\r\n", strlen("\r\n"), 0, 1);

            if (return_value > 0)
            {
                *data_len = return_value;
                return RIG_OK;
            }
            else
            {
                if (retry_read++ >= rot->state.rotport.retry)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: read_string error %s\n", __func__,
                              rigerror(return_value));
                    *data_len = 0;
                    return -RIG_ETIMEOUT;
                }
                else
                {
                    goto transaction;
                }
            }
        }
        else
        {
            return RIG_OK;
        }
    }
}

/*
 * Initialization
 */
static int meade_init(ROT *rot)
{
    struct meade_priv_data *priv;

    rot->state.priv = (struct meade_priv_data *)
                      calloc(1, sizeof(struct meade_priv_data));

    if (!rot->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called version %s\n", __func__,
              rot->caps->version);
    rot->state.rotport.type.rig = RIG_PORT_SERIAL;

    priv->az = priv->el = 0;

    priv->target_az = priv->target_el = 0;

    return RIG_OK;
}

/*
 * Cleanup
 */
static int meade_cleanup(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rot->state.priv)
    {
        free(rot->state.priv);
    }

    rot->state.priv = NULL;

    return RIG_OK;
}

/*
 * Opens the Port and sets all needed parameters for operation
 */
static int meade_open(ROT *rot)
{
    char return_str[BUFSIZE];
    size_t return_str_size = 0;
    struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // Get our product name for any custom things we need to do
    // The LX200 does not have :GVP# so no response will default to LX200
    retval = meade_transaction(rot, ":GVP#", return_str, &return_str_size,
                               sizeof(return_str));

    if (retval != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: meade_transaction %s\n", __func__, rigerror(retval)); }

    if (return_str_size > 0) { strtok(return_str, "#"); }

    strcpy(priv->product_name, return_str_size > 0 ? return_str : "LX200 Assumed");
    rig_debug(RIG_DEBUG_VERBOSE, "%s product_name=%s\n", __func__,
              priv->product_name);

    /* Set Telescope to Land alignment mode to deactivate sloping */
    /* Allow 0-90 Degree Elevation */
    if (strcmp(priv->product_name, "Autostar"))  // if we're not an audiostar
    {
        retval = meade_transaction(rot, ":AL#:So00#:Sh90#", NULL, 0, 0);
    }
    else
    {
        // Audiostar elevation is in arcminutes
        retval = meade_transaction(rot, ":So00#:Sh5400#", NULL, 0, 0);
    }

    if (retval != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: meade_transaction %s\n", __func__, rigerror(retval)); }

    return RIG_OK;
}

/*
 * Closes the port and stops all movement
 */
static int meade_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    /* Stop all Movement */
    return meade_transaction(rot, ":Q#", NULL, 0, 0);
}

/*
 * Sets the target position and starts movement
 *
 * az: Target azimuth
 * el: Target elevation
 */
static int meade_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;
    char cmd_str[BUFSIZE];
    char return_str[BUFSIZE];
    size_t return_str_size;
    float az_degrees, az_minutes, el_degrees, el_minutes;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    az_degrees = floor(az);
    az_minutes = (az - az_degrees) * 60;
    el_degrees = floor(el);
    el_minutes = (el - el_degrees) * 60;

    // LX200 won't do 180 degrees exactly...so we fudge everybody
    if (strstr(priv->product_name, "LX200") && az_degrees == 180
            && az_minutes == 0)
    {
        az_degrees = 179;
        az_minutes = 59;
    }

    /* Check if there is an active movement and stop it */
    /* Undesirable behavior if stopped can happen */
    /* So we just ignore commands while moving */
    /* Should we return RIG_OK or an error though? */
    meade_transaction(rot, ":D#", return_str, &return_str_size, sizeof(return_str));

    // LX200 return 0xff bytes and Autostart returns 0x7f
    if (return_str_size > 0 && ((return_str[0] & 0x7f) == 0x7f))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: rotor is moving...ignoring move\n", __func__);
        return RIG_OK; // we don't give an error -- just ignore it
    }

    priv->target_az = az;
    priv->target_el = el;

    num_sprintf(cmd_str, ":Sz %03.0f*%02.0f#:Sa+%02.0f*%02.0f#:MA#",
                az_degrees, az_minutes, el_degrees, el_minutes);

    meade_transaction(rot, cmd_str, return_str, &return_str_size, 3);

    /* '1' == Azimuth accepted '1' == Elevation accepted '0' == No error */
    /* MA command may return 1=Lower than or 2=Higher than min/max elevation */
    if (return_str_size > 0 && strstr(return_str, "110") != NULL)
    {
        return RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: expected 110, got %s\n", __func__,
                  return_str);
        return RIG_EINVAL;
    }
}

/*
 * Get position of rotor, simulating slow rotation
 */
static int meade_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char return_str[BUFSIZE];
    char eom;
    size_t return_str_size;
    int az_degrees, az_minutes, az_seconds, el_degrees, el_minutes, el_seconds;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    meade_transaction(rot, ":GZ#:GA#", return_str, &return_str_size, BUFSIZE);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: returned '%s'\n", __func__, return_str);
    // GZ returns DDD*MM# or DDD*MM'SS#
    // GA returns sDD*MM# or sDD*MM'SS#
    n = sscanf(return_str, "%d%*c%d:%d#%d%*c%d:%d%c", &az_degrees, &az_minutes,
               &az_seconds, &el_degrees, &el_minutes, &el_seconds, &eom);

    if (n != 7 || eom != '#')
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: not 6 args in '%s'\nTrying low precision\n",
                  __func__, return_str);
        az_seconds = el_seconds = 0;
        n = sscanf(return_str, "%d%*c%d#%d%*c%d%c", &az_degrees, &az_minutes,
                   &el_degrees, &el_minutes, &eom);

        if (n != 5 || eom != '#')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: not 4 args in '%s', parsing failed\n", __func__,
                      return_str);
            return -RIG_EPROTO;
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: az=%03d:%02d:%02d, el=%03d:%02d:%02d\n",
              __func__, az_degrees, az_minutes, az_seconds, el_degrees, el_minutes,
              el_seconds);
    *az = dmmm2dec(az_degrees, az_minutes, az_seconds, az_seconds);
    *el = dmmm2dec(el_degrees, el_minutes, el_seconds, el_seconds);
    return RIG_OK;
}

/*
 * Stops all movement
 */
static int meade_stop(ROT *rot)
{
    struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;
    azimuth_t az;
    elevation_t el;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    meade_transaction(rot, ":Q#", NULL, 0, 0);
    meade_get_position(rot, &az, &el);

    priv->target_az = priv->az = az;
    priv->target_el = priv->el = el;

    return RIG_OK;
}

/*
 * Moves to Home Position
 */
static int meade_park(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Assume home is 0,0 */
    meade_set_position(rot, 0, 0);

    return RIG_OK;
}

/*
 * Reset: Nothing to do except parking
 */
static int meade_reset(ROT *rot, rot_reset_t reset)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    meade_park(rot);

    return RIG_OK;
}

/*
 * Movement to direction
 */
static int meade_move(ROT *rot, int direction, int speed)
{
    struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: Direction = %d, Speed = %d\n", __func__,
              direction, speed);

    switch (direction)
    {
    case ROT_MOVE_UP:
        return meade_set_position(rot, priv->target_az, 90);

    case ROT_MOVE_DOWN:
        return meade_set_position(rot, priv->target_az, 0);

    case ROT_MOVE_CCW:
        return meade_set_position(rot, -180, priv->target_el);

    case ROT_MOVE_CW:
        return meade_set_position(rot, 180, priv->target_el);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static const char *meade_get_info(ROT *rot)
{
    static char buf[256]; // this is not thread-safe but not important either
    struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(buf, sizeof(buf),
             "Meade telescope rotator with LX200 protocol.\nModel: %s",
             priv->product_name);
    return buf;
}

/*
 * Meade telescope rotator capabilities.
 */

const struct rot_caps meade_caps =
{
    ROT_MODEL(ROT_MODEL_MEADE),
    .model_name =       "LX200/Autostar",
    .mfg_name =         "Meade",
    .version =          "20220109.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rot_type =         ROT_TYPE_AZEL,

    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 200,
    .timeout =          400,
    .retry =            2,

    .min_az =      0.,
    .max_az =    360.,
    .min_el =      0.,
    .max_el =     90.,


    .priv =      NULL,  /* priv */

    .rot_init =         meade_init,
    .rot_cleanup =      meade_cleanup,
    .rot_open =         meade_open,
    .rot_close =        meade_close,

    .set_position =     meade_set_position,
    .get_position =     meade_get_position,
    .park =             meade_park,
    .stop =             meade_stop,
    .reset =            meade_reset,
    .move =             meade_move,

    .get_info =         meade_get_info,

    .get_conf =         rot_get_conf,
    .set_conf =         rot_set_conf,
};

DECLARE_INITROT_BACKEND(meade)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&meade_caps);

    return RIG_OK;
}
