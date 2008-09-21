/*
 *  Hamlib Netrotctl backend - main file
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *	$Id: netrotctl.c,v 1.1 2008-09-21 19:34:15 fillods Exp $
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
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <errno.h>

#include "hamlib/rotator.h"
#include "iofunc.h"
#include "misc.h"

#include "rot_dummy.h"

#define CMD_MAX 32
#define BUF_MAX 64
#define ROTCTL_ERROR "ERROR "

static int netrotctl_open(ROT *rot)
{
  int ret, len;
  struct rot_state *rs = &rot->state;
  rot_model_t model;
  int prot_ver;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);


  len = sprintf(cmd, "\\dump_state\n");

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return -RIG_EIO;
  }

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  if (!memcmp(buf, ROTCTL_ERROR, strlen(ROTCTL_ERROR)))
	return atoi(buf+strlen(ROTCTL_ERROR));
  prot_ver = atoi(buf);
#define ROTCTLD_PROT_VER 0
  if (prot_ver < ROTCTLD_PROT_VER)
	  return -RIG_EPROTO;

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  model = atoi(buf);

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  rs->min_az = atof(buf);

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  rs->max_az = atof(buf);

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  rs->min_el = atof(buf);

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  rs->max_el = atof(buf);

  return RIG_OK;
}

static int netrotctl_close(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}

static int netrotctl_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
  int ret, len;
  char cmd[CMD_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %f %f\n", __FUNCTION__, 
 			az, el);

  len = sprintf(cmd, "P %f %f\n", az, el);

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return -RIG_EIO;
  }

  return RIG_OK;
}

static int netrotctl_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "p\n");

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return -RIG_EIO;
  }

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  if (!memcmp(buf, ROTCTL_ERROR, strlen(ROTCTL_ERROR)))
           return atoi(buf+strlen(ROTCTL_ERROR));

  *az = atof(buf);

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }
  *el = atof(buf);

  /* read dummy END */
  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return -RIG_EIO;
  }

  return RIG_OK;
}


static int netrotctl_stop(ROT *rot)
{
  int ret, len;
  char cmd[CMD_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "S\n");

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return -RIG_EIO;
  }

  return RIG_OK;
}


static int netrotctl_park(ROT *rot)
{
  int ret, len;
  char cmd[CMD_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "K\n");

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return -RIG_EIO;
  }

  return RIG_OK;
}

static int netrotctl_reset(ROT *rot, rot_reset_t reset)
{
  int ret, len;
  char cmd[CMD_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "R %d\n", reset);

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return -RIG_EIO;
  }

  return RIG_OK;
}

static int netrotctl_move(ROT *rot, int direction, int speed)
{
  int ret, len;
  char cmd[CMD_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "M %d %d\n", direction, speed);

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return -RIG_EIO;
  }

  return RIG_OK;
}

static const char *netrotctl_get_info(ROT *rot)
{
  int ret, len;
  char cmd[CMD_MAX];
  static char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "_\n");

  ret = write(rot->state.rotport.fd, cmd, len);
  if (ret != len) {
  	rig_debug(RIG_DEBUG_ERR,"%s: write failed: %s\n", __FUNCTION__, 
 			strerror(errno));
	return NULL;
  }

  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret < 0) {
	return NULL;
  }
  if (!memcmp(buf, ROTCTL_ERROR, strlen(ROTCTL_ERROR)))
           return NULL;

  buf [ret] = '\0';

  /* read dummy END */
  ret = read_string(&rot->state.rotport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0) {
	return NULL;
  }
  return buf;
}



/*
 * NET rotctl capabilities.
 */

const struct rot_caps netrotctl_caps = {
  .rot_model =      ROT_MODEL_NETROTCTL,
  .model_name =     "NET rotctl",
  .mfg_name =       "Hamlib",
  .version =        "0.1",
  .copyright = 	    "LGPL",
  .status =         RIG_STATUS_ALPHA,
  .rot_type =       ROT_TYPE_OTHER,
  .port_type =      RIG_PORT_NETWORK,
  .timeout = 2000,
  .retry =   3,

  .min_az =  	-180.,
  .max_az =  	180.,
  .min_el =  	0.,
  .max_el =  	90.,

  .priv =  NULL,	/* priv */

  /* .rot_init =     netrotctl_init, */
  /* .rot_cleanup =  netrotctl_cleanup, */
  .rot_open =     netrotctl_open,
  .rot_close =    netrotctl_close,

  .set_position =     netrotctl_set_position,
  .get_position =     netrotctl_get_position,
  .park =     netrotctl_park,
  .stop =     netrotctl_stop,
  .reset =    netrotctl_reset,
  .move =     netrotctl_move,

  .get_info =      netrotctl_get_info,
};

