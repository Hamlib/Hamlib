/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icr8500.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICR-8500
 * using the "CI-V" interface.
 *
 *
 * $Id: icr8500.c,v 1.1 2001-01-28 22:03:57 f4cfe Exp $  
 *
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
 * 
 */

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "icom.h"


#define ICR8500_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define ICR8500_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM)
#define ICR8500_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY)

#define ICR8500_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_NR|RIG_FUNC_ANF)

#define ICR8500_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

/*
 * ICR8500 rigs capabilities.
 */
const struct rig_caps icr8500_caps = {
  RIG_MODEL_ICR8500, "ICR-8500", "Icom", "0.2", RIG_STATUS_UNTESTED,
  RIG_TYPE_RECEIVER, RIG_PTT_NONE, 300, 19200, 8, 1, RIG_PARITY_NONE, 
  RIG_HANDSHAKE_NONE, 0, 0, 2000, 3, ICR8500_FUNC_ALL,
  ICR8500_LEVEL_ALL, ICR8500_LEVEL_ALL, 0, 999, RIG_TRN_ON,
  { {KHz(10),199999999,ICR8500_ALL_RX_MODES,-1,-1},
 	{0,0,0,0,0}, },
  { {0,0,0,0,0}, },	/* no TX ranges, this is a receiver */
	{{ICR8500_ALL_RX_MODES,10},
	 {ICR8500_ALL_RX_MODES,50},
	 {ICR8500_ALL_RX_MODES,100},
	 {ICR8500_ALL_RX_MODES,KHz(1)},
	 {ICR8500_ALL_RX_MODES,2500},
	 {ICR8500_ALL_RX_MODES,KHz(5)},
	 {ICR8500_ALL_RX_MODES,KHz(9)},
	 {ICR8500_ALL_RX_MODES,KHz(10)},
	 {ICR8500_ALL_RX_MODES,12500},
	 {ICR8500_ALL_RX_MODES,KHz(20)},
	 {ICR8500_ALL_RX_MODES,KHz(25)},
	 {ICR8500_ALL_RX_MODES,KHz(100)},
	 {ICR8500_1MHZ_TS_MODES,MHz(1)},
	 {0,0}
	},
  icom_init, icom_cleanup, NULL, NULL, NULL /* probe not supported yet */,
  icom_set_freq, icom_get_freq, icom_set_mode, icom_get_mode, icom_set_vfo,
  NULL, 
  /*
   * FIXME:
   * the use of the following GNU extension (field: value)
   * is bad manner in portable code but admit it, quite handy
   * when testing stuff. --SF
   */
decode_event: icom_decode_event,
set_level: icom_set_level,
get_level: icom_get_level,
set_func: icom_set_func,
set_channel: icom_set_channel,
get_channel: icom_get_channel,
set_mem: icom_set_mem,
mv_ctl: icom_mv_ctl,
set_ts: icom_set_ts,
get_ts: icom_get_ts,
};


/*
 * Function definitions below
 */


