/*
 *  Hamlib Rotator backend - Celestron
 *  Copyright (c) 2011 by Stephane Fillod
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
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "rot_ioptron.h"

#define ACK "#"
#define ACK1 '1'

#define BUFSZ 128

/**
 * ioptron_transaction
 *
 * cmdstr - Command to be sent to the rig.
 * data - Buffer for reply string.  Can be NULL, indicating that no reply is
 *        is needed, but answer will still be read.
 * data_len - in: Size of buffer. It is the caller's responsibily to provide
 *            a large enough buffer for all possible replies for a command.
 *
 *  COMMANDS  note: as of 12/2018 a mixture of V2 and V3
 * | TTTTTTTT(T) .01 arc seconds
 * | alt- sign with 8 digits, az - 9 digits                           |
 * | Command     | Attribute | Return value | Description              |
 * -------------------------------------------------------------------|
 * | :GAC# | .01 arcsec | sTTTTTTTTTTTTTTTTT# | gets alt(s8), az(9)   |
 * | :SzTTTTTTTTT# | .01 arcsec | '1' == OK | Set Target azimuth      |
 * | :SasTTTTTTTT# |.01 arcsec | '1' == OK | Set Target elevation     |
 * | :Q#         | -        | '1' == OK    | Halt all slewing         |
 * | :ST0#       | -        | '1' == OK    | Halt tracking            |
 * | :MS#        | -        | '1' == OK    | GoTo Target              |
 * |
 * returns:
 *   RIG_OK  -  if no error occurred.
 *   RIG_EIO  -  if an I/O error occurred while sending/receiving data.
 *   RIG_ETIMEOUT  -  if timeout expires without any characters received.
 */

static int
ioptron_transaction(ROT *rot, const char *cmdstr,
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

    /** Always read the reply to know whether the cmd went OK */
    if (!data)
    {
        data = replybuf;
    }

    if (!data_len)
    {
        data_len = BUFSZ;
    }

    /** the answer */
    memset(data, 0, data_len);
    retval = read_string(&rs->rotport, (unsigned char *) data, data_len, ACK,
                         strlen(ACK), 0, 1);

    if (retval < 0)
    {
        if (retry_read++ < rot->state.rotport.retry)
        {
            goto transaction_write;
        }

        goto transaction_quit;
    }

    /** check for acknowledge */
    if (retval < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected response, len %d: '%s'\n", __func__,
                  retval, data);
        return -RIG_EPROTO;
    }

    retval = RIG_OK;
transaction_quit:
    return retval;
}

/**
 * Opens the Port and sets all needed parameters for operation
 * as of 12/2018 initiates mount with V3 :MountInfo#
 */
static int ioptron_open(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return ioptron_transaction(rot, ":Mountinfo#", NULL, 0);
}

/** sets mount position, requires 4 steps
 * set azmiuth
 * set altitude
 * goto set
 * stop tracking - mount starts tracking after goto
 */
static int
ioptron_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[32];
    char retbuf[10];
    int retval;
    float faz, fel;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    /* units .01 arc sec */
    faz = az * 360000;
    fel = el * 360000;
    /* set azmiuth, returns '1" if OK */
    SNPRINTF(cmdstr, sizeof(cmdstr), ":Sz%09.0f#", faz);
    retval = ioptron_transaction(rot, cmdstr, retbuf, sizeof(retbuf));

    if (retval != RIG_OK || retbuf[0] != ACK1)
    {
        return  -RIG_EPROTO;
    }

    /* set altitude, returns '1" if OK */
    SNPRINTF(cmdstr, sizeof(cmdstr), ":Sa+%08.0f#", fel);
    retval = ioptron_transaction(rot, cmdstr, retbuf, sizeof(retbuf));

    if (retval != RIG_OK || retbuf[0] != ACK1)
    {
        return  -RIG_EPROTO;
    }

    /* move to set target, V2 command, returns '1" if OK */
    SNPRINTF(cmdstr, sizeof(cmdstr), ":MS#"); //
    retval = ioptron_transaction(rot, cmdstr, retbuf, sizeof(retbuf));

    if (retval != RIG_OK || retbuf[0] != ACK1)
    {
        return  -RIG_EPROTO;
    }

    /* stop tracking, V2 command, returns '1" if OK */
    SNPRINTF(cmdstr, sizeof(cmdstr), ":ST0#");
    retval = ioptron_transaction(rot, cmdstr, retbuf, sizeof(retbuf));

    if (retval != RIG_OK || retbuf[0] != ACK1)
    {
        return  -RIG_EPROTO;
    }

    return retval;
}

/**  gets current position  */
static int
ioptron_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char posbuf[32];
    int retval;
    float w;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /** Get Az-Alt */
    retval = ioptron_transaction(rot, ":GAC#", posbuf, sizeof(posbuf));

    if (retval != RIG_OK || strlen(posbuf) < 18)
    {
        return retval < 0 ? retval : -RIG_EPROTO;
    }

    if (sscanf(posbuf, "%9f", &w) != 1)
    {
        return -RIG_EPROTO;
    }

    /** convert from .01 arc sec to degrees  */
    *el = ((elevation_t)w / 360000.);

    if (sscanf(posbuf + 9, "%9f", &w) != 1)
    {
        return -RIG_EPROTO;
    }

    *az = ((azimuth_t)w / 360000.);

    rig_debug(RIG_DEBUG_TRACE, "%s: (az, el) = (%.1f, %.1f)\n",
              __func__, *az, *el);

    return RIG_OK;
}

/** stop everything  **/
static int
ioptron_stop(ROT *rot)
{
    int retval;
    char retbuf[10];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /** stop slew, returns "1" if OK */
    retval = ioptron_transaction(rot, ":Q#", retbuf, 10);

    if (retval != RIG_OK || retbuf[0] != ACK1)
    {
        return  -RIG_EPROTO;
    }

    /** stops tracking returns "1" if OK */
    retval = ioptron_transaction(rot, ":ST0#", retbuf, 10);

    if (retval != RIG_OK || retbuf[0] != ACK1)
    {
        return  -RIG_EPROTO;
    }

    return retval;
}

/** get mount type code, initializes mount */
static const char *
ioptron_get_info(ROT *rot)
{
    static char info[32];
    char str[6];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    retval = ioptron_transaction(rot, ":MountInfo#", str, sizeof(str));

    rig_debug(RIG_DEBUG_TRACE, "retval, RIG_OK str %d  %d  %str\n", retval, RIG_OK,
              str);

    SNPRINTF(info, sizeof(info), "MountInfo %s", str);

    return info;
}



/** *************************************************************************
 *
 * ioptron mount capabilities.
 *
 * Protocol documentation:
 *  from ioptron:
 *    RS232-Command_Language  pdf
 * note that iOptron is currently (12/2018) using a mix of V2 and V3 commands :(
 */

const struct rot_caps ioptron_rot_caps =
{
    ROT_MODEL(ROT_MODEL_IOPTRON),
    .model_name =     "iOptron",
    .mfg_name =       "iOptron",
    .version =        "20220109.0",
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
    .timeout          = 1000, /* worst case scenario 3500 */
    .retry            = 1,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .rot_open     = ioptron_open,
    .get_position = ioptron_get_position,
    .set_position = ioptron_set_position,
    .stop         = ioptron_stop,
    .get_info     = ioptron_get_info,
};

/* ****************************************************************** */

DECLARE_INITROT_BACKEND(ioptron)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&ioptron_rot_caps);

    return RIG_OK;
}

/* ****************************************************************** */
/* end of file */

