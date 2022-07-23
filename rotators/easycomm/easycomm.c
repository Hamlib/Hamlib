/*
 *  Hamlib Rotator backend - Easycom
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *  Copyright (c) 2014 by Alexander Schultze <alexschultze@gmail.com>
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

#include <stdio.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "idx_builtin.h"

#include "easycomm.h"

#define EASYCOMM3_LEVELS ROT_LEVEL_SPEED

/* ************************************************************************* */
/**
 *  easycomm_transaction
 *
 *  Assumes rot!=NULL and cmdstr!=NULL
 *
 *  cmdstr   - string to send to rotator
 *  data     - buffer for reply string
 *  data_len - (input) Maximum size of buffer
 *             (output) Number of bytes read.
 */
static int
easycomm_transaction(ROT *rot, const char *cmdstr, char *data, size_t data_len)
{
    struct rot_state *rs;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %s\n", __func__, cmdstr);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rs = &rot->state;
    rig_flush(&rs->rotport);
    retval = write_block(&rs->rotport, (unsigned char *) cmdstr, strlen(cmdstr));

    if (retval != RIG_OK)
    {
        goto transaction_quit;
    }

    if (data == NULL)
    {
        return RIG_OK;    /* don't want a reply */
    }

    retval = read_string(&rs->rotport, (unsigned char *) data, data_len,
                         "\n", 1, 0, 1);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s read_string failed with status %d\n", __func__,
                  retval);
        goto transaction_quit;
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s read_string: %s\n", __func__, data);
        retval = RIG_OK;
    }

transaction_quit:
    return retval;
}

/* ************************************************************************* */

static int
easycomm_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[64];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    if (rot->caps->rot_model == ROT_MODEL_EASYCOMM1)
    {
        SNPRINTF(cmdstr, sizeof(cmdstr), "AZ%.1f EL%.1f UP000 XXX DN000 XXX\n", az, el);
    }
    else   // for easycomm 2 & 3 and upwards
    {
        SNPRINTF(cmdstr, sizeof(cmdstr), "AZ%.1f EL%.1f\n", az, el);
    }

    retval = easycomm_transaction(rot, cmdstr, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TODO: Error processing */
    return RIG_OK;
}

