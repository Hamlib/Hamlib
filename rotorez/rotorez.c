/*
 * Hamlib backend library for the DCU rotor command set.
 *
 * rotorez.c - (C) Nate Bargmann 2003 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to a Hy-Gain or Yaesu rotor using
 * the Idiom Press Rotor-EZ or RotorCard interface.  It also
 * supports the Hy-Gain DCU-1.
 *
 *
 *    $Id: rotorez.c,v 1.3 2003-01-12 14:59:46 n0nb Exp $
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>             /* Standard library definitions */
#include <stdio.h>              /* Standard input/output definitions */
#include <string.h>             /* String function definitions */
#include <unistd.h>             /* UNIX standard function definitions */
#include <fcntl.h>              /* File control definitions */
#include <errno.h>              /* Error number definitions */
#include <termios.h>            /* POSIX terminal control definitions */
#include <sys/ioctl.h>          /* System IO Control definitions */

#include <hamlib/rotator.h>
#include <serial.h>
#include <misc.h>

#include "rotorez.h"


/*
 * Private data structure
 */
struct rotorez_rot_priv_data {
  azimuth_t az;
};

/*
 * Private helper function prototypes
 */

static int rotorez_send_priv_cmd(ROT *rot, const char *cmd);

/* *************************************
 *
 * Seperate model capabilities
 *
 * *************************************
 */


/*
 * Rotor-EZ enhanced rotor capabilities
 */

const struct rot_caps rotorez_rot_caps = {
  .rot_model =          ROT_MODEL_ROTOREZ,
  .model_name =         "Rotor-EZ",
  .mfg_name =           "Idiom Press",
  .version =            "0.1.0",
  .copyright = 	        "LGPL",
  .status =             RIG_STATUS_NEW,
  .rot_type =           ROT_TYPE_OTHER,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    4800,
  .serial_data_bits =   8,
  .serial_stop_bits =   1,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        0,
  .post_write_delay =   500,
  .timeout =            5000,
  .retry =              3,

  .min_az = 	        0,
  .max_az =  	        360,
  .min_el = 	        0,
  .max_el =  	        0,

  .priv =  NULL,	/* priv */

  .rot_init =           rotorez_rot_init,
  .rot_cleanup =        rotorez_rot_cleanup,
  .set_position =       rotorez_rot_set_position,
  .get_position =       rotorez_rot_get_position,
  .stop =               rotorez_rot_stop,

};


/*
 * RotorCard enhanced rotor capabilities
 */

const struct rot_caps rotorcard_rot_caps = {
  .rot_model =          ROT_MODEL_ROTORCARD,
  .model_name =         "RotorCard",
  .mfg_name =           "Idiom Press",
  .version =            "0.1.0",
  .copyright = 	        "LGPL",
  .status =             RIG_STATUS_NEW,
  .rot_type =           ROT_TYPE_OTHER,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    4800,
  .serial_data_bits =   8,
  .serial_stop_bits =   1,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        0,
  .post_write_delay =   500,
  .timeout =            5000,
  .retry =              3,

  .min_az = 	        0,
  .max_az =  	        360,
  .min_el = 	        0,
  .max_el =  	        0,

  .priv =  NULL,	/* priv */

  .rot_init =           rotorez_rot_init,
  .rot_cleanup =        rotorez_rot_cleanup,
  .set_position =       rotorez_rot_set_position,
  .get_position =       rotorez_rot_get_position,
  .stop =               rotorez_rot_stop,

};


/*
 * Hy-Gain DCU-1/DCU-1X rotor capabilities
 */

const struct rot_caps dcu_rot_caps = {
  .rot_model =          ROT_MODEL_DCU,
  .model_name =         "DCU-1/DCU-1X",
  .mfg_name =           "Hy-Gain",
  .version =            "0.1.0",
  .copyright = 	        "LGPL",
  .status =             RIG_STATUS_NEW,
  .rot_type =           ROT_TYPE_OTHER,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    4800,
  .serial_data_bits =   8,
  .serial_stop_bits =   1,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        0,
  .post_write_delay =   500,
  .timeout =            5000,
  .retry =              3,

  .min_az = 	        0,
  .max_az =  	        360,
  .min_el = 	        0,
  .max_el =  	        0,

  .priv =  NULL,	/* priv */

  .rot_init =           rotorez_rot_init,
  .rot_cleanup =        rotorez_rot_cleanup,
  .set_position =       rotorez_rot_set_position,

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

static int rotorez_rot_init(ROT *rot) {
  struct rotorez_rot_priv_data *priv;

  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n", __func__);

  if (!rot)
    return -RIG_EINVAL;

  priv = (struct rotorez_rot_priv_data*)
    malloc(sizeof(struct rotorez_rot_priv_data));

  if (!priv)
    return -RIG_ENOMEM;
  rot->state.priv = (void*)priv;

  rot->state.rotport.type.rig = RIG_PORT_SERIAL;

  priv->az = 0;

  return RIG_OK;
}

/*
 * Clean up allocated memory structures
 */

static int rotorez_rot_cleanup(ROT *rot) {

  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n", __func__);

  if (!rot)
    return -RIG_EINVAL;

  if (rot->state.priv)
  	free(rot->state.priv);
  rot->state.priv = NULL;

  return RIG_OK;
}


/*
 * Set position
 * DCU protocol supports azimuth only--elevation is ignored
 * Range is converted to an integer string, 0 to 360 degrees
 */

static int rotorez_rot_set_position(ROT *rot, azimuth_t azimuth, elevation_t elevation) {
  unsigned char cmdstr[8];
  unsigned char execstr[5] = "AM1;";
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n", __func__);

  if (!rot)
    return -RIG_EINVAL;

  if (azimuth < 0 || azimuth > 360)
    return -RIG_EINVAL;

  if (azimuth > 359.4999)           /* DCU-1 compatibility */
    azimuth = 0;

  sprintf(cmdstr, "AP1%03.0f;", azimuth);    /* Target bearing */
  err = rotorez_send_priv_cmd(rot, cmdstr);
  if (err != RIG_OK)
    return err;

  err = rotorez_send_priv_cmd(rot, execstr); /* Execute command */
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Get position
 * Returns current azimuth position in whole degrees.
 * Range is an integer, 0 to 359 degrees
 */

static int rotorez_rot_get_position(ROT *rot, azimuth_t *azimuth, elevation_t *elevation) {
  struct rot_state *rs;
  unsigned char cmdstr[5] = "AI1;";
  unsigned char az[4];
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n", __func__);

  if (!rot)
    return -RIG_EINVAL;

  err = rotorez_send_priv_cmd(rot, cmdstr);
  if (err != RIG_OK)
    return err;

  rs = &rot->state;
  err = read_block(&rs->rotport, az, AZ_READ_LEN);
  if (err != AZ_READ_LEN)
    return -RIG_ETRUNC;

  *azimuth = (azimuth_t)atof(az);
  if (*azimuth < 0 || *azimuth > 359)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "rotorez: azimuth = %.1f degrees\n", *azimuth);

  return RIG_OK;
}


/*
 * Stop rotation
 */

static int rotorez_rot_stop(ROT *rot) {
  unsigned char cmdstr[2] = ";";
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n", __func__);

  if (!rot)
    return -RIG_EINVAL;

  err = rotorez_send_priv_cmd(rot, cmdstr);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Send command string to rotor
 */

static int rotorez_send_priv_cmd(ROT *rot, const char *cmdstr) {
  struct rot_state *rs;
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n", __func__);

  if (!rot)
    return -RIG_EINVAL;

  rs = &rot->state;
  err = write_block(&rs->rotport, cmdstr, strlen(cmdstr));
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Initialize backend
 */

int initrots_rotorez(void *be_handle) {
  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n", __func__);

  rot_register(&rotorez_rot_caps);
  rot_register(&rotorcard_rot_caps);
  rot_register(&dcu_rot_caps);

  return RIG_OK;
}


