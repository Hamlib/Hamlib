/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ar8200.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a AOR-AR8200 handheld scanner radio
 * using the serial interface.
 *
 *
 * $Id: ar8200.c,v 1.1 2000-11-01 23:23:56 f4cfe Exp $  
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
#include "aor.h"


#define AR8200_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM)

#define AR8200_FUNC_ALL (RIG_FUNC_TSQL)

#define AR8200_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

/*
 * ar8200 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ar8200_caps = {
  RIG_MODEL_AR8200, "AR8200", "AOR", "0.1", RIG_STATUS_UNTESTED,
  RIG_TYPE_SCANNER, RIG_PTT_NONE, 9600, 19200, 8, 2, RIG_PARITY_NONE, 
  RIG_HANDSHAKE_XONXOFF, 0, 0, 2000, 3, AR8200_FUNC_ALL, AR8200_LEVEL_ALL,
  AR8200_LEVEL_ALL, 1000, RIG_TRN_ON,
  {
	{KHz(100),MHz(2040),AR8200_ALL_MODES,-1,-1},
	{0,0,0,0,0},
  }, /* rx range */
  { {0,0,0,0,0} },	/* no tx range, this is a scanner! */
	{
	 {AR8200_ALL_MODES,50},
	 {AR8200_ALL_MODES,100},
	 {AR8200_ALL_MODES,KHz(1)},
	 {AR8200_ALL_MODES,KHz(5)},
	 {AR8200_ALL_MODES,KHz(9)},
	 {AR8200_ALL_MODES,KHz(10)},
	 {AR8200_ALL_MODES,12500},
	 {AR8200_ALL_MODES,KHz(20)},
	 {AR8200_ALL_MODES,KHz(25)},
	 {AR8200_ALL_MODES,KHz(100)},
	 {AR8200_ALL_MODES,MHz(1)},
	 {AR8200_ALL_MODES,0},	/* any tuning step */
	 {0,0}
	},
  NULL, NULL, NULL, aor_close, NULL /* probe not supported yet */,
  aor_set_freq, aor_get_freq, aor_set_mode, aor_get_mode, NULL,
set_ts: aor_set_ts,
set_poweroff: aor_set_poweroff,
};

/*
 * Function definitions below
 */


