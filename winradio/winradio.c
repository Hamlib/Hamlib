/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * winradio.c - Copyright (C) 2001 pab@users.sourceforge.net
 * Derived from hamlib code (C) 2000 Stephane Fillod.
 *
 * This shared library supports winradio receivers through the
 * /dev/winradio API.
 *
 *
 *		$Id: winradio.c,v 1.1 2001-02-07 23:54:14 f4cfe Exp $
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


static int wr_rig_init(RIG *rig) {
  rig->state.port_type = RIG_PORT_DEVICE;
  return RIG_OK;
}

static int wr_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  unsigned long f = freq;
  if ( ioctl(rig->state.fd, RADIO_SET_FREQ, &f) ) return -RIG_EINVAL;
  return RIG_OK;
}

static int wr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  unsigned long f;
  if ( ioctl(rig->state.fd, RADIO_GET_FREQ, &f) < 0 ) return -RIG_EINVAL;
  *freq = f;
  return RIG_OK;
}

static int wr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
  unsigned long m;
  switch ( mode ) {
  case RIG_MODE_AM:  m = RMD_AM; break;
  case RIG_MODE_CW:  m = RMD_CW; break;
  case RIG_MODE_LSB: m = RMD_LSB; break;
  case RIG_MODE_USB: m = RMD_USB; break;
  case RIG_MODE_FM:
    switch ( width ) {
    case RIG_PASSBAND_NARROW: m = RMD_FM6; break;
    case RIG_PASSBAND_NORMAL: m = RMD_FMN; break;
    case RIG_PASSBAND_WIDE:   m = RMD_FMW; break;
    default: return -RIG_EINVAL;
    }
  default: return -RIG_EINVAL;
  }
  if ( ioctl(rig->state.fd, RADIO_SET_MODE, &m) ) return -RIG_EINVAL;
  return  RIG_OK;
}

static int wr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  unsigned long m;
  if ( ioctl(rig->state.fd, RADIO_GET_MODE, &m) ) return -RIG_EINVAL;
  switch ( m ) {
  case RMD_CW: *mode = RIG_MODE_CW; *width = RIG_PASSBAND_NORMAL; break;
  case RMD_AM: *mode = RIG_MODE_AM; *width = RIG_PASSBAND_NORMAL; break;
  case RMD_FMN: *mode = RIG_MODE_FM; *width = RIG_PASSBAND_NARROW; break;
  case RMD_FMW: *mode = RIG_MODE_FM; *width = RIG_PASSBAND_WIDE; break;
  case RMD_LSB: *mode = RIG_MODE_LSB; *width = RIG_PASSBAND_NORMAL; break;
  case RMD_USB: *mode = RIG_MODE_USB; *width = RIG_PASSBAND_NORMAL; break;
  case RMD_FMM: *mode = RIG_MODE_FM; *width = RIG_PASSBAND_NARROW; break;
  case RMD_FM6: *mode = RIG_MODE_FM; *width = RIG_PASSBAND_NARROW; break;
  default: return -RIG_EINVAL;
  }
  return RIG_OK;
}

static int wr_set_poweron(RIG *rig) {
  unsigned long p = 1;
  if ( ioctl(rig->state.fd, RADIO_SET_POWER, &p) ) return -RIG_EINVAL;
  return RIG_OK;
}
static int wr_set_poweroff(RIG *rig) {
  unsigned long p = 0;
  if ( ioctl(rig->state.fd, RADIO_SET_POWER, &p) ) return -RIG_EINVAL;
  return RIG_OK;
}

static int wr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val) {
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

static int wr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val) {
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

static unsigned char *wr_get_info(RIG *rig) {
  static char buf[100];
  if ( ioctl(rig->state.fd, RADIO_GET_DESCR, buf) < 0 ) return "?";
  return buf;
}

/*
 * Winradio rigs capabilities.
 */

#define WR1500_FUNC  0
#define WR1500_SET_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AF)
#define WR1500_LEVEL (WR1500_SET_LEVEL | RIG_LEVEL_STRENGTH)

#define WR1500_MODES (RIG_MODE_AM | RIG_MODE_CW | \
                     RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_FM)

const struct rig_caps wr1500_caps = {
  rig_model:     RIG_MODEL_WR1500,
  model_name:    "WR-1500",
  mfg_name:      "Winradio",
  version:       "0.6",
  status:        RIG_STATUS_NEW,
  rig_type:      RIG_TYPE_RECEIVER,
  targetable_vfo:	 0,
  ptt_type:      RIG_PTT_NONE,
  has_func:      WR1500_FUNC,
  has_level:     WR1500_LEVEL,
  has_set_level: WR1500_SET_LEVEL,
  transceive:    RIG_TRN_OFF,
  rx_range_list: { {start:KHz(150),end:MHz(1500),modes:WR1500_MODES,
		    low_power:-1,high_power:-1},
		   {0,}, },
  tx_range_list: { {0,}, },
  tuning_steps: { {WR1500_MODES,1}, {0,} },

  rig_init:    wr_rig_init,
  /*
  rig_cleanup: wr_rig_cleanup,
  rig_open:    wr_rig_open,
  rig_close:   wr_rig_close,
  rig_probe:   wr_rig_probe,
  */

  set_freq:    wr_set_freq,
  get_freq:    wr_get_freq,
  set_mode:    wr_set_mode,
  get_mode:    wr_get_mode,
  
  set_poweron:  wr_set_poweron,
  set_poweroff: wr_set_poweroff,
  set_level:    wr_set_level,
  get_level:    wr_get_level,
  set_func:     NULL,
  get_func:     NULL,

  get_info:     wr_get_info,
};

int init_winradio(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "winradio: _init called\n");

	rig_register(&wr1500_caps);

	return RIG_OK;
}
