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
 *    $Id: rotorez.c,v 1.1 2003-01-11 00:47:48 n0nb Exp $
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
 * Rotor-EZ enhanced rotor capabilities
 */
const struct rot_caps rotorez_rot_caps = {
  .rot_model =          ROT_MODEL_ROTOREZ,
  .model_name =         "Rotor-EZ",
  .mfg_name =           "Idiom Press",
  .version =            "0.1",
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
  .post_write_delay =   0,
  .timeout =            200,
  .retry =              3,

  .min_az = 	        0,
  .max_az =  	        360,
  .min_el = 	        0,
  .max_el =  	        0,

  .priv =  NULL,	/* priv */

//  .set_position =  fodtrack_set_position,
};


int initrots_rotorez(void *be_handle) {
  rig_debug(RIG_DEBUG_VERBOSE, "rotorez: %s called\n");

  rot_register(&rotorez_rot_caps);

  return RIG_OK;
}

