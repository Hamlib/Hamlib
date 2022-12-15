/*
 *  Hamlib Prosistel backend
 *  Copyright (c) 2015 by Dario Ventura IZ7CRX
 *  Copyright (c) 2020 by Mikael Nousiainen OH3BHX
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
#include <string.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "register.h"
#include "num_stdio.h"

#include "prosistel.h"

#define BUFSZ 128
#define CR "\r"
#define STX "\x02"

struct prosistel_rot_priv_caps
{
    float angle_multiplier;
    char azimuth_id;
    char elevation_id;

    int stop_angle;
};

/**
 * prosistel_transaction
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
 *   RIG_EPROTO - if a the response does not follow Prosistel protocol
 *   RIG_ETIMEOUT  -  if timeout expires without any characters received.
 */
static int prosistel_transaction(ROT *rot, const char *cmdstr,
                                 char *data, size_t data_len)
{
    struct rot_state *rs;
    int retval;
    int retry_read = 0;
    char replybuf[BUFSZ];

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

    /* Always read the reply to know whether the cmd went OK */
    if (!data)
    {
        data = replybuf;
    }

    if (!data_len)
    {
        data_len = BUFSZ;
    }

    // Remember to check for STXA,G,R or STXA,?,XXX,R 10 bytes
    retval = read_string(&rs->rotport, (unsigned char *) data, 20, CR, strlen(CR),
                         0, 1);

    if (retval < 0)
    {
        if (retry_read++ < rot->state.rotport.retry)
        {
            goto transaction_write;
        }

        goto transaction_quit;
    }

    // Check if reply matches issued command
    if (cmdstr && data[0] == 0x02 && data[3] == cmdstr[2])
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s Command %c reply received\n", __func__,
                  data[3]);
        retval = RIG_OK;
    }
    else if (cmdstr)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s Error Command issued: %c doesn't match reply %c\n", __func__, cmdstr[2],
                  data[3]);
        retval = -RIG_EPROTO;
    }

transaction_quit:
    return retval;
}


