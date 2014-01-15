/*
 *  Hamlib Rotator backend - Easycom
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
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

#include "hamlib/rotator.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "easycomm.h"

/* ************************************************************************* */
/**
 *  easycomm_transaction
 *
 *  Assumes rot!=NULL and cmdstr!=NULL
 *
 *  cmdstr   - string to send to rotator
 *  data     - buffer for reply string
 *  data_len - (input) Maximum size of buffer
 *             (output) Number of bytes read.
 */
static int
easycomm_transaction (ROT *rot, const char *cmdstr, char *data, size_t data_len)
{
  struct rot_state *rs;
  int retval;

  rig_debug(RIG_DEBUG_TRACE, "%s called: %s\n", __FUNCTION__, cmdstr);

  if (!rot )
    return -RIG_EINVAL;

  rs = &rot->state;
  serial_flush(&rs->rotport);
  retval = write_block(&rs->rotport, cmdstr, strlen(cmdstr));
  if (retval != RIG_OK) {
      goto transaction_quit;
  }

  if (data == NULL || data_len <= 0)
    return RIG_OK;  /* don't want a reply */

  memset(data,0,data_len);
  retval = read_string(&rs->rotport, data, data_len, "\n", 1);
  if (retval < 0) {
    rig_debug(RIG_DEBUG_TRACE, "%s read_string failed with status %d\n", __FUNCTION__, retval);
    goto transaction_quit;
  } else {
    rig_debug(RIG_DEBUG_TRACE, "%s read_string: %s\n", __FUNCTION__, data);
    retval = RIG_OK;
  }

  transaction_quit:
  return retval;
}

/* ************************************************************************* */

static int
easycomm_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    char cmdstr[64];
    int retval;
	rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __FUNCTION__, az, el);

    sprintf(cmdstr, "AZ%.1f EL%.1f UP000 XXX DN000 XXX\n", az, el);
    retval = easycomm_transaction(rot, cmdstr, NULL, 0);
	if (retval != RIG_OK) {
		return retval;
	}
    /* TODO: Error processing */
	return RIG_OK;
}

static int
easycomm_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    char cmdstr[16], ackbuf[32];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    sprintf(cmdstr, "AZ EL \n");

    retval = easycomm_transaction(rot, cmdstr, ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK) {
	  rig_debug(RIG_DEBUG_TRACE, "%s got error: %d\n", __FUNCTION__, retval);
	  return retval;
	}

    /* Parse parse string to extract AZ,EL values */
    rig_debug(RIG_DEBUG_TRACE, "%s got response: %s\n", __FUNCTION__, ackbuf);
    retval = sscanf(ackbuf, "AZ%f EL%f", az, el);
    if (retval != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown response (%s)\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }
    return RIG_OK;
}

static int
easycomm_rot_stop(ROT *rot)
{
    char ackbuf[32];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    retval = easycomm_transaction(rot, "SA SE \n", ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK)
		return retval;

	/* TODO: error processing */

	return RIG_OK;
}

static int
easycomm_rot_reset(ROT *rot, rot_reset_t rst)
{
    char ackbuf[32];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    retval = easycomm_transaction(rot, "RESET\n", ackbuf, sizeof(ackbuf));
    if (retval != RIG_OK)   /* Custom command (not in Easycomm) */
		return retval;

	return RIG_OK;
}

static int
easycomm_rot_park(ROT *rot)
{
    char ackbuf[32];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    retval = easycomm_transaction(rot, "PARK\n", ackbuf, sizeof(ackbuf));
    if (retval != RIG_OK)   /* Custom command (not in Easycomm) */
		return retval;

	return RIG_OK;
}

static int
easycomm_rot_move(ROT *rot, int direction, int speed)
{
    char cmdstr[24], ackbuf[32];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    /* Note: speed is unused at the moment */
    switch (direction) {
    case ROT_MOVE_UP:       /* Elevation increase */
        sprintf(cmdstr, "MU\n");
        break;
    case ROT_MOVE_DOWN:     /* Elevation decrease */
        sprintf(cmdstr, "MD\n");
        break;
    case ROT_MOVE_LEFT:     /* Azimuth decrease */
        sprintf(cmdstr, "ML\n");
        break;
    case ROT_MOVE_RIGHT:    /* Azimuth increase */
        sprintf(cmdstr, "MR\n");
        break;
    default:
        rig_debug(RIG_DEBUG_ERR,"%s: Invalid direction value! (%d)\n", __FUNCTION__, direction);
        return -RIG_EINVAL;
    }

    retval = easycomm_transaction(rot, cmdstr, ackbuf, sizeof(ackbuf));
    if (retval != RIG_OK)
        return retval;

    return RIG_OK;
}

/* ************************************************************************* */
/*
 * Easycomm rotator capabilities.
 */

/** EasycommI implement essentially only the set position function, but
 *  I included the stop command too.  The radio control tags is only included
 *  as dummy entries because the spec require them.
 */
const struct rot_caps easycomm1_rot_caps = {
  .rot_model =      ROT_MODEL_EASYCOMM1,
  .model_name =     "EasycommI",
  .mfg_name =       "Hamlib",
  .version =        "0.3",
  .copyright = 	 "LGPL",
  .status =         RIG_STATUS_BETA,
  .rot_type =       ROT_TYPE_OTHER,
  .port_type =      RIG_PORT_SERIAL,
  .serial_rate_min =  9600,
  .serial_rate_max =  19200,
  .serial_data_bits =  8,
  .serial_stop_bits =  1,
  .serial_parity =  RIG_PARITY_NONE,
  .serial_handshake =  RIG_HANDSHAKE_NONE,
  .write_delay =  0,
  .post_write_delay =  0,
  .timeout =  200,
  .retry =  3,

  .min_az = 	0.0,
  .max_az =  	360.0,
  .min_el = 	0.0,
  .max_el =  	180.0,

  .priv =  NULL,	/* priv */

  .set_position =  easycomm_rot_set_position,
  .stop = 	easycomm_rot_stop,
};

/* EasycommII implement most of the functions. Again the radio tags
 * is only dummy values.
 */
const struct rot_caps easycomm2_rot_caps = {
  .rot_model =      ROT_MODEL_EASYCOMM2,
  .model_name =     "EasycommII",
  .mfg_name =       "Hamlib",
  .version =        "0.3",
  .copyright = 	 "LGPL",
  .status =         RIG_STATUS_BETA,
  .rot_type =       ROT_TYPE_OTHER,
  .port_type =      RIG_PORT_SERIAL,
  .serial_rate_min =  9600,
  .serial_rate_max =  19200,
  .serial_data_bits =  8,
  .serial_stop_bits =  1,
  .serial_parity =  RIG_PARITY_NONE,
  .serial_handshake =  RIG_HANDSHAKE_NONE,
  .write_delay =  0,
  .post_write_delay =  0,
  .timeout =  200,
  .retry =  3,

  .min_az = 	0.0,
  .max_az =  	360.0,
  .min_el = 	0.0,
  .max_el =  	180.0,

  .priv =  NULL,	/* priv */

  .rot_init =  NULL,
  .rot_cleanup =  NULL,
  .rot_open =  NULL,
  .rot_close =  NULL,

  .get_position =  easycomm_rot_get_position,
  .set_position =  easycomm_rot_set_position,
  .stop = 	easycomm_rot_stop,
  .park =  easycomm_rot_park,
  .reset =  easycomm_rot_reset,
  .move =  easycomm_rot_move,

  .get_info =  NULL,
};

/* ************************************************************************* */

DECLARE_INITROT_BACKEND(easycomm)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

    rot_register(&easycomm1_rot_caps);
    rot_register(&easycomm2_rot_caps);

	return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