static int
easycomm_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char cmdstr[16], ackbuf[32];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    SNPRINTF(cmdstr, sizeof(cmdstr), "AZ EL \n");

    retval = easycomm_transaction(rot, cmdstr, ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s got error: %d\n", __func__, retval);
        return retval;
    }

    /* Parse parse string to extract AZ,EL values */
    rig_debug(RIG_DEBUG_TRACE, "%s got response: %s\n", __func__, ackbuf);
    retval = sscanf(ackbuf, "AZ%f EL%f", az, el);

    if (retval != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown response (%s)\n", __func__, ackbuf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

static int
easycomm_rot_stop(ROT *rot)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    retval = easycomm_transaction(rot, "SA SE \n", NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TODO: error processing */

    return RIG_OK;
}

static int
easycomm_rot_reset(ROT *rot, rot_reset_t rst)
{
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    retval = easycomm_transaction(rot, "RESET\n", NULL, 0);

    if (retval != RIG_OK)   /* Custom command (not in Easycomm) */
    {
        return retval;
    }

    return RIG_OK;
}

static int
easycomm_rot_park(ROT *rot)
{
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    retval = easycomm_transaction(rot, "PARK\n", NULL, 0);

    if (retval != RIG_OK)   /* Custom command (not in Easycomm) */
    {
        return retval;
    }

    return RIG_OK;
}

static int
easycomm_rot_move(ROT *rot, int direction, int speed)
{
    char cmdstr[24];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* For EasyComm 1/2/3 */
    switch (direction)
    {
    case ROT_MOVE_UP:       /* Elevation increase */
        SNPRINTF(cmdstr, sizeof(cmdstr), "MU\n");
        break;

    case ROT_MOVE_DOWN:     /* Elevation decrease */
        SNPRINTF(cmdstr, sizeof(cmdstr), "MD\n");
        break;

    case ROT_MOVE_LEFT:     /* Azimuth decrease */
        SNPRINTF(cmdstr, sizeof(cmdstr), "ML\n");
        break;

    case ROT_MOVE_RIGHT:    /* Azimuth increase */
        SNPRINTF(cmdstr, sizeof(cmdstr), "MR\n");
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid direction value! (%d)\n", __func__,
                  direction);
        return -RIG_EINVAL;
    }

    retval = easycomm_transaction(rot, cmdstr, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int easycomm_rot_move_velocity(ROT *rot, int direction, int speed)
{
    struct rot_state *rs = &rot->state;
    char cmdstr[24];
    int retval;
    int easycomm_speed;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (speed == ROT_SPEED_NOCHANGE)
    {
        easycomm_speed = rs->current_speed;
    }
    else
    {
        if (speed < 1 || speed > 100)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Invalid speed value (1-100)! (%d)\n", __func__,
                      speed);
            return -RIG_EINVAL;
        }

        easycomm_speed = ((speed - 1) * 100);
        rs->current_speed = easycomm_speed;
    }

    /* Speed for EasyComm 3 */
    switch (direction)
    {
    case ROT_MOVE_UP:       /* Elevation increase */
        SNPRINTF(cmdstr, sizeof(cmdstr), "VU%04d\n", easycomm_speed);
        break;

    case ROT_MOVE_DOWN:     /* Elevation decrease */
        SNPRINTF(cmdstr, sizeof(cmdstr), "VD%04d\n", easycomm_speed);
        break;

    case ROT_MOVE_LEFT:     /* Azimuth decrease */
        SNPRINTF(cmdstr, sizeof(cmdstr), "VL%04d\n", easycomm_speed);
        break;

    case ROT_MOVE_RIGHT:    /* Azimuth increase */
        SNPRINTF(cmdstr, sizeof(cmdstr), "VR%04d\n", easycomm_speed);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid direction value! (%d)\n", __func__,
                  direction);
        return -RIG_EINVAL;
    }

    retval = easycomm_transaction(rot, cmdstr, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int easycomm_rot_get_level(ROT *rot, setting_t level, value_t *val)
{
    struct rot_state *rs = &rot->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, rot_strlevel(level));

    switch (level)
    {
    case ROT_LEVEL_SPEED:
        val->i = rs->current_speed;
        break;

    default:
        return -RIG_ENAVAIL;
    }

    return RIG_OK;
}


static int easycomm_rot_set_level(ROT *rot, setting_t level, value_t val)
{
    struct rot_state *rs = &rot->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, rot_strlevel(level));

    switch (level)
    {
    case ROT_LEVEL_SPEED:
    {
        int speed = val.i;

        if (speed < 0)
        {
            speed = 0;
        }
        else if (speed > 9999)
        {
            speed = 9999;
        }

        rs->current_speed = speed;
        break;
    }

    default:
        return -RIG_ENAVAIL;
    }

    return RIG_OK;
}

/*
 * Get Info
 * returns the model name string
 */
static const char *easycomm_rot_get_info(ROT *rot)
{
    const struct rot_caps *rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return (const char *) - RIG_EINVAL;
    }

    rc = rot->caps;

    return rc->model_name;
}

/*
 * Receive status / configuration / output
 *
 * For configuration registers, *val must contain string of register e.g. '0'-'f'
 */

static int easycomm_rot_get_conf(ROT *rot, token_t token, char *val)
{
    char cmdstr[16], ackbuf[32];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: token = %d", __func__, (int)token);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_GET_STATUS:
        SNPRINTF(cmdstr, sizeof(cmdstr), "GS\n;");
        break;

    case TOK_GET_ERRORS:
        SNPRINTF(cmdstr, sizeof(cmdstr), "GE\n;");
        break;

    case TOK_GET_INPUT:
        SNPRINTF(cmdstr, sizeof(cmdstr), "IP\n;");
        break;

    case TOK_GET_ANALOG_INPUT:
        SNPRINTF(cmdstr, sizeof(cmdstr), "AN\n;");
        break;

    case TOK_GET_VERSION:
        SNPRINTF(cmdstr, sizeof(cmdstr), "VE\n;");
        break;

    case TOK_GET_CONFIG:
        SNPRINTF(cmdstr, sizeof(cmdstr), "CR %c\n;", *val);
        break;

    default:


        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s, *val = %c\n", __func__, cmdstr,
              *val);



    retval = easycomm_transaction(rot, cmdstr, ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s got error: %d\n", __func__, retval);
        return retval;
    }


    rig_debug(RIG_DEBUG_TRACE, "%s got response: %s\n", __func__, ackbuf);
    /* Return given string at correct position*/
    memcpy(val, ackbuf + 2, sizeof(ackbuf) - 2); /* CCxxxxxx */
    return RIG_OK;
}

/*
 * Set configuration
 *
 * For configuration registers, *val must contain char of register e.g. '0'-'f' followed by setting
 * e.g. x,yyyyy
 */

static int easycomm_rot_set_conf(ROT *rot, token_t token, const char *val)
{
    char cmdstr[16];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: token = %d", __func__, (int)token);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_SET_CONFIG:
        SNPRINTF(cmdstr, sizeof(cmdstr), "CW%s\n;", val);
        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s, *val = %c\n", __func__, cmdstr,
              *val);

    retval = easycomm_transaction(rot, cmdstr, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s got error: %d\n", __func__, retval);
        return retval;
    }

    return RIG_OK;
}


static int easycomm_rot_init(ROT *rot)
{
    struct rot_state *rs = &rot->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // Set default speed to half of maximum
    rs->current_speed = 5000;

    return RIG_OK;
}

/* ************************************************************************* */
/*
 * Easycomm rotator capabilities.
 */

/** EasycommI implement essentially only the set position function, but
 *  I included the stop command too.  The radio control tags is only included
 *  as dummy entries because the spec require them.
 */
const struct rot_caps easycomm1_rot_caps =
{
    ROT_MODEL(ROT_MODEL_EASYCOMM1),
    .model_name =     "EasycommI",
    .mfg_name =       "Hamlib",
    .version =        "20220109.0",
    .copyright =   "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .priv =  NULL,    /* priv */

    .set_position =  easycomm_rot_set_position,
    .stop =   easycomm_rot_stop,
    .get_info =  easycomm_rot_get_info,
};

/* EasycommII implement most of the functions. Again the radio tags
 * is only dummy values.
 */
const struct rot_caps easycomm2_rot_caps =
{
    ROT_MODEL(ROT_MODEL_EASYCOMM2),
    .model_name =     "EasycommII",
    .mfg_name =       "Hamlib",
    .version =        "20191206.0",
    .copyright =   "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .priv =  NULL,    /* priv */

    .rot_init =  NULL,
    .rot_cleanup =  NULL,
    .rot_open =  NULL,
    .rot_close =  NULL,

    .get_position =  easycomm_rot_get_position,
    .set_position =  easycomm_rot_set_position,
    .stop =   easycomm_rot_stop,
    .park =  easycomm_rot_park,
    .reset =  easycomm_rot_reset,
    .move =  easycomm_rot_move,
    .set_conf = easycomm_rot_set_conf,
    .get_conf = easycomm_rot_get_conf,
    .get_info =  easycomm_rot_get_info,
};

/* EasycommIII provides changes Moving functions and info.
 */
const struct rot_caps easycomm3_rot_caps =
{
    ROT_MODEL(ROT_MODEL_EASYCOMM3),
    .model_name =     "EasycommIII",
    .mfg_name =       "Hamlib",
    .version =        "20201203.0",
    .copyright =   "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_OTHER,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .priv =  NULL,    /* priv */

    .has_get_level =  EASYCOMM3_LEVELS,
    .has_set_level =  ROT_LEVEL_SET(EASYCOMM3_LEVELS),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 0 }, .max = { .i = 9999 }, .step = { .i = 1 } } },

    .rot_init =  easycomm_rot_init,
    .rot_cleanup =  NULL,
    .rot_open =  NULL,
    .rot_close =  NULL,

    .get_position =  easycomm_rot_get_position,
    .set_position =  easycomm_rot_set_position,
    .stop =   easycomm_rot_stop,
    .park =  easycomm_rot_park,
    .reset =  easycomm_rot_reset,
    .move =  easycomm_rot_move_velocity,
    .get_level = easycomm_rot_get_level,
    .set_level = easycomm_rot_set_level,
    .set_conf = easycomm_rot_set_conf,
    .get_conf = easycomm_rot_get_conf,
    .get_info =  easycomm_rot_get_info,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(easycomm)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&easycomm1_rot_caps);
    rot_register(&easycomm2_rot_caps);
    rot_register(&easycomm3_rot_caps);
    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
