/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ar8200.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a AOR-AR8200 handheld scanner radio
 * using the serial interface.
 *
 *
 * $Id: ar8200.c,v 1.9 2001-04-26 21:29:34 f4cfe Exp $  
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


#define AR8200_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM)

#define AR8200_FUNC_ALL (RIG_FUNC_TSQL)

#define AR8200_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL)

/*
 * ar8200 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/8200.htm
 */
const struct rig_caps ar8200_caps = {
  RIG_MODEL_AR8200, "AR8200", "AOR", "0.1", "GPL",
  RIG_STATUS_UNTESTED, RIG_TYPE_SCANNER, 
  RIG_PTT_NONE, RIG_DCD_NONE, RIG_PORT_SERIAL, 
  4800, 19200, 8, 2, RIG_PARITY_NONE, RIG_HANDSHAKE_XONXOFF,
  0, 0, 200, 3, 
  RIG_FUNC_NONE, AR8200_FUNC_ALL, AR8200_LEVEL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH, AR8200_LEVEL,
  RIG_PARM_NONE, RIG_PARM_NONE,	/* FIXME: parms */
  NULL, NULL,	/* FIXME: CTCSS/DCS list */
  { RIG_DBLST_END, },
  { RIG_DBLST_END, },
  NULL,
  Hz(0), Hz(0),	/* RIT, IF-SHIFT */
  RIG_VFO_A,
  0, RIG_TRN_RIG,
  1000, 0, 0,

  { RIG_CHAN_END, },	/* FIXME: memory channel list */

  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  { RIG_FRNG_END, },
  {
	{kHz(100),MHz(2040),AR8200_MODES,-1,-1,RIG_VFO_A},
	RIG_FRNG_END,
  }, /* rx range */
  { RIG_FRNG_END, },	/* no tx range, this is a scanner! */

  {
	 {AR8200_MODES,50},
	 {AR8200_MODES,100},
	 {AR8200_MODES,kHz(1)},
	 {AR8200_MODES,kHz(5)},
	 {AR8200_MODES,kHz(9)},
	 {AR8200_MODES,kHz(10)},
	 {AR8200_MODES,12500},
	 {AR8200_MODES,kHz(20)},
	 {AR8200_MODES,kHz(25)},
	 {AR8200_MODES,kHz(100)},
	 {AR8200_MODES,MHz(1)},
	 {AR8200_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
    {
        /* mode/filter list, remember: order matters! */
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(3)}, 
		{RIG_MODE_AM, kHz(9)},
		{RIG_MODE_AM, kHz(3)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(12)}, 
		{RIG_MODE_FM, kHz(9)}, 
		{RIG_MODE_WFM, kHz(230)}, /* 50kHz at -3dB, 380kHz at -20dB */
		RIG_FLT_END,
	},

  NULL,	/* priv */
  NULL, NULL, NULL, aor_close, NULL /* probe not supported yet */,
  aor_set_freq, aor_get_freq, aor_set_mode, aor_get_mode, NULL,
set_ts: aor_set_ts,
set_powerstat: aor_set_powerstat,
};

/*
 * Function definitions below
 */


