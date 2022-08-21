/*
 * Hamlib backend library for the DCU rotor command set.
 *
 * rotorez.c - (C) Nate Bargmann 2003,2009,2010 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to a Hy-Gain or Yaesu rotor using
 * the Idiom Press Rotor-EZ or RotorCard interface.  It also
 * supports the Hy-Gain DCU-1, and DF9GR ERC.
 *
 * Rotor-EZ is a trademark of Idiom Press
 * Hy-Gain is a trademark of MFJ Enterprises
 *
 * Tested on a HAM-IV with the Rotor-EZ V1.4S interface installed.
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>          /* Standard library definitions */
#include <string.h>          /* String function definitions */
#include <ctype.h>            /* for isdigit function */

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "iofunc.h"

#include "rotorez.h"


/*
 * API local implementation
 *
 */

static int rotorez_rot_init(ROT *rot);
static int rotorez_rot_cleanup(ROT *rot);

static int rotorez_rot_set_position(ROT *rot, azimuth_t azimuth,
                                    elevation_t elevation);
static int rotorez_rot_get_position(ROT *rot, azimuth_t *azimuth,
                                    elevation_t *elevation);
static int erc_rot_get_position(ROT *rot, azimuth_t *azimuth,
                                elevation_t *elevation);
static int rt21_rot_get_position(ROT *rot, azimuth_t *azimuth,
                                 elevation_t *elevation);
static int rt21_rot_set_position(ROT *rot, azimuth_t azimuth,
                                 elevation_t elevation);

static int rotorez_rot_reset(ROT *rot, rot_reset_t reset);
static int rotorez_rot_stop(ROT *rot);
static int dcu1_rot_stop(ROT *rot);

static int rotorez_rot_set_conf(ROT *rot, token_t token, const char *val);

static const char *rotorez_rot_get_info(ROT *rot);


/*
 * Private data structure
 */
struct rotorez_rot_priv_data
{
    azimuth_t az;
};

/*
 * Private helper function prototypes
 */

static int rotorez_send_priv_cmd(ROT *rot, const char *cmd);
static int rotorez_send_priv_cmd2(ROT *rot, const char *cmd);
static int rotorez_flush_buffer(ROT *rot);

/*
 * local configuration parameters
 */