static int prosistel_rot_open(ROT *rot)
{
    struct prosistel_rot_priv_caps *priv_caps =
        (struct prosistel_rot_priv_caps *) rot->caps->priv;
    char cmdstr[64];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    // Disable CPM mode - CPM stands for Continuous Position Monitor operating mode
    // The rotator controller sends position data continuously when CPM is enabled

    // Disable CPM for azimuth if the rotator has an azimuth rotator
    if (rot->caps->rot_type == ROT_TYPE_AZIMUTH
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        num_sprintf(cmdstr, STX"%cS"CR, priv_caps->azimuth_id);
        retval = prosistel_transaction(rot, cmdstr, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    // Disable CPM for elevation if the rotator has an elevation rotator
    if (rot->caps->rot_type == ROT_TYPE_ELEVATION
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        num_sprintf(cmdstr, STX"%cS"CR, priv_caps->elevation_id);
        retval = prosistel_transaction(rot, cmdstr, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}


static int prosistel_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    struct prosistel_rot_priv_caps *priv_caps =
        (struct prosistel_rot_priv_caps *) rot->caps->priv;
    char cmdstr[64];
    int retval = -RIG_EINTERNAL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.1f %.1f\n", __func__, az, el);

    // Leading zeros in angle values are optional according to Prosistel protocol documentation

    // Set azimuth only if the rotator has the capability to do so
    // It is an error to set azimuth if it's not supported by the rotator controller
    if (rot->caps->rot_type == ROT_TYPE_AZIMUTH
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        num_sprintf(cmdstr, STX"%cG%.0f"CR, priv_caps->azimuth_id,
                    az * priv_caps->angle_multiplier);
        retval = prosistel_transaction(rot, cmdstr, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    // Set elevation only if the rotator has the capability to do so
    // It is an error to set elevation if it's not supported by the rotator controller
    if (rot->caps->rot_type == ROT_TYPE_ELEVATION
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        num_sprintf(cmdstr, STX"%cG%.0f"CR, priv_caps->elevation_id,
                    el * priv_caps->angle_multiplier);
        retval = prosistel_transaction(rot, cmdstr, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return retval;
}


static int prosistel_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    struct prosistel_rot_priv_caps *priv_caps =
        (struct prosistel_rot_priv_caps *) rot->caps->priv;
    char cmdstr[64];
    char data[20];
    float posval;
    int retval = RIG_OK;
    int n;

    // Query azimuth only if the rotator has the capability to do so
    // It is an error to query for azimuth if it's not supported by the rotator controller
    if (rot->caps->rot_type == ROT_TYPE_AZIMUTH
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        char rot_id;

        num_sprintf(cmdstr, STX"%c?"CR, priv_caps->azimuth_id);
        retval = prosistel_transaction(rot, cmdstr, data, sizeof(data));

        if (retval != RIG_OK)
        {
            return retval;
        }

        // Example response of 290 degree azimuth with 3 digits
        // 02 41 2c 3f 2c 32 39 30 2c 52 0d      .A,?,290,R.
        // Example response of 100 degree azimuth with 4 digits
        // 02 41 2c 3f 2c 31 30 30 30 2c 52 0d   .A,?,1000,R.
        n = sscanf(data, "%*c%c,?,%f,%*c.", &rot_id, &posval);

        if (n != 2 || rot_id != priv_caps->azimuth_id)
        {
            rig_debug(RIG_DEBUG_ERR, "%s failed to parse azimuth '%s'\n", __func__, data);
            return -RIG_EPROTO;
        }

        posval /= priv_caps->angle_multiplier;

        rig_debug(RIG_DEBUG_VERBOSE, "%s got position from '%s' converted to %f\n",
                  __func__, data, posval);

        *az = (azimuth_t) posval;
    }
    else
    {
        *az = 0;
    }

    // Query elevation only if the rotator has the capability to do so
    // It is an error to query for elevation if it's not supported by the rotator controller
    if (rot->caps->rot_type == ROT_TYPE_ELEVATION
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        char rot_id;

        num_sprintf(cmdstr, STX"%c?"CR, priv_caps->elevation_id);
        retval = prosistel_transaction(rot, cmdstr, data, sizeof(data));

        if (retval != RIG_OK)
        {
            return retval;
        }

        // Example response of 90 degree elevation with 4 digits
        // 02 42 2c 3f 2c 30 39 30 30 2c 52 0d   .B,?,0900,R.
        // The response will be an error if no elevation is available
        // 02 42 2c 3f 2c 45 2c 30 30 30 30 33 0d    .B,?,E,00003.
        n = sscanf(data, "%*c%c,?,%f,%*c.", &rot_id, &posval);

        if (n != 2 || rot_id != priv_caps->elevation_id)
        {
            rig_debug(RIG_DEBUG_ERR, "%s failed to parse elevation '%s'\n", __func__, data);
            return -RIG_EPROTO;
        }

        posval /= priv_caps->angle_multiplier;

        rig_debug(RIG_DEBUG_VERBOSE, "%s got position from '%s' converted to %f\n",
                  __func__, data, posval);

        *el = (elevation_t) posval;
    }
    else
    {
        *el = 0;
    }

    return retval;
}


static int prosistel_rot_stop(ROT *rot)
{
    struct prosistel_rot_priv_caps *priv_caps =
        (struct prosistel_rot_priv_caps *) rot->caps->priv;
    char cmdstr[64];
    int retval = -RIG_EINTERNAL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // Stop azimuth only if the rotator has the capability to do so
    if (rot->caps->rot_type == ROT_TYPE_AZIMUTH
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        num_sprintf(cmdstr, STX"%cG%d"CR, priv_caps->azimuth_id, priv_caps->stop_angle);
        retval = prosistel_transaction(rot, cmdstr, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    // Stop elevation only if the rotator has the capability to do so
    if (rot->caps->rot_type == ROT_TYPE_ELEVATION
            || rot->caps->rot_type == ROT_TYPE_AZEL)
    {
        num_sprintf(cmdstr, STX"%cG%d"CR, priv_caps->elevation_id,
                    priv_caps->stop_angle);
        retval = prosistel_transaction(rot, cmdstr, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return retval;
}


static const struct prosistel_rot_priv_caps prosistel_rot_az_or_el_priv_caps =
{
    .angle_multiplier = 1.0f,
    /**
     * Angle 977 = soft stop, stops the rotator using using PWM if PWM mode is enabled, otherwise results in a fast stop
     * Angle 999 = fast stop, stops the rotator immediately
     */
    .stop_angle = 997,
    .azimuth_id = 'A',
    .elevation_id = 'E',
};


/*
 * The Prosistel Combi-Track azimuth + elevation controllers use angle values multiplied by 10 and have
 * two "units" with identifier A and B (instead of A and E).
 */
static const struct prosistel_rot_priv_caps prosistel_rot_combitrack_priv_caps =
{
    .angle_multiplier = 10.0f,
    /**
     * Angle 9777 = soft stop, stops the rotator using using PWM if PWM mode is enabled, otherwise results in a fast stop
     * Angle 9999 = fast stop, stops the rotator immediately
     */
    .stop_angle = 9777,
    .azimuth_id = 'A',
    .elevation_id = 'B',
};


// Elevation rotator with Control box D using azimuth logic
static const struct prosistel_rot_priv_caps prosistel_rot_el_cboxaz =
{
    .angle_multiplier = 1.0f,
    .stop_angle = 997,
    .elevation_id = 'A',
};


/*
 * Prosistel rotator capabilities
 */
const struct rot_caps prosistel_d_az_rot_caps =
{
    ROT_MODEL(ROT_MODEL_PROSISTEL_D_AZ),
    .model_name =     "D azimuth",
    .mfg_name =       "Prosistel",
    .version =        "20201215.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZIMUTH,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min  = 9600,
    .serial_rate_max  = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 0,
    .post_write_delay = 0,
    .timeout          = 3000,
    .retry            = 3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     0.0,

    .priv = &prosistel_rot_az_or_el_priv_caps,

    .rot_open = prosistel_rot_open,
    .stop = prosistel_rot_stop,
    .set_position = prosistel_rot_set_position,
    .get_position = prosistel_rot_get_position,
};


const struct rot_caps prosistel_d_el_rot_caps =
{
    ROT_MODEL(ROT_MODEL_PROSISTEL_D_EL),
    .model_name =     "D elevation",
    .mfg_name =       "Prosistel",
    .version =        "20201215.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_ELEVATION,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min  = 9600,
    .serial_rate_max  = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 0,
    .post_write_delay = 0,
    .timeout          = 3000,
    .retry            = 3,

    .min_az =     0.0,
    .max_az =     0.0,
    .min_el =     0.0,
    .max_el =     90.0,

    .priv = &prosistel_rot_az_or_el_priv_caps,

    .rot_open = prosistel_rot_open,
    .stop = prosistel_rot_stop,
    .set_position = prosistel_rot_set_position,
    .get_position = prosistel_rot_get_position,
};


const struct rot_caps prosistel_combi_track_azel_rot_caps =
{
    ROT_MODEL(ROT_MODEL_PROSISTEL_COMBI_TRACK_AZEL),
    .model_name =     "Combi-Track az+el",
    .mfg_name =       "Prosistel",
    .version =        "20201215.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min  = 9600,
    .serial_rate_max  = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 0,
    .post_write_delay = 0,
    .timeout          = 3000,
    .retry            = 3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     90.0,

    .priv = &prosistel_rot_combitrack_priv_caps,

    .rot_open = prosistel_rot_open,
    .stop = prosistel_rot_stop,
    .set_position = prosistel_rot_set_position,
    .get_position = prosistel_rot_get_position,
};


// Elevation rotator with Control box D using azimuth logic
const struct rot_caps prosistel_d_el_cboxaz_rot_caps =
{
    ROT_MODEL(ROT_MODEL_PROSISTEL_D_EL_CBOXAZ),
    .model_name =     "D elevation CBOX az",
    .mfg_name =       "Prosistel",
    .version =        "20221215.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_ELEVATION,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min  = 9600,
    .serial_rate_max  = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 0,
    .post_write_delay = 0,
    .timeout          = 3000,
    .retry            = 3,

    .min_az =     0.0,
    .max_az =     0.0,
    .min_el =     0.0,
    .max_el =     90.0,

    .priv = &prosistel_rot_el_cboxaz,

    .rot_open = prosistel_rot_open,
    .stop = prosistel_rot_stop,
    .set_position = prosistel_rot_set_position,
    .get_position = prosistel_rot_get_position,
};


DECLARE_INITROT_BACKEND(prosistel)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&prosistel_d_az_rot_caps);
    rot_register(&prosistel_d_el_rot_caps);
    rot_register(&prosistel_combi_track_azel_rot_caps);
    rot_register(&prosistel_d_el_cboxaz_rot_caps);

    return RIG_OK;
}
