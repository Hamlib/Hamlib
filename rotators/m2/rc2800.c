/*
 *  Hamlib Rotator backend - M2 RC2800
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
#include <ctype.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "num_stdio.h"

#include "rc2800.h"

#define CR "\r"
#define LF "\x0a"

#define BUFSZ 128

/*
  The continuous output of some of the RC2800
  models can be a nuisance.  Even if we flush
  the input stream before sending the command,
  there may be partial sentence in the stream
  ahead of the data we want.

*/


/*
  rc2800_parse

  Parse output from the rotator controller

  We want to recognize the following sentences:
  A ERR=<n><cr>
  E ERR=<n><cr>
  A P=<ff.f> S=<n> <s><cr>
  E P=<ff.f> S=<n> <s><cr>
  A=<ff.f> S=<n> <s><cr>
  E=<ff.f> S=<n> <s><cr>
*/

static int rc2800_parse(char *s, char *device, float *value)
{
    int msgtype = 0, errcode = 0;
    int len;

    rig_debug(RIG_DEBUG_TRACE, "%s: device return->%s", __func__, s);

    len = strlen(s);

    if (len == 0)
    {
        return -RIG_EPROTO;
    }

    if (len > 7)
    {
        if (*s == 'A' || *s == 'E')
        {
            int i;
            *device = *s;

            if (!strncmp(s + 2, "ERR=", 4))
            {
                msgtype = 1;
                i = sscanf(s + 6, "%d", &errcode);

                if (i == EOF)
                {
                    return -RIG_EINVAL;
                }
            }
            else if (!strncmp(s + 2, "P=", 2))
            {
                msgtype = 2;
                i = num_sscanf(s + 5, "%f", value);

                if (i == EOF)
                {
                    return -RIG_EINVAL;
                }
            }
            else if (s[1] == '=')
            {
                msgtype = 2;
                i = num_sscanf(s + 2, "%f", value);

                if (i == EOF)
                {
                    return -RIG_EINVAL;
                }
            }
        }
    }

    if (msgtype == 2)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: device=%c value=%3.1f\n", __func__, *device,
                  *value);
        return RIG_OK;
    }
    else if (msgtype == 1)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: driver error code %d\n", __func__, errcode);
        *device = ' ';
        return RIG_OK;
    }

    return -RIG_EPROTO;
}


#if 0
int testmain()
{
    rc2800_parse("A P=  98.1 S=9 MV");
    rc2800_parse("A P= 100.0 S=9 MV");
    rc2800_parse("E=43.7 S=9 M");
    rc2800_parse("E=42.8 S=9 S");
    rc2800_parse("E ERR=05");
    return 0;
}
#endif




/**
 * rc2800_transaction
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
rc2800_transaction(ROT *rot, const char *cmdstr,
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

    /* then comes the answer */
    memset(data, 0, data_len);
    retval = read_string(&rs->rotport, (unsigned char *) data, data_len, CR,
                         strlen(CR), 0, 1);

    // some models seem to echo -- so we'll check and read again if echoed
    if (cmdstr && strcmp(data, cmdstr) == 0)
    {
        memset(data, 0, data_len);
        retval = read_string(&rs->rotport, (unsigned char *) data, data_len, CR,
                             strlen(CR), 0, 1);
    }

    if (retval < 0)
    {
        if (retry_read++ < rot->state.rotport.retry)
        {
            goto transaction_write;
        }

        goto transaction_quit;
    }

    retval = RIG_OK;
transaction_quit:
    return retval;
}


static int
rc2800_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[64];
    int retval1, retval2 = RIG_OK;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    if (rot->caps->rot_model == ROT_MODEL_RC2800_EARLY_AZ)
    {
        // we only do azimuth and this is the old protocol
        // we have to switch modes and then send azimuth
        // an extra CR gives us a response to expect
        num_sprintf(cmdstr, "A\r%.0f\r\r", az);
    }
    else
    {
        // does the new protocol use decimal points?
        // we'll assume no for now
        num_sprintf(cmdstr, "A%0f"CR, az);
    }

    retval1 = rc2800_transaction(rot, cmdstr, NULL, 0);

    if (rot->caps->rot_type == ROT_TYPE_AZIMUTH)
    {
        return retval1;
    }

    /* do not overwhelm the MCU? */
    hl_usleep(200 * 1000);

    if (rot->caps->rot_model == ROT_MODEL_RC2800_EARLY_AZEL)
    {
        // this is the old protocol
        // we have to switch modes and then send azimuth
        // an extra CR gives us a response to expect
        num_sprintf(cmdstr, "E\r%.0f\r\r", el);
    }
    else
    {
        num_sprintf(cmdstr, "E%.0f"CR, el);
    }

    retval2 = rc2800_transaction(rot, cmdstr, NULL, 0);

    if (retval1 == retval2)
    {
        return retval1;
    }

    return (retval1 != RIG_OK ? retval1 : retval2);
}