static const struct confparams rotorez_cfg_params[] =
{
    {
        TOK_ENDPT, "endpt", "Endpoint option", "Endpoint option",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_JAM, "jam", "Jam protection", "Jam protection",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_OVRSHT, "oversht", "Overshoot option", "Overshoot option",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_UNSTICK, "unstick", "Unstick option", "Unstick option",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    { RIG_CONF_END, NULL, }
};

/* *************************************
 *
 * Separate model capabilities
 *
 * *************************************
 */


/*
 * Idiom Press Rotor-EZ enhanced rotor capabilities for
 * Hy-Gain CD45, Ham-II, Ham-III, Ham IV and all TailTwister rotors
 */

const struct rot_caps rotorez_rot_caps =
{
    ROT_MODEL(ROT_MODEL_ROTOREZ),
    .model_name =       "Rotor-EZ",
    .mfg_name =         "Idiom Press",
    .version =          "20220109.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rot_type =         ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 500,
    .timeout =          1500,
    .retry =            2,

    .min_az =           0,
    .max_az =           360,
    .min_el =           0,
    .max_el =           0,

    .priv =             NULL,   /* priv */
    .cfgparams =        rotorez_cfg_params,

    .rot_init =         rotorez_rot_init,
    .rot_cleanup =      rotorez_rot_cleanup,
    .set_position =     rotorez_rot_set_position,
    .get_position =     rotorez_rot_get_position,
    .stop =             rotorez_rot_stop,
    .set_conf =         rotorez_rot_set_conf,
    .get_info =         rotorez_rot_get_info,

};


/*
 * Idiom Press RotorCard enhanced rotor capabilities for
 * Yaesu SDX and DXA series rotors
 */

const struct rot_caps rotorcard_rot_caps =
{
    ROT_MODEL(ROT_MODEL_ROTORCARD),
    .model_name =       "RotorCard",
    .mfg_name =         "Idiom Press",
    .version =          "20100214.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_BETA,
    .rot_type =         ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 500,
    .timeout =          1500,
    .retry =            2,

    .min_az =           0,
    .max_az =           360,
    .min_el =           0,
    .max_el =           0,

    .priv =             NULL,   /* priv */
    .cfgparams =        rotorez_cfg_params,

    .rot_init =         rotorez_rot_init,
    .rot_cleanup =      rotorez_rot_cleanup,
    .set_position =     rotorez_rot_set_position,
    .get_position =     rotorez_rot_get_position,
    .stop =             rotorez_rot_stop,
    .set_conf =         rotorez_rot_set_conf,
    .get_info =         rotorez_rot_get_info,

};


/*
 * Hy-Gain DCU-1/DCU-1X rotor capabilities
 */

const struct rot_caps dcu_rot_caps =
{
    ROT_MODEL(ROT_MODEL_DCU),
    .model_name =       "DCU-1/DCU-1X",
    .mfg_name =         "Hy-Gain",
    .version =          "20100823.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rot_type =         ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 500,
    .timeout =          1500,
    .retry =            2,

    .min_az =           0,
    .max_az =           360,
    .min_el =           0,
    .max_el =           0,

    .priv =             NULL,   /* priv */

    .rot_init =         rotorez_rot_init,
    .rot_cleanup =      rotorez_rot_cleanup,
    .set_position =     rotorez_rot_set_position,
    .stop =             dcu1_rot_stop,
    .reset =            rotorez_rot_reset,
    .get_info =         rotorez_rot_get_info,

};


/*
 * Rotor capabilities for DF9GR ERC
 *
 * TODO: Learn of additional capabilities of the ERC
 */

const struct rot_caps erc_rot_caps =
{
    ROT_MODEL(ROT_MODEL_ERC),
    .model_name =       "ERC",
    .mfg_name =         "DF9GR",
    .version =          "20100823.2",      /* second revision on 23 Aug 2010 */
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rot_type =         ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 500,
    .timeout =          1500,
    .retry =            2,

    .min_az =           0,
    .max_az =           360,
    .min_el =           0,
    .max_el =           0,

    .priv =             NULL,   /* priv */
//  .cfgparams =        rotorez_cfg_params,

    .rot_init =         rotorez_rot_init,
    .rot_cleanup =      rotorez_rot_cleanup,
    .set_position =     rotorez_rot_set_position,
    .get_position =     erc_rot_get_position,
    .stop =             dcu1_rot_stop,
    .reset =            rotorez_rot_reset,
//  .stop =             rotorez_rot_stop,
//  .set_conf =         rotorez_rot_set_conf,
    .get_info =         rotorez_rot_get_info,

};

/*
 * Rotor capabilities for Hy-Gain YRC-1 compatible with DF9GR ERC
 *
 * TODO: Learn of additional capabilities of the ERC
 */

const struct rot_caps yrc1_rot_caps =
{
    ROT_MODEL(ROT_MODEL_YRC1),
    .model_name =       "DCU2/DCU3/YRC-1",
    .mfg_name =         "Hy-Gain",
    .version =          "20100823.2",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rot_type =         ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 500,
    .timeout =          1500,
    .retry =            2,

    .min_az =           0,
    .max_az =           360,
    .min_el =           0,
    .max_el =           0,

    .priv =             NULL,   /* priv */
//  .cfgparams =        rotorez_cfg_params,

    .rot_init =         rotorez_rot_init,
    .rot_cleanup =      rotorez_rot_cleanup,
    .set_position =     rotorez_rot_set_position,
    .get_position =     erc_rot_get_position,
    .stop =             dcu1_rot_stop,
    .reset =            rotorez_rot_reset,
//  .stop =             rotorez_rot_stop,
//  .set_conf =         rotorez_rot_set_conf,
    .get_info =         rotorez_rot_get_info,

};


const struct rot_caps rt21_rot_caps =
{
    ROT_MODEL(ROT_MODEL_RT21),
    .model_name =       "RT-21",
    .mfg_name =     "Green Heron",
    .version =      "20220104.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rot_type =     ROT_TYPE_OTHER,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 500,
    .timeout =      1500,
    .retry =        2,

    .min_az =       0,
    .max_az =       359.9,
    .min_el =       0,
    .max_el =       90,

    .priv =         NULL,   /* priv */
//  .cfgparams =        rotorez_cfg_params,

    .rot_init =     rotorez_rot_init,
    .rot_cleanup =      rotorez_rot_cleanup,
    .set_position =     rt21_rot_set_position,
    .get_position =     rt21_rot_get_position,
    .stop =             rotorez_rot_stop,
//  .set_conf =         rotorez_rot_set_conf,
//  .get_info =         rotorez_rot_get_info,

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

static int rotorez_rot_init(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rot->state.priv = (struct rotorez_rot_priv_data *)
                      calloc(1, sizeof(struct rotorez_rot_priv_data));

    if (!rot->state.priv)
    {
        return -RIG_ENOMEM;
    }

    rot->state.rotport.type.rig = RIG_PORT_SERIAL;

    ((struct rotorez_rot_priv_data *)rot->state.priv)->az = 0;

    return RIG_OK;
}

/*
 * Clean up allocated memory structures
 */

static int rotorez_rot_cleanup(ROT *rot)
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
 * DCU protocol supports azimuth only--elevation is ignored
 * Range is converted to an integer string, 0 to 360 degrees
 */

static int rotorez_rot_set_position(ROT *rot, azimuth_t azimuth,
                                    elevation_t elevation)
{
    char cmdstr[8];
    char execstr[5] = "AM1;";
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (azimuth < 0 || azimuth > 360)
    {
        return -RIG_EINVAL;
    }

    if (azimuth > 359.4999)     /* DCU-1 compatibility */
    {
        azimuth = 0;
    }

    SNPRINTF(cmdstr, sizeof(cmdstr), "AP1%03.0f;",
             azimuth);     /* Target bearing */
    err = rotorez_send_priv_cmd(rot, cmdstr);

    if (err != RIG_OK)
    {
        return err;
    }

    err = rotorez_send_priv_cmd(rot, execstr);  /* Execute command */

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * RT-21 Set position.
 *
 * RT-21 has a custom command to set azimuth to a precision of 1/10 of a
 * degree--"AP1XXX.Y\r;".  XXX must be zero padded.  The '\r' causes the
 * command to be executed immediately (no need to send "AM1;").
 */

static int rt21_rot_set_position(ROT *rot, azimuth_t azimuth,
                                 elevation_t elevation)
{
    char cmdstr[16];
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (azimuth < 0 || azimuth > 360)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdstr, sizeof(cmdstr), "AP1%05.1f\r;",
             azimuth);   /* Total field width of 5 chars */
    err = rotorez_send_priv_cmd(rot, cmdstr);

    if (err != RIG_OK)
    {
        return err;
    }

    if (rot->state.rotport2.pathname[0] != 0)
    {
        SNPRINTF(cmdstr, sizeof(cmdstr), "AP1%05.1f\r;",
                 elevation);    /* Total field width of 5 chars */

        err = rotorez_send_priv_cmd2(rot, cmdstr);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    return RIG_OK;
}


/*
 * Get position
 * Returns current azimuth position in whole degrees.
 * Range returned from Rotor-EZ is an integer, 0 to 359 degrees
 * Elevation is set to 0
 */

static int rotorez_rot_get_position(ROT *rot, azimuth_t *azimuth,
                                    elevation_t *elevation)
{
    struct rot_state *rs;
    char cmdstr[5] = "AI1;";
    char az[5];         /* read azimuth string */
    char *p;
    azimuth_t tmp = 0;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    do
    {
        err = rotorez_send_priv_cmd(rot, cmdstr);

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

        /* The azimuth string should be ';xxx' beginning at offset 0.  If the
         * ';' is not there, it's likely the RotorEZ has received an invalid
         * command and the buffer needs to be flushed.  See
         * rotorez_flush_buffer() definition below for a complete description.
         */
        if (az[0] != ';')
        {
            err = rotorez_flush_buffer(rot);

            if (err == -RIG_EIO)
            {
                return err;
            }
            else
            {
                err = -RIG_EINVAL;
            }
        }
        else if (err == AZ_READ_LEN)
        {
            /* Check if remaining chars are digits if az[0] == ';' */
            for (p = az + 1; p < az + 4; p++)
                if (isdigit((int)*p))
                {
                    continue;
                }
                else
                {
                    err = -RIG_EINVAL;
                }
        }
    }
    while (err == -RIG_EINVAL);

    /*
     * Rotor-EZ returns a four octet string consisting of a ';' followed
     * by three octets containing the rotor's position in degrees.  The
     * semi-colon is ignored when passing the string to atof().
     */
    az[4] = 0x00;               /* NULL terminated string */
    p = az + 1;                 /* advance past leading ';' */
    tmp = (azimuth_t)atof(p);
    rig_debug(RIG_DEBUG_TRACE, "%s: \"%s\" after conversion = %.1f\n",
              __func__, p, tmp);

    if (tmp == 360)
    {
        tmp = 0;
    }
    else if (tmp < 0 || tmp > 359)
    {
        return -RIG_EINVAL;
    }

    *azimuth = tmp;
    *elevation = 0;             /* RotorEZ does not support elevation */
    rig_debug(RIG_DEBUG_TRACE,
              "%s: azimuth = %.1f deg; elevation = %.1f deg\n",
              __func__, *azimuth, *elevation);

    return RIG_OK;
}


/*
 * Get position for ERC
 * Returns current azimuth position in whole degrees.
 * Range returned from ERC is an integer, 0 to 359 degrees
 * Elevation is set to 0
 */

static int erc_rot_get_position(ROT *rot, azimuth_t *azimuth,
                                elevation_t *elevation)
{
    struct rot_state *rs;
    char cmdstr[5] = "AI1;";
    char az[5];         /* read azimuth string */
    char *p;
    azimuth_t tmp = 0;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    do
    {
        err = rotorez_send_priv_cmd(rot, cmdstr);

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
         * The azimuth string returned by the ERC should be 'xxx;'
         * beginning at offset 0.  A new version may implement the
         * Idiom PRess format, hence the test for ';xxx'.
         */

        /* Check if remaining chars are digits if az[3] == ';' */
        if (az[3] == ';')
        {
            for (p = az; p < az + 3; p++)
                if (isdigit((int)*p))
                {
                    continue;
                }
                else
                {
                    err = -RIG_EINVAL;
                }
        }
        else if (az[0] == ';')
        {
            /* Check if remaining chars are digits if az[0] == ';' */
            for (p = az + 1; p < az + 4; p++)
                if (isdigit((int)*p))
                {
                    continue;
                }
                else
                {
                    err = -RIG_EINVAL;
                }
        }
    }
    while (err == -RIG_EINVAL);

    /*
     * Old ERC returns a four octet string consisting of three octets
     * followed by ';' ('xxx;') containing the rotor's position in degrees.
     * A new version will implement Idiom Press format of ';xxx'.
     *
     * We test for either case and pass a string of only digits to atof()
     */
    az[4] = 0x00;           /* NULL terminated string */
    p = az;

    if (*p == ';')
    {
        p++;
    }
    else
    {
        az[3] = 0x00;    /* truncate trailing ';' */
    }

    tmp = (azimuth_t)atof(p);
    rig_debug(RIG_DEBUG_TRACE, "%s: \"%s\" after conversion = %.1f\n",
              __func__, p, tmp);

    if (tmp == 360)
    {
        tmp = 0;
    }
    else if (tmp < 0 || tmp > 359)
    {
        return -RIG_EINVAL;
    }

    *azimuth = tmp;
    *elevation = 0;             /* ERC does not support elevation */
    rig_debug(RIG_DEBUG_TRACE,
              "%s: azimuth = %.1f deg; elevation = %.1f deg\n",
              __func__, *azimuth, *elevation);

    return RIG_OK;
}


/*
 * Get position from Green Heron RT-21 series of controllers Returns
 * current azimuth position in degrees and tenths.  Range returned from
 * RT-21 is a float, 0.0 to 359.9 degrees Elevation is set to 0
 */

static int rt21_rot_get_position(ROT *rot, azimuth_t *azimuth,
                                 elevation_t *elevation)
{
    struct rot_state *rs;
    char az[8];     /* read azimuth string */
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    /* 'BI1' is an RT-21 specific command that queries for a floating
     * point position (to the tenth of a degree).
     */
    err = rotorez_send_priv_cmd(rot, "BI1;");

    if (err != RIG_OK)
    {
        return err;
    }

    rs = &rot->state;

    err = read_string(&rs->rotport, (unsigned char *) az, RT21_AZ_LEN + 1, ";",
                      strlen(";"), 0, 1);

    if (err < 0)    /* read_string returns negative on error. */
    {
        return err;
    }

    /* RT-21 returns a five to six octet string consisting of one to
     * three octets containing the rotor's position in degrees, one
     * octet containing a decimal '.', one octet containing the rotor's
     * position in tenths of a degree, and one octet with the
     * terminating ';' with a leading space as padding--'[xx| ]x.y;'.
     * Seems as though at least five characters are returned and a
     * space is used as a leading pad character if needed.
     */
    if ((isdigit((int)az[0])) || (isspace((int)az[0])))
    {
        azimuth_t tmp = strtof(az, NULL);
        rig_debug(RIG_DEBUG_TRACE, "%s: \"%s\" after conversion = %.1f\n",
                  __func__, az, tmp);

        if (tmp == 360.0)
        {
            tmp = 0;
        }
        else if (tmp < 0.0 || tmp > 359.9)
        {
            return -RIG_EINVAL;
        }

        *azimuth = tmp;
        *elevation = 0.0;   /* RT-21 backend does not support el at this time. */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: azimuth = %.1f deg; elevation = %.1f deg\n",
                  __func__, *azimuth, *elevation);
    }
    else
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * Stop rotation on RotorEZ, reset on DCU-1
 *
 * Sending the ";" string will stop rotation on the RotorEZ and reset the DCU-1
 */

static int rotorez_rot_stop(ROT *rot)
{
    char cmdstr[2] = ";";
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    err = rotorez_send_priv_cmd(rot, cmdstr);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

static int rotorez_rot_reset(ROT *rot, rot_reset_t reset)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return rotorez_rot_stop(rot);
}

/*
 * Stop rotation on DCU-1
 */

static int dcu1_rot_stop(ROT *rot)
{
    char cmdstr[5] = "AS1;";
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    err = rotorez_send_priv_cmd(rot, cmdstr);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * Send configuration character
 *
 * Rotor-EZ interface will act on these commands immediately --
 * no other command or command terminator is needed.  Expects token
 * define in rotorez.h and *val of '1' or '0' (enable/disable).
 */

static int rotorez_rot_set_conf(ROT *rot, token_t token, const char *val)
{
    char cmdstr[2];
    char c;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: token = %d, *val = %c\n", __func__, (int)token,
              *val);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (*val < '0' || *val > '1')
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_ENDPT:          /* Endpoint option */
        if (*val == '1')
        {
            c = 'E';
        }
        else
        {
            c = 'e';
        }

        break;

    case TOK_JAM:              /* Jam protection */
        if (*val == '1')
        {
            c = 'J';
        }
        else
        {
            c = 'j';
        }

        break;

    case TOK_OVRSHT:            /* Overshoot option */
        if (*val == '1')
        {
            c = 'O';
        }
        else
        {
            c = 'o';
        }

        break;

    case TOK_UNSTICK:          /* Unstick option */
        if (*val == '1')
        {
            c = 'S';
        }
        else
        {
            c = 's';
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: c = %c, *val = %c\n", __func__, c, *val);
    SNPRINTF(cmdstr, sizeof(cmdstr), "%c", c);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s, *val = %c\n",
              __func__, cmdstr, *val);

    err = rotorez_send_priv_cmd(rot, cmdstr);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * Get Info
 * returns the model name string
 */
static const char *rotorez_rot_get_info(ROT *rot)
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
 * Send command string to rotor
 */

static int rotorez_send_priv_cmd(ROT *rot, const char *cmdstr)
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

// send command to 2nd rotator port
static int rotorez_send_priv_cmd2(ROT *rot, const char *cmdstr)
{
    struct rot_state *rs;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rs = &rot->state;
    err = write_block(&rs->rotport2, (unsigned char *) cmdstr, strlen(cmdstr));

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * Flush the serial input buffer
 *
 * If the RotorEZ should receive an invalid command, such as an the ';'
 * character while the rotor is not in motion, as sent by the rotorez_rot_stop
 * function or the 'S' command from 'rotctl', it will output the following
 * string, "C2000 IDIOM V1.4S " into the input buffer.  This function flushes
 * the buffer by reading it until a timeout occurs.  Once the timeout occurs,
 * this function returns and the buffer is presumed to be empty.
 */

static int rotorez_flush_buffer(ROT *rot)
{
    struct rot_state *rs;
    char garbage[32];         /* read buffer */
    int err = 0;
    size_t MAX = 31;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rs = &rot->state;

    do
    {
        err = read_block(&rs->rotport, (unsigned char *) garbage, MAX);

        /* Oops!  An IO error was encountered.  Bail out! */
        if (err == -RIG_EIO)
        {
            return -RIG_EIO;
        }
    }
    while (err != -RIG_ETIMEOUT);

    return RIG_OK;
}


/*
 * Initialize backend
 */

DECLARE_INITROT_BACKEND(rotorez)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&rotorez_rot_caps);
    rot_register(&rotorcard_rot_caps);
    rot_register(&dcu_rot_caps);
    rot_register(&erc_rot_caps);
    rot_register(&rt21_rot_caps);
    rot_register(&yrc1_rot_caps);

    return RIG_OK;
}

