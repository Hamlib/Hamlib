/*
 *  Hamlib Dummy backend - main file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: rot_dummy.c,v 1.1 2001-12-28 20:29:33 fillods Exp $
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

#include "rot_dummy.h"

struct dummy_rot_priv_data {
		azimuth_t az;
		elevation_t el;
};



static int dummy_rot_init(ROT *rot)
{
  struct dummy_rot_priv_data *priv;

  priv = (struct dummy_rot_priv_data*)
		  malloc(sizeof(struct dummy_rot_priv_data));

  if (!priv)
		  return -RIG_ENOMEM;
  rot->state.priv = (void*)priv;

  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");
  rot->state.rotport.type.rig = RIG_PORT_NONE;

  priv->az = priv->el = 0;

  return RIG_OK;
}

static int dummy_rot_cleanup(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  if (rot->state.priv)
  	free(rot->state.priv);

  rot->state.priv = NULL;

  return RIG_OK;
}

static int dummy_rot_open(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_rot_close(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
  struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)rot->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called: %f %f\n", 
 			az, el);
  priv->az = az;
  priv->el = el;

  return RIG_OK;
}


static int dummy_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
  struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)rot->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  *az = priv->az;
  *el = priv->el;

  return RIG_OK;
}


static int dummy_rot_stop(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_rot_park(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_rot_reset(ROT *rot, rot_reset_t reset)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static const char *dummy_rot_get_info(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return "";
}



/*
 * Dummy rotator capabilities.
 */

const struct rot_caps dummy_rot_caps = {
  rot_model:     ROT_MODEL_DUMMY,
  model_name:    "Dummy",
  mfg_name:      "Hamlib",
  version:       "0.1",
  copyright:	 "LGPL",
  status:        RIG_STATUS_ALPHA,
  rot_type:      ROT_TYPE_OTHER,
  port_type:     RIG_PORT_NONE,

  min_az: 	-180.,
  max_az: 	180.,
  min_el: 	0.,
  max_el: 	90.,

  priv: NULL,	/* priv */

  rot_init:    dummy_rot_init,
  rot_cleanup: dummy_rot_cleanup,
  rot_open:    dummy_rot_open,
  rot_close:   dummy_rot_close,

  set_position:    dummy_rot_set_position,
  get_position:    dummy_rot_get_position,
  park:    dummy_rot_park,
  stop:    dummy_rot_stop,
  reset:   dummy_rot_reset,
  
  get_info:     dummy_rot_get_info,
};

int initrots_dummy(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "dummy: _init called\n");

	rot_register(&dummy_rot_caps);

	return RIG_OK;
}
