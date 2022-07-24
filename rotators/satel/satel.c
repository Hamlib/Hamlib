/*
 *  Hamlib SatEL backend - main file
 *  Copyright (c) 2021 Joshua Lynch
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

#include "hamlib/rig.h"
#include <strings.h>
#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "satel.h"


/**
 * Protocol documentation.
 *
 * Apparently, the system is modeled after this one:
 * “An Inexpensive Az-El Rotator System”
 *   "Dec, 1999, QST article by Jim Koehler, VE5FP
 *
 * '?' - returns 'SatEL\r\n'. a good test to see if there's
 *       connectivity.
 *
 * 'g' - enable motion. nothing happens without this enabled.
 *
 * 'z' - display rotator status. contains current Az/El among other
 *       things. here's an example:
 *
 *       Motion ENABLED
 *       Mode 0 - azimuth break at NORTH
 *       Time: 2001/00/00 00:00:07
 *       Azimuth = 000     Absolute = 000
 *       Elevation = 000
 *
 *       Number of stored positions: 00
 *
 *
 * '*' - reset the rotator controller.
 *
 * 'pAZ EL\r\n' - tell the rotator where to point where AZ is the
 *                integer azimuth and EL is the integer
 *                elevation. e.g. 'p010 045\n'. the controller will
 *                report the current pointing status after the
 *                operation has completed.
 *
 * NOTE: The SatEL system changed a few commands as described in the
 * user's manual. They are not used here. You can find the manual for
 * this rotator here:
 *
 * http://www.codeposse.com/~jlynch/SatEL%20Az-EL.pdf
 *
 */

/**
 * Idiosyncrasies
 *
 * - the controller does zero input checking. you can put it into an
 *   incredibly bad state very easily.
 *
 * - the controller doesn't accept any data whilst moving the
 *   rotators. In fact, you can put the controller into a bad state on
 *   occasion if you try and send it commands while its slewing the
 *   rotators. this means we have a really long read timeout so we can
 *   wait for the rotators to slew around before accepting any more
 *   commands.
 *
 */



#define BUF_SIZE 256


typedef struct satel_stat satel_stat_t;
struct satel_stat
{
    bool   motion_enabled;
    int    mode;
    time_t time;
    int    absolute;
    int    az;
    int    el;
};


static int satel_cmd(ROT *rot, char *cmd, int cmdlen, char *res, int reslen)
{
    int ret;
    struct rot_state *rs;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    rs = &rot->state;

    rig_flush(&rs->rotport);

    ret = write_block(&rs->rotport, (unsigned char *) cmd, cmdlen);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (reslen > 0 && res != NULL)
    {
        ret = read_string(&rs->rotport, (unsigned char *) res, reslen, "\n", 1, 0, 1);

        if (ret < 0)
        {
            return ret;
        }
    }


    return RIG_OK;
}


static int satel_read_status(ROT *rot, satel_stat_t *stat)
{
    char resbuf[BUF_SIZE];
    char *p;
    int ret;
    struct rot_state *rs;


    rs = &rot->state;



    // read motion state
    ret = read_string(&rs->rotport, (unsigned char *) resbuf, BUF_SIZE, "\n", 1, 0,
                      1);

    if (ret < 0)
    {
        return ret;
    }

    stat->motion_enabled = strcmp(resbuf, "Motion ENABLED") == 0 ? true : false;

    // XXX skip mode
    ret = read_string(&rs->rotport, (unsigned char *) resbuf, BUF_SIZE, "\n", 1, 0,
                      1);

    if (ret < 0)
    {
        return ret;
    }

    // XXX skip time
    ret = read_string(&rs->rotport, (unsigned char *) resbuf, BUF_SIZE, "\n", 1, 0,
                      1);

    if (ret < 0)
    {
        return ret;
    }

    // read azimuth line
    ret = read_string(&rs->rotport, (unsigned char *) resbuf, BUF_SIZE, "\n", 1, 0,
                      1);

    if (ret < 0)
    {
        return ret;
    }

    p = resbuf + 10;
    p[3] = '\0';
    stat->az = (int)strtof(p, NULL);

    // read elevation line
    ret = read_string(&rs->rotport, (unsigned char *) resbuf, BUF_SIZE, "\n", 1, 0,
                      1);

    if (ret < 0)
    {
        return ret;
    }

    p = resbuf + 12;
    p[3] = '\0';
    stat->el = (int)strtof(p, NULL);

    // skip blank line
    ret = read_string(&rs->rotport, (unsigned char *) resbuf, BUF_SIZE, "\n", 1, 0,
                      1);

    if (ret < 0)
    {
        return ret;
    }

    // XXX skip stored position count
    ret = read_string(&rs->rotport, (unsigned char *) resbuf, BUF_SIZE, "\n", 1, 0,
                      1);

    if (ret < 0)
    {
        return ret;
    }


    return RIG_OK;
}


static int satel_get_status(ROT *rot, satel_stat_t *stat)
{
    int ret;


    ret = satel_cmd(rot, "z", 1, NULL, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }


    return satel_read_status(rot, stat);
}


static int satel_rot_open(ROT *rot)
{
    char resbuf[BUF_SIZE];
    int ret;



    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    // are we connected?
    ret = satel_cmd(rot, "?", 1, resbuf, BUF_SIZE);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = strncasecmp("SatEL", resbuf, 5);

    if (ret != 0)
    {
        return -RIG_EIO;
    }

    // yep, reset system
    ret = satel_cmd(rot, "*", 1, NULL, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return RIG_OK;
}


static int satel_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdbuf[BUF_SIZE];
    int ret;
    satel_stat_t stat;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);


    ret = satel_get_status(rot, &stat);

    if (ret < 0)
    {
        return ret;
    }

    if (stat.motion_enabled == false)
    {
        ret = satel_cmd(rot, "g", 1, NULL, 0);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    SNPRINTF(cmdbuf, BUF_SIZE, "p%d %d\r\n", (int)az, (int)el);
    ret = satel_cmd(rot, cmdbuf, strlen(cmdbuf), NULL, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // wait-for, read and discard the status message
    ret = satel_read_status(rot, &stat);

    if (ret < 0)
    {
        return ret;
    }


    return RIG_OK;
}


static int satel_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    int ret;
    satel_stat_t stat;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    ret = satel_get_status(rot, &stat);

    if (ret < 0)
    {
        return ret;
    }

    *az = stat.az;
    *el = stat.el;


    return RIG_OK;
}


static int satel_rot_stop(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // send reset command
    return satel_cmd(rot, "*", 1, NULL, 0);
}


static const char *satel_rot_get_info(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "SatEL rotator";
}


/*
 * Satel rotator capabilities.
 */
const struct rot_caps satel_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SATEL),
    .model_name       = "SatEL",
    .mfg_name         = "SatEL",
    .version          = "20210123.0",
    .copyright        = "LGPL",
    .status           = RIG_STATUS_STABLE,
    .rot_type         = ROT_TYPE_AZEL,
    .port_type        = RIG_PORT_SERIAL,
    .serial_rate_max  = 9600,
    .serial_rate_min  = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 0,
    .post_write_delay = 0,
    .timeout          = 60000,
    .retry            = 0,
    .min_az           = 0.,
    .max_az           = 360.,
    .min_el           = 0.,
    .max_el           = 90.,
    .rot_open         = satel_rot_open,
    .get_position     = satel_rot_get_position,
    .set_position     = satel_rot_set_position,
    .stop             = satel_rot_stop,
    .get_info         = satel_rot_get_info,
    .priv             = NULL,   /* priv */
};

DECLARE_INITROT_BACKEND(satel)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&satel_rot_caps);

    return RIG_OK;
}
