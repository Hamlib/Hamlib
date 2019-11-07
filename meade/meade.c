/*
 *  Hamlib Meade telescope rotor backend - main file
 *  Copyright (c) 2018 by Andreas Mueller (DC1MIL)
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

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include <hamlib/rotator.h>
#include <num_stdio.h>

#include "serial.h"
#include "misc.h"
#include "register.h"

#include "meade.h"

struct meade_priv_data {
  azimuth_t az;
  elevation_t el;

  struct timeval tv;	/* time last az/el update */
  azimuth_t target_az;
  elevation_t target_el;
};

/**
 * Command list:
 * See https://www.meade.com/support/LX200CommandSet.pdf
 * and https://www.meade.com/support/TelescopeProtocol_2010-10.pdf for newer
 * Firmware Versions
 *
 * Not the full set of available commands is used, the list here shows
 * only the commands of the telescope used by hamlib
 *
 * All used Commands are supportet by Meade Telescopes with LX-200 protocol
 * (e.g. DS-2000 with Autostar) and should also work with the LX16 and
 * LX200GPS.
 * Tested only with DS-2000 and AutoStar 494 together with Meade 506 i2c to
 * Serial cable. But should also work with other AutoStars and the regular
 * Serial Cable.
 *
 * | Command     | Atribute | Return value | Description              |
 * --------------------------------------------------------------------
 * | :Me#        | -        | -            | Moves telescope east     |
 * | :Mn#        | -        | -            | Moves telescope north    |
 * | :Ms#        | -        | -            | Moves telescope south    |
 * | :Mw#        | -        | -            | Moves telescope west     |
 * | :AL#        | -        | -            | Set to Land mode         |
 * | :Sz DDD*MM# | D,M      | 1' == OK     | Set Target azimuth       |
 * | :SasDD*MM#  | s,D,M    | 1' == OK     | Set Target elevation     |
 * | :Mw#        | -        | -            | Moves telescope west     |
 * | :Q#         | -        | -            | Halt all slewing         |
 * | :SoDD#      | D        | '1' == OK    | Set minimal elevation    |
 * | :ShDD#      | D        | '1' == OK    | Set maximal elevation    |
 * | :MA#        | -        | '0' == OK    | GoTo Target              |
 * | :D#         | -        | 0x7F == YES  | Check if active movement |
 *
 */

/**
 * meade_transaction
 *
 * cmdstr - Command to be sent to the rig.
 * data - Buffer for reply string.  Can be NULL, indicating that no reply is
 *        is needed, but answer will still be read.
 * data_len - in: Size of buffer. It is the caller's responsibily to provide
 *            a large enough buffer for all possible replies for a command.
 *
 * returns:
 *   RIG_OK  -  if no error occurred.
 *   RIG_EIO  -  if an I/O error occurred while sending/receiving data.
 *   RIG_ETIMEOUT  -  if timeout expires without any characters received.
 */
static int meade_transaction (ROT *rot, const char *cmdstr,
                                    char *data, size_t *data_len, size_t expected_return_length)
{
  struct rot_state *rs;
  int return_value;
  int retry_read = 0;

  rs = &rot->state;

  while(1) {
    serial_flush(&rs->rotport);

    if (cmdstr) {
      return_value = write_block(&rs->rotport, cmdstr, strlen(cmdstr));
      if (return_value != RIG_OK) {
        return return_value;
      }
    }

    /* Not all commands will send a return value, so use data = NULL if no
       return value is expected, Strings end with '#' */
    if (data != NULL) {
      memset(data,0,BUFSIZE);
      *data_len = read_string(&rs->rotport, data, expected_return_length + 1, "\n", strlen("\n"));
      if (*data_len < 0) {
        if (retry_read++ >= rot->state.rotport.retry) {
          return RIG_ETIMEOUT;
        }
      }
      else {
        return RIG_OK;
      }
    }
    else {
      return RIG_OK;
    }
  }
}

/*
 * Initialization
 */
static int meade_init(ROT *rot)
{
  struct meade_priv_data *priv;

  priv = (struct meade_priv_data*)
    malloc(sizeof(struct meade_priv_data));

  if (!priv)
    return -RIG_ENOMEM;
  rot->state.priv = (void*)priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
  rot->state.rotport.type.rig = RIG_PORT_SERIAL;

  priv->az = priv->el = 0;

  priv->target_az = priv->target_el = 0;

  return RIG_OK;
}

/*
 * Cleanup
 */
static int meade_cleanup(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (rot->state.priv)
    free(rot->state.priv);

  rot->state.priv = NULL;

  return RIG_OK;
}

/*
 * Opens the Port and sets all needed parametes for operation
 */
static int meade_open(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  /* Set Telescope to Land alignment mode to deactivate sloping */
  /* Allow 0-90 Degree Elevation */
  return meade_transaction(rot, ":AL#:So00#:Sh90#" , NULL, 0, 0);
}

/*
 * Closes the port and stops all movement
 */
static int meade_close(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
  /* Stop all Movement */
  return meade_transaction(rot, ":Q#" , NULL, 0, 0);
}

/*
 * Sets the target position and starts movement
 *
 * az: Target azimuth
 * el: Target elevation
 */
