/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * winradio.c - Copyright (C) 2001 pab@users.sourceforge.net
 * Derived from hamlib code (C) 2000,2001 Stephane Fillod.
 *
 * This shared library supports winradio receivers through the
 * /dev/winradio API.
 *
 *
 *		$Id: winradio.c,v 1.8 2001-04-26 21:32:54 f4cfe Exp $
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *  */

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <serial.h>
#include <misc.h>
#include "winradio.h"

#include <linradio/wrapi.h>
#include <linradio/radio_ioctl.h>

#define DEFAULT_WINRADIO_PATH "/dev/winradio0"

int wr_rig_init(RIG *rig) {
  rig->state.port_type = RIG_PORT_DEVICE;
  strncpy(rig->state.rig_path, DEFAULT_WINRADIO_PATH, FILPATHLEN);

  return RIG_OK;
}

int wr_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  unsigned long f = freq;
  if ( ioctl(rig->state.fd, RADIO_SET_FREQ, &f) ) return -RIG_EINVAL;
  return RIG_OK;
}

int wr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  unsigned long f;
  if ( ioctl(rig->state.fd, RADIO_GET_FREQ, &f) < 0 ) return -RIG_EINVAL;
  *freq = f;
  return RIG_OK;
}

int wr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
  unsigned long m;
  switch ( mode ) {
  case RIG_MODE_AM:  m = RMD_AM; break;
  case RIG_MODE_CW:  m = RMD_CW; break;
  case RIG_MODE_LSB: m = RMD_LSB; break;
  case RIG_MODE_USB: m = RMD_USB; break;
  case RIG_MODE_WFM: m = RMD_FMW; break;
  case RIG_MODE_FM:
    switch ( width ) {
    case RIG_PASSBAND_NORMAL:
    case kHz(17):
    case kHz(15): m = RMD_FMN; break;

    case kHz(6): m = RMD_FM6; break;
    case kHz(50): m = RMD_FMM; break;
    default: return -RIG_EINVAL;
    }
  default: return -RIG_EINVAL;
  }
  if ( ioctl(rig->state.fd, RADIO_SET_MODE, &m) ) return -RIG_EINVAL;
  return  RIG_OK;
}

int wr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  unsigned long m;
  if ( ioctl(rig->state.fd, RADIO_GET_MODE, &m) ) return -RIG_EINVAL;

  *width = RIG_PASSBAND_NORMAL;
  switch ( m ) {
  case RMD_CW: *mode = RIG_MODE_CW; break;
  case RMD_AM: *mode = RIG_MODE_AM; break;
  case RMD_FMN: *mode = RIG_MODE_FM; break; /* 15kHz or 17kHz on WR-3100 */
  case RMD_FM6: *mode = RIG_MODE_FM; break; /* 6kHz */
  case RMD_FMM: *mode = RIG_MODE_FM; break; /* 50kHz */
  case RMD_FMW: *mode = RIG_MODE_WFM; break;
  case RMD_LSB: *mode = RIG_MODE_LSB; break;
  case RMD_USB: *mode = RIG_MODE_USB; break;
  default: return -RIG_EINVAL;
  }
  if (*width == RIG_PASSBAND_NORMAL)
  	*width = rig_passband_normal(rig,*mode);

  return RIG_OK;
}

int wr_set_powerstat(RIG *rig, powerstat_t status) {
  unsigned long p = 1;
  p = status==RIG_POWER_ON ? 1 : 0;
  if ( ioctl(rig->state.fd, RADIO_SET_POWER, &p) ) return -RIG_EINVAL;
  return RIG_OK;
}
int wr_get_powerstat(RIG *rig, powerstat_t *status) {
  unsigned long p;
  if ( ioctl(rig->state.fd, RADIO_GET_POWER, &p) ) return -RIG_EINVAL;
  *status = p ? RIG_POWER_ON : RIG_POWER_OFF;
  return RIG_OK;
}

int wr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val) {
  switch ( level ) {
  case RIG_LEVEL_AF: {
    unsigned long v;
    if ( ioctl(rig->state.fd, RADIO_GET_MAXVOL, &v) ) return -RIG_EINVAL;
    v *= val.f;
    if ( ioctl(rig->state.fd, RADIO_SET_VOL, &v) ) return -RIG_EINVAL;
    return RIG_OK;
  }
  case RIG_LEVEL_ATT: {
    unsigned long v = val.i ? 1 : 0;
    if ( ioctl(rig->state.fd, RADIO_SET_ATTN, &v) ) return -RIG_EINVAL;
    return RIG_OK;
  }
  default:
    return -RIG_EINVAL;
  }    
}

int wr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val) {
  switch ( level ) {
  case RIG_LEVEL_AF: {
    unsigned long v, mv;
    if ( ioctl(rig->state.fd, RADIO_GET_MAXVOL, &mv) ) return -RIG_EINVAL;
    if ( ioctl(rig->state.fd, RADIO_GET_VOL, &v) ) return -RIG_EINVAL;
    val->f = (float)v / mv;
    return RIG_OK;
  }
  case RIG_LEVEL_ATT: {
    unsigned long v;
    if ( ioctl(rig->state.fd, RADIO_GET_VOL, &v) ) return -RIG_EINVAL;
    val->i = v ? 20 : 0; /* TODO */
    return RIG_OK;
  }
  case RIG_LEVEL_STRENGTH: {
    unsigned long v;
    if ( ioctl(rig->state.fd, RADIO_GET_SS, &v) ) return -RIG_EINVAL;
    val->i = v; /* TODO */
    return RIG_OK;
  }
  default:
    return -RIG_EINVAL;
  }   
}

/*
 * FIXME: static buf does not allow reentrancy!
 */
unsigned char *wr_get_info(RIG *rig) {
  static char buf[100];
  if ( ioctl(rig->state.fd, RADIO_GET_DESCR, buf) < 0 ) return "?";
  return buf;
}


int init_winradio(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "winradio: _init called\n");

	rig_register(&wr1500_caps);

	return RIG_OK;
}
