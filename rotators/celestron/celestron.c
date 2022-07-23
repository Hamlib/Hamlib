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

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "celestron.h"

#define ACK "#"

#define BUFSZ 128

/**
 * celestron_transaction
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
static int
celestron_transaction(ROT *rot, const char *cmdstr,
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

    /* the answer */
    memset(data, 0, data_len);
    retval = read_string(&rs->rotport, (unsigned char *) data, data_len,
                         ACK, strlen(ACK), 0, 1);

    if (retval < 0)
    {
        if (retry_read++ < rot->state.rotport.retry)
        {
            goto transaction_write;
        }

        goto transaction_quit;
    }

    /* check for acknowledge */
    if (retval < 1 || data[retval - 1] != '#')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected response, len %d: '%s'\n", __func__,
                  retval, data);
        return -RIG_EPROTO;
    }

    data[retval - 1] = '\0';

    retval = RIG_OK;
transaction_quit:
    return retval;
}


static int
celestron_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[32];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    /*
      Note: if the telescope has not been aligned, the RA/DEC values will not be meaningful and the AZM-ALT values will
          be relative to where the telescope was powered on. After alignment, RA/DEC values will reflect the actual sky,
          azimuth will be indexed to North equals 0 and altitude will be indexed with 0 equal to the orientation where the optical
          tube is perpendicular to the azimuth axis.
     */

    SNPRINTF(cmdstr, sizeof(cmdstr), "B%04X,%04X",
             (unsigned)((az / 360.) * 65535),
             (unsigned)((el / 360.) * 65535));

    retval = celestron_transaction(rot, cmdstr, NULL, 0);

    return retval;
}

static int
celestron_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char posbuf[32];
    int retval;
    unsigned w;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* Get Azm-Alt */
    retval = celestron_transaction(rot, "Z", posbuf, sizeof(posbuf));

    if (retval != RIG_OK || strlen(posbuf) < 9 || posbuf[4] != ',')
    {
        return retval < 0 ? retval : -RIG_EPROTO;
    }

    if (sscanf(posbuf, "%04X", &w) != 1)
    {
        return -RIG_EPROTO;
    }

    *az = ((azimuth_t)w * 360.) / 65536.;

    if (sscanf(posbuf + 5, "%04X", &w) != 1)
    {
        return -RIG_EPROTO;
    }

    *el = ((elevation_t)w * 360.) / 65536.;

    rig_debug(RIG_DEBUG_TRACE, "%s: (az, el) = (%.1f, %.1f)\n",
              __func__, *az, *el);

    return RIG_OK;
}

static int
celestron_stop(ROT *rot)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* Cancel Goto */
    retval = celestron_transaction(rot, "M", NULL, 0);

    return retval;
}

static const char *
celestron_get_info(ROT *rot)
{
    static char info[32];
    char str[8];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (celestron_transaction(rot, "V", str, sizeof(str)) != RIG_OK)
    {
        return NULL;
    }

    SNPRINTF(info, sizeof(info), "V%c.%c", str[0], str[1]);

    return info;
}



/* ************************************************************************* */
/*
 * Celestron Nexstar telescope(rotator) capabilities.
 *
 * Protocol documentation:
 *  from Celestron:
 *    http://www.celestron.com/c3/images/files/downloads/1154108406_nexstarcommprot.pdf
 *  from Orion Teletrack Az-G:
 *    http://content.telescope.com/rsc/img/catalog/product/instructions/29295.pdf
 */

const struct rot_caps nexstar_rot_caps =
{
    ROT_MODEL(ROT_MODEL_NEXSTAR),
    .model_name =     "NexStar",  // Any Celestron starting with version 1.2
    .mfg_name =       "Celestron",
    .version =        "20220109.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
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
    .timeout          = 3500, /* worst case scenario */
    .retry            = 1,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .get_position = celestron_get_position,
    .set_position = celestron_set_position,
    .stop         = celestron_stop,
    .get_info     = celestron_get_info,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(celestron)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&nexstar_rot_caps);

    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */

