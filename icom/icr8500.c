/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icr8500.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICR-8500
 * using the "CI-V" interface.
 *
 *
 * $Id: icr8500.c,v 1.4 2001-02-14 23:54:21 f4cfe Exp $  
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


#define ICR8500_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define ICR8500_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM)

#define ICR8500_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_TSQL|RIG_FUNC_APF)

#define ICR8500_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_APF|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

/*
 * ICR8500 rigs capabilities.
 */
const struct rig_caps icr8500_caps = {
  RIG_MODEL_ICR8500, "ICR-8500", "Icom", "0.2", "GPL", 
  RIG_STATUS_UNTESTED, RIG_TYPE_RECEIVER, 

  RIG_PTT_NONE, RIG_DCD_NONE, RIG_PORT_SERIAL,
  300, 19200, 8, 1, RIG_PARITY_NONE, RIG_HANDSHAKE_NONE,
  0, 0, 200, 3,
  RIG_FUNC_NONE, ICR8500_FUNC_ALL, ICR8500_LEVEL_ALL, ICR8500_LEVEL_ALL,
  { 20, RIG_DBLST_END, },
  { 20, RIG_DBLST_END, },
  9999,
  0, RIG_TRN_ON,
  999, 12, 0,

  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  { RIG_FRNG_END, },

  { {kHz(100),MHz(824)-10,ICR8500_MODES,-1,-1},
    {MHz(849)+10,MHz(869)-10,ICR8500_MODES,-1,-1},
    {MHz(894)+10,GHz(2)-10,ICR8500_MODES,-1,-1},
 	RIG_FRNG_END, },
  { RIG_FRNG_END, },	/* no TX ranges, this is a receiver */

  {  {ICR8500_MODES,10},
	 {ICR8500_MODES,50},
	 {ICR8500_MODES,100},
	 {ICR8500_MODES,kHz(1)},
	 {ICR8500_MODES,2500},
	 {ICR8500_MODES,kHz(5)},
	 {ICR8500_MODES,kHz(9)},
	 {ICR8500_MODES,kHz(10)},
	 {ICR8500_MODES,12500},
	 {ICR8500_MODES,kHz(20)},
	 {ICR8500_MODES,kHz(25)},
	 {ICR8500_MODES,kHz(100)},
	 {ICR8500_1MHZ_TS_MODES,MHz(1)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
	{
			/* FIXME: To be confirmed! --SF */
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_AM, kHz(8)},
		{RIG_MODE_AM, kHz(2.4)},	/* narrow */
		{RIG_MODE_AM, kHz(15)},		/* wide */
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_FM, kHz(8)},	/* narrow */
		{RIG_MODE_WFM, kHz(230)},
		RIG_FLT_END,
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


