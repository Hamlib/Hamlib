/*
 *  Hamlib Rotator backend - Easycom
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *
 *		$Id: easycomm.c,v 1.1 2002-01-16 16:35:22 fgretief Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#include <hamlib/rotator.h>
#include <serial.h>
#include <misc.h>

#include "easycomm.h"

#define USE_CUSTOM_CODE 1

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

	rs = &rot->state;
	retval = write_block(&rs->rotport, cmdstr, strlen(cmdstr));
	if (retval != RIG_OK) {
		return retval;
	}

    if (data == NULL || data_len <= 0)
        return RIG_OK;  /* don't want a reply */

    retval = read_string(&rs->rotport, data, data_len, "\n");
    if (retval < 0)
        return retval;  /* error */

    /* TODO: Error checking */

    return RIG_OK;
}

/* ************************************************************************* */

static int
easycomm_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    unsigned char cmdstr[64], ackstr[64];
    int retval;
	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called: %f %f\n", az, el);

    sprintf(cmdstr, "AZ%.1f EL%.1f UP000 XXX DN000 XXX\n", az, el);
    retval = easycomm_transaction(rot, cmdstr, ackstr, sizeof(ackstr));
	if (retval != RIG_OK) {
		return retval;
	}
    /* TODO: Error processing */
	return RIG_OK;
}

static int
easycomm_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    unsigned char cmdstr[16], ackbuf[32];
    int retval;
    int t;
	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

#ifdef USE_CUSTOM_CODE
    sprintf(cmdstr, "!");   /* Custom implementation: Remove later */
#else
    sprintf(cmdstr, "AZ EL \n");
#endif

    retval = easycomm_transaction(rot, cmdstr, ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK) {
		return retval;
	}

    /* Parse parse string to extract AZ,EL values */
#ifdef USE_CUSTOM_CODE
    retval = sscanf(ackbuf, "TM%i AZ%f EL%f", &t, az, el);
    if (retval != 3) {
        rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unknown replay (%s)\n", ackbuf);
        return -RIG_ERJCTED;
    }
#ifndef USETESTCODE
    /* Correct for desimal point. */
    *az /= 10.0;
    *el /= 10.0;
#else
    /* Debugging code, remove later */

    rig_debug(RIG_DEBUG_TRACE, "  (az, el) = (%.1f, %.1f)\n", *az, *el);

    *az /= 10.0;
    *el /= 10.0;

    rig_debug(RIG_DEBUG_TRACE, "  (az, el) = (%f, %f)\n", *az, *el);
    rig_debug(RIG_DEBUG_TRACE, "  (az, el) = (%.2f, %.2f)\n", *az, *el);

    /* Note: For some reason I found that the result of this expression
     * does not give accurate results.
     * The first printf give the correct value.
     * The second printf give incorrect value, while
     * the third printf give correct values.
     *
     * I get  az = 42.79999 when it should be 42.8
     */
#endif

#else
    *az = 45.3;
    *el = 10.3;
#endif
	return RIG_OK;
}

static int
easycomm_rot_stop(ROT *rot)
{
    unsigned char ackbuf[32];
    int retval;
	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

    retval = easycomm_transaction(rot, "SA SE \n", ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK)
		return retval;

	/* TODO: error processing */

	return RIG_OK;
}

static int
easycomm_rot_reset(ROT *rot, rot_reset_t rst)
{
    unsigned char ackbuf[32];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

    retval = easycomm_transaction(rot, "RESET\n", ackbuf, sizeof(ackbuf));
    if (retval != RIG_OK)   /* Custom command (not in Easycomm) */
		return retval;

	return RIG_OK;
}

static int
easycomm_rot_park(ROT *rot)
{
    unsigned char ackbuf[32];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

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
    rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

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
        rig_debug(RIG_DEBUG_ERR,__FUNCTION__": Invalid direction value! (%d)\n", direction);
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
  rot_model:     ROT_MODEL_EASYCOMM1,
  model_name:    "EasycommI",
  mfg_name:      "Hamlib",
  version:       "0.1",
  copyright:	 "LGPL",
  status:        RIG_STATUS_ALPHA,
  rot_type:      ROT_TYPE_OTHER,
  port_type:     RIG_PORT_SERIAL,
  serial_rate_min: 9600,
  serial_rate_max: 19200,
  serial_data_bits: 8,
  serial_stop_bits: 1,
  serial_parity: RIG_PARITY_NONE,
  serial_handshake: RIG_HANDSHAKE_NONE,
  write_delay: 0,
  post_write_delay: 0,
  timeout: 200,
  retry: 3,

  min_az:	-180.0,
  max_az: 	180.0,
  min_el:	0.0,
  max_el: 	90.0,

  priv: NULL,	/* priv */

  set_position: easycomm_rot_set_position,
  stop:	easycomm_rot_stop,
};

/* EasycommII implement most of the functions. Again the radio tags
 * is only dummy values.
 */
const struct rot_caps easycomm2_rot_caps = {
  rot_model:     ROT_MODEL_EASYCOMM2,
  model_name:    "EasycommII",
  mfg_name:      "Hamlib",
  version:       "0.1",
  copyright:	 "LGPL",
  status:        RIG_STATUS_ALPHA,
  rot_type:      ROT_TYPE_OTHER,
  port_type:     RIG_PORT_SERIAL,
  serial_rate_min: 9600,
  serial_rate_max: 19200,
  serial_data_bits: 8,
  serial_stop_bits: 1,
  serial_parity: RIG_PARITY_NONE,
  serial_handshake: RIG_HANDSHAKE_NONE,
  write_delay: 0,
  post_write_delay: 0,
  timeout: 200,
  retry: 3,

  min_az:	-180.0,
  max_az: 	180.0,
  min_el:	0.0,
  max_el: 	90.0,

  priv: NULL,	/* priv */

  rot_init: NULL,
  rot_cleanup: NULL,
  rot_open: NULL,
  rot_close: NULL,

  get_position: easycomm_rot_get_position,
  set_position: easycomm_rot_set_position,
  stop:	easycomm_rot_stop,
  park: easycomm_rot_park,
  reset: easycomm_rot_reset,
  move: easycomm_rot_move,

  get_info: NULL,
};

/* ************************************************************************* */

int initrots_easycomm(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, __FUNCTION__" called\n");

    rot_register(&easycomm1_rot_caps);
    rot_register(&easycomm2_rot_caps);

	return RIG_OK;
}

/* ************************************************************************* */
/* end of file */