static int meade_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
  struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;
  char cmd_str[BUFSIZE];
  char return_str[BUFSIZE];
  size_t return_str_size;
  float az_degrees, az_minutes, el_degrees, el_minutes;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %.2f %.2f\n", __func__,
    az, el);

  az_degrees = floor(az);
  az_minutes = (az - az_degrees) * 60;
  el_degrees = floor(el);
  el_minutes = (el - el_degrees) * 60;

  /* Check if there is an active movement, if yes, stop it if
     new target is more than 5 Degrees away from old target
     if not, don't accept new target*/
  meade_transaction(rot, ":D#", return_str, &return_str_size, 1);
  if(return_str_size > 0 && return_str[0] == 0x7F) {
    if(fabsf(az - priv->target_az) > 5 || fabsf(el - priv->target_el) > 5)
      meade_transaction(rot, ":Q#", NULL, 0, 0);
    else
      return RIG_OK;
  }

  priv->target_az = az;
  priv->target_el = el;

  num_sprintf(cmd_str, ":Sz %03.0f*%02.0f#:Sa+%02.0f*%02.0f#:MA#",
              az_degrees, az_minutes, el_degrees, el_minutes);

  meade_transaction(rot, cmd_str, return_str, &return_str_size, 3);
  /* '1' == Azimuth accepted '1' == Elevation accepted '0' == No error */
  if(return_str_size > 0 && strstr(return_str , "110") != NULL)
    return RIG_OK;
  else
    return RIG_EINVAL;
}

/*
 * Get position of rotor, simulating slow rotation
 */
static int meade_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
  char return_str[BUFSIZE];
  size_t return_str_size;
  int az_degree, az_minutes, el_degree, el_minutes;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  meade_transaction(rot, ":GZ#:GA#", return_str, &return_str_size, BUFSIZE);
  // answer expecting one of two formats depending on precision setting
  // The Meade manual is not clear on this format
  // The period separator is coming back as 0xdf so we won't assume which char it is
  // DDD.MM:SS#sDD.MM:SS#
  // DDD.MM#sDD.MM#
  // Tested and working with the Meade LX200 and Autostar 497
  rig_debug(RIG_DEBUG_VERBOSE, "%s: parsing \"%s\" as high precision\n", __func__, return_str);
  int n = sscanf(return_str,"%d%*c%d:%*d#%d%*c%d:%*d#",&az_degree,&az_minutes,&el_degree,&el_minutes);
  if (n != 4) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s: parsing as low precision\n", __func__);
    n = sscanf(return_str,"%d%*c%d#%d%*c%d",&az_degree,&az_minutes,&el_degree,&el_minutes);
    if (n != 4) {
      return RIG_EPROTO;
    }
  }
  *az = dmmm2dec(az_degree, az_minutes, 0);
  *el = dmmm2dec(el_degree, el_minutes, 0);
  return RIG_OK;
}

/*
 * Stops all movement
 */
static int meade_stop(ROT *rot)
{
  struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;
  azimuth_t az;
  elevation_t el;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  meade_transaction(rot, ":Q#", NULL, 0, 0);
  meade_get_position(rot, &az, &el);

  priv->target_az = priv->az = az;
  priv->target_el = priv->el = el;

  return RIG_OK;
}

/*
 * Moves to Home Position
 */
static int meade_park(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  /* Assume home is 0,0 */
  meade_set_position(rot, 0, 0);

  return RIG_OK;
}

/*
 * Reset: Nothing to do exept parking
 */
static int meade_reset(ROT *rot, rot_reset_t reset)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
  meade_park(rot);

  return RIG_OK;
}

/*
 * Movement to direction
 */
static int meade_move(ROT *rot, int direction, int speed)
{
  struct meade_priv_data *priv = (struct meade_priv_data *)rot->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
  rig_debug(RIG_DEBUG_TRACE, "%s: Direction = %d, Speed = %d\n", __func__, direction, speed);

  switch(direction) {
  case ROT_MOVE_UP:
    return meade_set_position(rot, priv->target_az, 90);

  case ROT_MOVE_DOWN:
    return meade_set_position(rot, priv->target_az, 0);

  case ROT_MOVE_CCW:
    return meade_set_position(rot, -180, priv->target_el);

  case ROT_MOVE_CW:
    return meade_set_position(rot, 180, priv->target_el);

  default:
    return -RIG_EINVAL;
  }

  return RIG_OK;
}

static const char *meade_get_info(ROT *rot)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  return "Meade telescope rotator with LX200 protocol.";
}

/*
 * Meade telescope rotator capabilities.
 */

const struct rot_caps meade_caps = {
  .rot_model =        ROT_MODEL_MEADE,
  .model_name =       "LX200",
  .mfg_name =         "Meade",
  .version =          "0.2",
  .copyright =        "LGPL",
  .status =           RIG_STATUS_ALPHA,
  .rot_type =         ROT_TYPE_AZEL,

  .port_type =        RIG_PORT_SERIAL,
  .serial_rate_min =  9600,
  .serial_rate_max =  9600,
  .serial_data_bits = 8,
  .serial_stop_bits = 1,
  .serial_parity =    RIG_PARITY_NONE,
  .serial_handshake = RIG_HANDSHAKE_NONE,
  .write_delay =      0,
  .post_write_delay = 200,
  .timeout =          400,
  .retry =            5,

  .min_az =      0.,
  .max_az =    360.,
  .min_el =      0.,
  .max_el =     90.,

  .priv =      NULL,  /* priv */

  .rot_init =         meade_init,
  .rot_cleanup =      meade_cleanup,
  .rot_open =         meade_open,
  .rot_close =        meade_close,

  .set_position =     meade_set_position,
  .get_position =     meade_get_position,
  .park =             meade_park,
  .stop =             meade_stop,
  .reset =            meade_reset,
  .move =             meade_move,

  .get_info =         meade_get_info,
};

DECLARE_INITROT_BACKEND(meade)
{
  rig_debug(RIG_DEBUG_VERBOSE, "meade: _init called\n");

  rot_register(&meade_caps);

  return RIG_OK;
}
