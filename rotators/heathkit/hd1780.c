/*
 * Hamlib backend library for the HD 1780 Intellirotor command set.
 *
 * hd1780.c - (C) Nate Bargmann 2003 (n0nb at arrl.net)
 *            (C) Rob Frohne 2008 (kl7na at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to a Heathkit HD 1780 Intellirotor.
 *
 * Heathkit is a trademark of Heath Company
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
#include <stdlib.h>             /* Standard library definitions */
#include <string.h>             /* String function definitions */

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "hd1780.h"


/*
 * Private data structure
 */
struct hd1780_rot_priv_data
{
    azimuth_t az;
};

/*
 * Private helper function prototypes
 */

static int hd1780_send_priv_cmd(ROT *rot, const char *cmd);


/*
 * HD 1780 Intellirotor rotor capabilities
 */

const struct rot_caps hd1780_rot_caps =
{
    ROT_MODEL(ROT_MODEL_HD1780),
    .model_name =         "HD 1780 Intellirotor",
    .mfg_name =           "Heathkit",
    .version =            "20220109.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rot_type =           ROT_TYPE_OTHER,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    300,
    .serial_rate_max =    9600,
    .serial_data_bits =   8,
    .serial_stop_bits =   1,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        10,
    .post_write_delay =   500,
    .timeout =            60000,
    .retry =              0,

    .min_az =             -180,
    .max_az =             180,
    .min_el =             0,
    .max_el =             0,

    .priv =  NULL,    /* priv */

    .rot_init =           hd1780_rot_init,
    .rot_cleanup =        hd1780_rot_cleanup,
    .set_position =       hd1780_rot_set_position,
    .get_position =       hd1780_rot_get_position,
};


/* ************************************
 *
 * API functions
 *
 * ************************************
 */


/*
 * Initialize data structures
 */

static int hd1780_rot_init(ROT *rot)
{
    struct hd1780_rot_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rot->state.priv = (struct hd1780_rot_priv_data *)
                      calloc(1, sizeof(struct hd1780_rot_priv_data));

    if (!rot->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rot->state.priv;

    rot->state.rotport.type.rig = RIG_PORT_SERIAL;

    priv->az = 0;

    return RIG_OK;
}

/*
 * Clean up allocated memory structures
 */

static int hd1780_rot_cleanup(ROT *rot)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (rot->state.priv)
    {
        free(rot->state.priv);
    }

    rot->state.priv = NULL;

    return RIG_OK;
}


/*
 * Set position
 * HD 1780 protocol supports azimuth only--elevation is ignored
 * Range is converted to an integer string, 0 to 360 degrees
 */

static int hd1780_rot_set_position(ROT *rot, azimuth_t azimuth,
                                   elevation_t elevation)
{
    struct rot_state *rs;
    char cmdstr[8];
    char execstr[5] = "\r", ok[3];
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (azimuth < hd1780_rot_caps.min_az || azimuth > hd1780_rot_caps.max_az)
    {
        return -RIG_EINVAL;
    }

    if (azimuth < 0) { azimuth = azimuth + 360; }

    SNPRINTF(cmdstr, sizeof(cmdstr), "%03.0f", azimuth);    /* Target bearing */
    err = hd1780_send_priv_cmd(rot, cmdstr);

    if (err != RIG_OK)
    {
        return err;
    }

    err = hd1780_send_priv_cmd(rot, execstr); /* Execute command */

    if (err != RIG_OK)
    {
        return err;
    }

    /* We need to look for the <CR> +<LF> to signify that everything finished.  The HD 1780
     * sends a <CR> when it is finished rotating.
     */
    rs = &rot->state;
    err = read_block(&rs->rotport, (unsigned char *) ok, 2);

    if (err != 2)
    {
        return -RIG_ETRUNC;
    }

    if ((ok[0] != '\r') || (ok[1] != '\n'))
    {
        return -RIG_ETRUNC;
    }


    return RIG_OK;
}


/*
 * Get position
 * Returns current azimuth position in whole degrees.
 * Range returned from Rotor-EZ is an integer, 0 to 359 degrees
 * Elevation is set to 0
 */

static int hd1780_rot_get_position(ROT *rot, azimuth_t *azimuth,
                                   elevation_t *elevation)
{
    struct rot_state *rs;
    char cmdstr[3] = "b\r";
    char az[7];          /* read azimuth string */
    char *p;
    azimuth_t tmp = 0;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    err = hd1780_send_priv_cmd(rot, cmdstr);

    if (err != RIG_OK)
    {
        return err;
    }

    rs = &rot->state;
    err = read_block(&rs->rotport, (unsigned char *) az, AZ_READ_LEN);

    if (err != AZ_READ_LEN)
    {
        return -RIG_ETRUNC;
    }

    /*
     * HD 1780 returns a four octet string consisting of
     * three octets containing the rotor's position in degrees followed by a
     * space and a <CR> and a <LF>.  The
     * space is ignored when passing the string to atof().
     */
    az[4] = 0x00;                 /* NULL terminated string */
    p = az; /* for hd1780 */
    tmp = (azimuth_t)atof(p);
    rig_debug(RIG_DEBUG_TRACE, "%s: \"%s\" after conversion = %.1f\n",
              __func__, p, tmp);

    if (tmp < 0 || tmp > 359)
    {
        return -RIG_EINVAL;
    }

    *azimuth = tmp;
    *elevation = 0;               /* assume aiming at the horizon */

    rig_debug(RIG_DEBUG_TRACE,
              "%s: azimuth = %.1f deg; elevation = %.1f deg\n",
              __func__, *azimuth, *elevation);

    return RIG_OK;
}



/*
 * Send command string to rotor.
 *
 * To make sure that the rotor is ready to take a command, we send it a <CR> and
 * it will normally reply with a ? <CR> <LF>.  If you don't do this, using the
 * w: command of rotctl it is possible to get it out of sequence.  This kind of
 * resets its command buffer before sending the command.
 */

static int hd1780_send_priv_cmd(ROT *rot, const char *cmdstr)
{
    struct rot_state *rs;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rs = &rot->state;

    err = write_block(&rs->rotport, (unsigned char *) cmdstr, strlen(cmdstr));

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * Initialize backend
 */

DECLARE_INITROT_BACKEND(heathkit)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&hd1780_rot_caps);

    return RIG_OK;
}


