/*
 *  Hamlib Rotator backend - GS-232A
 *  Copyright (c) 2001-2012 by Stephane Fillod
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

// cppcheck-suppress *
#include <stdio.h>
// cppcheck-suppress *
// cppcheck-suppress *
#include <string.h>  /* String function definitions */
// cppcheck-suppress *
// cppcheck-suppress *
#include <math.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "idx_builtin.h"

#include "gs232a.h"

#define EOM "\r"
#define REPLY_EOM "\n"

#define BUFSZ 64

#define GS232A_LEVELS ROT_LEVEL_SPEED

/**
 * gs232a_transaction
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
 *   RIG_REJECTED  -  if a negative acknowledge was received or command not
 *                    recognized by rig.
 */
static int
gs232a_transaction(ROT *rot, const char *cmdstr,
                   char *data, size_t data_len, int no_reply)
{
    struct rot_state *rs;
    int retval;
    int retry_read = 0;

    rs = &rot->state;

transaction_write:

    rig_flush(&rs->rotport);

    if (cmdstr)
    {
        retval = write_block(&rs->rotport, (unsigned char *) cmdstr, strlen(cmdstr));

        if (retval != RIG_OK)
        {
            goto transaction_quit;
        }
    }

    /* If no data requested just return */
    if (!data)
    {
        return RIG_OK;
    }

    if (!data_len)
    {
        data_len = BUFSZ;
    }

    if (!no_reply)
    {
        memset(data, 0, data_len);
        retval = read_string(&rs->rotport, (unsigned char *) data, data_len,
                             REPLY_EOM, strlen(REPLY_EOM), 0, 1);

        if (strncmp(data, "\r\n", 2) == 0
                || strchr(data, '>'))
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong response nbytes=%d\n", __func__,
                      (int)strlen(data));
            dump_hex((unsigned char *)data, strlen(data));
            retval = -1; // force retry
        }

        if (retval < 0)
        {
            if (retry_read++ < rot->state.rotport.retry)
            {
                goto transaction_write;
            }

            goto transaction_quit;
        }
    }

#ifdef XXREMOVEDXX

    /* Check that command termination is correct */
    if (strchr(REPLY_EOM, data[strlen(data) - 1]) == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                  __func__, data);

        if (retry_read++ < rig->state.rotport.retry)
        {
            goto transaction_write;
        }

        retval = -RIG_EPROTO;
        goto transaction_quit;
    }

#endif

    if (data[0] == '?')
    {
        /* Invalid command */
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Error for '%s': '%s'\n",
                  __func__, cmdstr, data);
        retval = -RIG_EPROTO;
        goto transaction_quit;
    }

    retval = RIG_OK;
transaction_quit:
    return retval;
}


static int
gs232a_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[64];
    int retval;
    unsigned u_az, u_el;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %.02f %.02f\n", __func__, az, el);

    if (az < 0.0)
    {
        az = az + 360.0;
    }

    u_az = (unsigned)rint(az);
    u_el = (unsigned)rint(el);

    SNPRINTF(cmdstr, sizeof(cmdstr), "W%03u %03u" EOM, u_az, u_el);
    retval = gs232a_transaction(rot, cmdstr, NULL, 0, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int
gs232a_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char posbuf[32];
    int retval, int_az, int_el = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    retval = gs232a_transaction(rot, "C2" EOM, posbuf, sizeof(posbuf), 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    // parse "+0aaa+0eee" and expect both arguments
    if (sscanf(posbuf, "+0%d+0%d", &int_az, &int_el) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong reply '%s', not +0xxx+0xxx format?\n",
                  __func__, posbuf);
        return -RIG_EPROTO;
    }

    *az = (azimuth_t) int_az;
    *el = (elevation_t) int_el;

    rig_debug(RIG_DEBUG_TRACE, "%s: (az, el) = (%.1f, %.1f)\n",
              __func__, *az, *el);

    return RIG_OK;
}

static int
gs232a_rot_stop(ROT *rot)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* All Stop */
    retval = gs232a_transaction(rot, "S" EOM, NULL, 0, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}


static int gs232a_rot_get_level(ROT *rot, setting_t level, value_t *val)
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


static int gs232a_rot_set_level(ROT *rot, setting_t level, value_t val)
{
    struct rot_state *rs = &rot->state;
    char cmdstr[24];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, rot_strlevel(level));

    switch (level)
    {
    case ROT_LEVEL_SPEED:
    {
        int speed = val.i;

        if (speed < 1)
        {
            speed = 1;
        }
        else if (speed > 4)
        {
            speed = 4;
        }

        /* between 1 (slowest) and 4 (fastest) */
        SNPRINTF(cmdstr, sizeof(cmdstr), "X%u" EOM, speed);
        retval = gs232a_transaction(rot, cmdstr, NULL, 0, 1);

        if (retval != RIG_OK)
        {
            return retval;
        }

        rs->current_speed = speed;
        break;
    }

    default:
        return -RIG_ENAVAIL;
    }

    return RIG_OK;
}


static int gs232a_rot_move(ROT *rot, int direction, int speed)
{
    char cmdstr[24];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called %d %d\n", __func__,
              direction, speed);

    if (speed != ROT_SPEED_NOCHANGE)
    {
        value_t gs232a_speed;

        if (speed < 1 || speed > 100)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Invalid speed value (1-100)! (%d)\n", __func__,
                      speed);
            return -RIG_EINVAL;
        }

        gs232a_speed.i = (3 * speed) / 100 + 1;

        retval = gs232a_rot_set_level(rot, ROT_LEVEL_SPEED, gs232a_speed);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    switch (direction)
    {
    case ROT_MOVE_UP:       /* Elevation increase */
        SNPRINTF(cmdstr, sizeof(cmdstr), "U" EOM);
        break;

    case ROT_MOVE_DOWN:     /* Elevation decrease */
        SNPRINTF(cmdstr, sizeof(cmdstr), "D" EOM);
        break;

    case ROT_MOVE_LEFT:     /* Azimuth decrease */
        SNPRINTF(cmdstr, sizeof(cmdstr), "L" EOM);
        break;

    case ROT_MOVE_RIGHT:    /* Azimuth increase */
        SNPRINTF(cmdstr, sizeof(cmdstr), "R" EOM);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid direction value! (%d)\n",
                  __func__, direction);
        return -RIG_EINVAL;
    }

    retval = gs232a_transaction(rot, cmdstr, NULL, 0, 1);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}


static int gs232a_rot_init(ROT *rot)
{
    struct rot_state *rs = &rot->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // Set default speed to half of maximum
    rs->current_speed = 3;

    return RIG_OK;
}


/* ************************************************************************* */
/*
 * Generic GS23 rotator capabilities.
 */

const struct rot_caps gs23_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GS23),
    .model_name =     "GS-23",
    .mfg_name =       "Yaesu/Kenpro",
    .version =        "20220109.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   150,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  400,
    .retry =  3,

    .min_az =     -180.0,
    .max_az =     450.0,  /* vary according to rotator type */
    .min_el =     0.0,
    .max_el =     180.0,

    .has_get_level =  GS232A_LEVELS,
    .has_set_level =  ROT_LEVEL_SET(GS232A_LEVELS),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .rot_init     =  gs232a_rot_init,
    .get_position =  gs232a_rot_get_position,
    .set_position =  gs232a_rot_set_position,
    .stop =          gs232a_rot_stop,
    .get_level =     gs232a_rot_get_level,
    .set_level =     gs232a_rot_set_level,
};


/* ************************************************************************* */
/*
 * Generic GS23 azimuth rotator capabilities.
 */

const struct rot_caps gs23_az_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GS23_AZ),
    .model_name =     "GS-23 azimuth",
    .mfg_name =       "Yaesu/Kenpro",
    .version =        "20220527.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZIMUTH,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   150,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  400,
    .retry =  3,

    .min_az =     -180.0,
    .max_az =     450.0,  /* vary according to rotator type */
    .min_el =     0.0,
    .max_el =     0.0,

    .has_get_level =  GS232A_LEVELS,
    .has_set_level =  ROT_LEVEL_SET(GS232A_LEVELS),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .rot_init     =  gs232a_rot_init,
    .get_position =  gs232a_rot_get_position,
    .set_position =  gs232a_rot_set_position,
    .stop =          gs232a_rot_stop,
    .get_level =     gs232a_rot_get_level,
    .set_level =     gs232a_rot_set_level,
};

/* ************************************************************************* */
/*
 * Generic GS232 rotator capabilities.
 */

const struct rot_caps gs232_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GS232),
    .model_name =     "GS-232",
    .mfg_name =       "Yaesu/Kenpro",
    .version =        "20220109.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   150,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  400,
    .retry =  3,

    .min_az =     -180.0,
    .max_az =     450.0,  /* vary according to rotator type */
    .min_el =     0.0,
    .max_el =     180.0,

    .has_get_level =  GS232A_LEVELS,
    .has_set_level =  ROT_LEVEL_SET(GS232A_LEVELS),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .rot_init     =  gs232a_rot_init,
    .get_position =  gs232a_rot_get_position,
    .set_position =  gs232a_rot_set_position,
    .stop =          gs232a_rot_stop,
    .get_level =     gs232a_rot_get_level,
    .set_level =     gs232a_rot_set_level,
};

/* ************************************************************************* */
/*
 * Generic GS232A rotator capabilities.
 */

const struct rot_caps gs232a_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GS232A),
    .model_name =     "GS-232A",
    .mfg_name =       "Yaesu",
    .version =        "20220109.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   150,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  400,
    .retry =  3,

    .min_az =     -180.0,
    .max_az =     450.0,  /* vary according to rotator type */
    .min_el =     0.0,
    .max_el =     180.0, /* requires G-5400B, G-5600B, G-5500, or G-500/G-550 */

    .has_get_level =  GS232A_LEVELS,
    .has_set_level =  ROT_LEVEL_SET(GS232A_LEVELS),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .rot_init     =  gs232a_rot_init,
    .get_position =  gs232a_rot_get_position,
    .set_position =  gs232a_rot_set_position,
    .stop =          gs232a_rot_stop,
    .move =          gs232a_rot_move,
    .get_level =     gs232a_rot_get_level,
    .set_level =     gs232a_rot_set_level,
};


/* ************************************************************************* */
/*
 * Generic GS232A azimuth rotator capabilities.
 */

const struct rot_caps gs232a_az_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GS232A_AZ),
    .model_name =     "GS-232A azimuth",
    .mfg_name =       "Yaesu",
    .version =        "20220109.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZIMUTH,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   150,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  400,
    .retry =  3,

    .min_az =     -180.0,
    .max_az =     450.0,  /* vary according to rotator type */
    .min_el =     0.0,
    .max_el =     0.0,

    .rot_init     =  gs232a_rot_init,
    .has_get_level =  GS232A_LEVELS,
    .has_set_level =  ROT_LEVEL_SET(GS232A_LEVELS),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .get_position =  gs232a_rot_get_position,
    .set_position =  gs232a_rot_set_position,
    .stop =          gs232a_rot_stop,
    .move =          gs232a_rot_move,
    .get_level =     gs232a_rot_get_level,
    .set_level =     gs232a_rot_set_level,
};


/* ************************************************************************* */
/*
 * Generic GS232A elevation rotator capabilities.
 */

const struct rot_caps gs232a_el_rot_caps =
{
    ROT_MODEL(ROT_MODEL_GS232A_EL),
    .model_name =     "GS-232A elevation",
    .mfg_name =       "Yaesu",
    .version =        "20220109.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_ELEVATION,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   150,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  400,
    .retry =  3,

    .min_az =     0.0,
    .max_az =     0.0,
    .min_el =     0.0,
    .max_el =     180.0, /* requires G-5400B, G-5600B, G-5500, or G-500/G-550 */

    .has_get_level =  GS232A_LEVELS,
    .has_set_level =  ROT_LEVEL_SET(GS232A_LEVELS),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .rot_init     =  gs232a_rot_init,
    .get_position =  gs232a_rot_get_position,
    .set_position =  gs232a_rot_set_position,
    .stop =          gs232a_rot_stop,
    .move =          gs232a_rot_move,
    .get_level =     gs232a_rot_get_level,
    .set_level =     gs232a_rot_set_level,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(gs232a)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&gs232a_rot_caps);
    rot_register(&gs232a_az_rot_caps);
    rot_register(&gs232a_el_rot_caps);
    rot_register(&gs232_generic_rot_caps);
    rot_register(&gs232b_rot_caps);
    rot_register(&gs232b_az_rot_caps);
    rot_register(&gs232b_el_rot_caps);
    rot_register(&f1tetracker_rot_caps);
    rot_register(&gs23_rot_caps);
    rot_register(&gs232_rot_caps);
    rot_register(&amsat_lvb_rot_caps);
    rot_register(&st2_rot_caps);

    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */

