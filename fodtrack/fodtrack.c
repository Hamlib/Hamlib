/*
 *  Hamlib Rotator backend - Fodtrack parallel port
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: fodtrack.c,v 1.1 2002-11-28 22:24:10 fillods Exp $
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

/*
 * So far, only supported on Linux platforms
 */
#ifdef HAVE_LINUX_PPDEV_H
#include <linux/parport.h>
#include <linux/ppdev.h>
#endif

#include <hamlib/rotator.h>
#include <serial.h>
#include <misc.h>

#include "fodtrack.h"


/* ************************************************************************* */

/** outputs an direction to the interface */
static int setDirection(int fodtrackfd, unsigned char outputvalue, int direction)
{
  unsigned char outputstatus;

  // set the data bits
  ioctl(fodtrackfd, PPWDATA, &outputvalue);

  // autofd=true --> azimuth otherwhise elevation
  if(direction)
    outputstatus = PARPORT_CONTROL_AUTOFD;
  else
    outputstatus=0;
  ioctl(fodtrackfd, PPWCONTROL, &outputstatus);
  // and now the strobe impulse
  usleep(1);
  if(direction)
    outputstatus = PARPORT_CONTROL_AUTOFD | PARPORT_CONTROL_STROBE;
  else
    outputstatus = PARPORT_CONTROL_STROBE;
  ioctl(fodtrackfd, PPWCONTROL, &outputstatus);
  usleep(1);
  if (direction)
    outputstatus= PARPORT_CONTROL_AUTOFD;
  else
    outputstatus=0;
  ioctl(fodtrackfd, PPWCONTROL, &outputstatus);

  return RIG_OK;
}

static int
fodtrack_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
 int fd;
 int retval;

  rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __FUNCTION__, az, el);

  fd = rot->state.rotport.fd;

  retval = setDirection(fd, el/(float)rot->state.max_el*255.0, 0);
  if (retval != RIG_OK)
	  return retval;

  retval = setDirection(fd, az/(float)rot->state.max_az*255.0, 1);
  if (retval != RIG_OK)
	  return retval;


  return RIG_OK;
}


/* ************************************************************************* */
/*
 * Fodtrack rotator capabilities.
 */

/** Fodtrack implement essentially only the set position function.
 */
const struct rot_caps fodtrack_rot_caps = {
  .rot_model =      ROT_MODEL_FODTRACK,
  .model_name =     "Fodtrack",
  .mfg_name =       "XQ2FOD",
  .version =        "0.1",
  .copyright = 	    "LGPL",
  .status =         RIG_STATUS_NEW,
  .rot_type =       ROT_TYPE_OTHER,
  .port_type =      RIG_PORT_PARALLEL,
  .write_delay =  0,
  .post_write_delay =  0,
  .timeout =  200,
  .retry =  3,

  .min_az = 	0,
  .max_az =  	450,
  .min_el = 	0,
  .max_el =  	180,

  .priv =  NULL,	/* priv */

  .set_position =  fodtrack_set_position,
};


/* ************************************************************************* */

int initrots_fodtrack(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

	rot_register(&fodtrack_rot_caps);

	return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
