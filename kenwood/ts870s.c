/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ts870s.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a Kenwood radio
 * using the serial interface.
 *
 *
 * $Id: ts870s.c,v 1.1 2000-12-23 08:40:14 f4cfe Exp $  
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
#include "kenwood.h"


#define TS870S_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM)

#define TS870S_FUNC_ALL (RIG_FUNC_TSQL)

#define TS870S_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

/*
 * ts870s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ts870s_caps = {
  RIG_MODEL_TS870S, "TS-870S", "Kenwood", "0.1", RIG_STATUS_UNTESTED,
  RIG_TYPE_TRANSCEIVER, RIG_PTT_RIG, 1200, 57600, 8, 1, RIG_PARITY_NONE, 
  RIG_HANDSHAKE_NONE, 0, 0, 2000, 3, TS870S_FUNC_ALL, TS870S_LEVEL_ALL,
  TS870S_LEVEL_ALL, 1000, RIG_TRN_ON,
  {
	{KHz(100),MHz(2040),TS870S_ALL_MODES,-1,-1},
	{0,0,0,0,0},
  }, /* rx range */
  { {0,0,0,0,0} },	/* no tx range, this is a scanner! */
	{
	 {TS870S_ALL_MODES,50},
	 {TS870S_ALL_MODES,100},
	 {TS870S_ALL_MODES,KHz(1)},
	 {TS870S_ALL_MODES,KHz(5)},
	 {TS870S_ALL_MODES,KHz(9)},
	 {TS870S_ALL_MODES,KHz(10)},
	 {TS870S_ALL_MODES,12500},
	 {TS870S_ALL_MODES,KHz(20)},
	 {TS870S_ALL_MODES,KHz(25)},
	 {TS870S_ALL_MODES,KHz(100)},
	 {TS870S_ALL_MODES,MHz(1)},
	 {TS870S_ALL_MODES,0},	/* any tuning step */
	 {0,0}
	},
  NULL, NULL, NULL, NULL, NULL /* probe not supported yet */,
  kenwood_set_freq, kenwood_get_freq, kenwood_set_mode, kenwood_get_mode, NULL,
};

/*
 * Function definitions below
 */


