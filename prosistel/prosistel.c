/*
 *  Hamlib Prosistel backend
 *  Copyright (c) 2015 by Dario Ventura IZ7CRX
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "num_stdio.h"

#include "prosistel.h"

#define BUFSZ 128
#define CR "\r"
#define STX "\x02"


struct prosistel_rot_priv_data
{
    azimuth_t az;
    elevation_t el;

    struct timeval tv;  /* time last az/el update */
    azimuth_t target_az;
    elevation_t target_el;
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

    serial_flush(&rs->rotport);

    if (cmdstr)
    {
        retval = write_block(&rs->rotport, cmdstr, strlen(cmdstr));

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

    //remember check for STXA,G,R or STXA,?,XXX,R 10 bytes
    retval = read_string(&rs->rotport, data, 20, CR, strlen(CR));

    if (retval < 0)
    {
        if (retry_read++ < rot->state.rotport.retry)
        {
            goto transaction_write;
        }

        goto transaction_quit;
    }

    //check if reply match issued command
    if (data[0] == 0x02 && data[3] == cmdstr[2])
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s Command %c reply received\n", __func__,
                  data[3]);
        retval = RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s Error Command issued: %c doesn't match reply %c\n", __func__, cmdstr[2],
                  data[3]);
        retval = RIG_EIO;
    }

transaction_quit:
    return retval;
}






static int prosistel_rot_open(ROT *rot)
{
    char cmdstr[64];
    int retval;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rot_open(rot);
    //disable CPM mode - CPM stands for Continuous Position Monitor operating mode
    //MCU continuously sends position data when CPM enabled
    num_sprintf(cmdstr, STX"AS"CR);
    retval = prosistel_transaction(rot, cmdstr, NULL, 0);
    return retval;
}



static int prosistel_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{

    char cmdstr[64];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.1f %.1f\n", __func__,
              az, el);

    num_sprintf(cmdstr, STX"AG%04.0f"CR, az*10);
    retval = prosistel_transaction(rot, cmdstr, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * Elevation section
    */
    num_sprintf(cmdstr, STX"EG%04.0f"CR, el*10);
    retval = prosistel_transaction(rot, cmdstr, NULL, 0);
    if(retval!=RIG_OK) {
      return retval;
      }

    return retval;
}


/*
 * Get position of rotor
 */
static int prosistel_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char cmdstr[64];
    char data[20];
    float posval;
    int retval;

    num_sprintf(cmdstr, STX"A?"CR);
    retval = prosistel_transaction(rot, cmdstr, data, sizeof(data));

    if (retval != RIG_OK)
    {
        return retval;
    }

    // Example response  of 100 azimuth
    // 02 41 2c 3f 2c 31 30 30 30 2c 52 0d   .A,?,1000,R.
    int n = sscanf(data, "%*cA,?,%f,%*c.", &posval);

    if (n != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s failed to parse azimuth '%s'\n", __func__, data);
        return RIG_EPROTO;
    }

    posval /= 10.0;
    rig_debug(RIG_DEBUG_VERBOSE, "%s got position from '%s' converted to %f\n",
              __func__, data, posval);
    *az = (azimuth_t) posval;


    /*
     * Elevation section
    */
    num_sprintf(cmdstr, STX"B?"CR);
    retval = prosistel_transaction(rot, cmdstr, data, sizeof(data));
    // Example response  of 90 elevation
    // 02 42 2c 3f 2c 30 39 30 30 2c 52 0d   .B,?,0900,R.
    n = sscanf(data, "%*cB,?,%f,%*c.", &posval);
    posval /= 10.0;

    if (n != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s failed to parse elevation '%s'\n", __func__, data);
        return RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s got position from '%s' converted to %f\n",
              __func__, data, posval);
    *el = (elevation_t) posval;

    return retval;
}





/*
 * Prosistel rotator capabilities.
 */

const struct rot_caps prosistel_rot_caps =
{
    .rot_model =      ROT_MODEL_PROSISTEL,
    .model_name =     "Prosistel D",
    .mfg_name =       "Prosistel",
    .version =        "0.5",
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
    .max_el =     90.0,

    .rot_open =     prosistel_rot_open,
    .set_position =     prosistel_rot_set_position,
    .get_position =     prosistel_rot_get_position,

};

DECLARE_INITROT_BACKEND(prosistel)
{
    rig_debug(RIG_DEBUG_VERBOSE, "prosistel: _init called\n");

    rot_register(&prosistel_rot_caps);


    return RIG_OK;
}