static int
rc2800_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char posbuf[32];
    int retval;
    char device;
    float value;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    *el = 0;

    retval = rc2800_transaction(rot, "A" CR, posbuf, sizeof(posbuf));

    if (retval != RIG_OK || strlen(posbuf) < 5)
    {
        return retval < 0 ? retval : -RIG_EPROTO;
    }

    if (rc2800_parse(posbuf, &device, &value) == RIG_OK)
    {
        if (device == 'A')
        {
            *az = (azimuth_t) value;
        }
        else
        {
            return -RIG_EPROTO;
        }
    }

    if (rot->caps->rot_model == ROT_MODEL_RC2800)
    {
        retval = rc2800_transaction(rot, "E" CR, posbuf, sizeof(posbuf));

        if (retval != RIG_OK || strlen(posbuf) < 5)
        {
            return retval < 0 ? retval : -RIG_EPROTO;
        }

        if (rc2800_parse(posbuf, &device, &value) == RIG_OK)
        {
            if (device == 'E')
            {
                *el = (elevation_t) value;
            }
            else
            {
                return -RIG_EPROTO;
            }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: (az, el) = (%.1f, %.1f)\n",
                  __func__, *az, *el);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: (az) = (%.1f)\n",
                  __func__, *az);
    }


    return RIG_OK;
}

static int
rc2800_rot_stop(ROT *rot)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* TODO: check each return value (do we care?) */

    /* Stop AZ*/
    retval = rc2800_transaction(rot, "A" CR, NULL, 0); /* select AZ */

    if (retval != RIG_OK) { rig_debug(RIG_DEBUG_VERBOSE, "%s: A command failed?\n", __func__); }

    retval = rc2800_transaction(rot, "S" CR, NULL, 0); /* STOP */

    if (retval != RIG_OK) { rig_debug(RIG_DEBUG_VERBOSE, "%s: az S command failed?\n", __func__); }

    if (rot->caps->rot_type == ROT_TYPE_AZIMUTH)
    {
        return retval;
    }

    /* do not overwhelm the MCU? */
    hl_usleep(200 * 1000);

    /* Stop EL*/
    retval = rc2800_transaction(rot, "E" CR, NULL, 0); /* select EL */

    if (retval != RIG_OK) { rig_debug(RIG_DEBUG_VERBOSE, "%s: E command failed?\n", __func__); }

    retval = rc2800_transaction(rot, "S" CR, NULL, 0); /* STOP */

    if (retval != RIG_OK) { rig_debug(RIG_DEBUG_VERBOSE, "%s: el S command failed?\n", __func__); }

    return retval;
}






/* ************************************************************************* */
/*
 * M2 RC2800 rotator capabilities.
 *
 * Protocol documentation: http://www.confluentdesigns.com/files/PdfFiles/devguide_24.pdf
 */

const struct rot_caps rc2800_rot_caps =
{
    ROT_MODEL(ROT_MODEL_RC2800),
    .model_name =     "RC2800",
    .mfg_name =       "M2",
    .version =        "20210129",
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
    .timeout          = 1000,
    .retry            = 3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .get_position = rc2800_rot_get_position,
    .set_position = rc2800_rot_set_position,
    .stop         = rc2800_rot_stop,
};

// below tested on RC2800P-A
const struct rot_caps rc2800az_rot_caps =
{
    ROT_MODEL(ROT_MODEL_RC2800_EARLY_AZ),
    .model_name =     "RC2800_EARLY_AZ",
    .mfg_name =       "M2",
    .version =        "20201130",
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
    .timeout          = 1000,
    .retry            = 3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .get_position = rc2800_rot_get_position,
    .set_position = rc2800_rot_set_position,
    .stop         = rc2800_rot_stop,
};

const struct rot_caps rc2800azel_rot_caps =
{
    ROT_MODEL(ROT_MODEL_RC2800_EARLY_AZEL),
    .model_name =     "RC2800_EARLY_AZEL",
    .mfg_name =       "M2",
    .version =        "20201130",
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
    .timeout          = 1000,
    .retry            = 3,

    .min_az =     0.0,
    .max_az =     360.0,
    .min_el =     0.0,
    .max_el =     180.0,

    .get_position = rc2800_rot_get_position,
    .set_position = rc2800_rot_set_position,
    .stop         = rc2800_rot_stop,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(m2)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&rc2800_rot_caps);
    rot_register(&rc2800az_rot_caps);
    rot_register(&rc2800azel_rot_caps);

    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */

