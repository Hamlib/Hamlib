/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * pcr1000.c - Copyright (C) 2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an Icom PCR-1000.
 *
 *
 *	$Id: pcr1000.c,v 1.2 2001-03-04 13:06:36 f4cfe Exp $  
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

#include "pcr.h"


#define PCR1000_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_WFM)

#define PCR1000_FUNC (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_TSQL|RIG_FUNC_ANF|RIG_FUNC_NR)

#define PCR1000_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_AF|RIG_LEVEL_STRENGTH)

/*
 * IC PCR1000 rigs capabilities.
 */
const struct rig_caps pcr1000_caps = {
  RIG_MODEL_PCR1000, "IC-PCR1000", "Icom", "0.1", "GPL",
  RIG_STATUS_UNTESTED, RIG_TYPE_PCRECEIVER, 
  RIG_PTT_NONE, RIG_DCD_RIG, RIG_PORT_SERIAL,
  300, 38400, 8, 1, RIG_PARITY_NONE, RIG_HANDSHAKE_NONE, 
  0, 0, 200, 3, 
  RIG_FUNC_NONE, PCR1000_FUNC, PCR1000_LEVEL, RIG_LEVEL_SET(PCR1000_LEVEL),
  RIG_PARM_NONE, RIG_PARM_NONE,	/* FIXME: parms */
  pcr1_ctcss_list, NULL,
  { RIG_DBLST_END, },
  { 20, RIG_DBLST_END, },	/* attenuator */
  "0123456789ABCDEF",
  Hz(0), kHz(1.2),	/* RIT, IF-SHIFT */
  0,			/* FIXME: VFO list */
  0, RIG_TRN_RIG,
  0, 0, 0,

  { RIG_CHAN_END, },	/* no memory channel list: this is a PC receiver */

  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  { RIG_FRNG_END, },
  { {kHz(100),MHz(824)-10,PCR1000_MODES,-1,-1},
    {MHz(849)+10,MHz(869)-10,PCR1000_MODES,-1,-1},
    {MHz(894)+10,GHz(1.3)-10,PCR1000_MODES,-1,-1},
 	RIG_FRNG_END, },
  { RIG_FRNG_END, },	/* no TX ranges, this is a receiver */

  { { PCR1000_MODES,Hz(1) },
	  RIG_TS_END,
	},
	      /* mode/filter list, remember: order matters! */
  {
		{RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_AM, kHz(2.8)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(15)},
		{RIG_MODE_WFM|RIG_MODE_FM|RIG_MODE_AM, kHz(50)},
		{RIG_MODE_WFM, kHz(230)},
		RIG_FLT_END,
  },
  NULL,	/* priv */

  pcr_init, pcr_cleanup, NULL, NULL, NULL /* probe not supported yet */,
  pcr_set_freq, pcr_get_freq, pcr_set_mode, pcr_get_mode,
  NULL, 
  get_info:	pcr_get_info,

  /*
   * TODO:
   * set_trn, set_powerstat,
   * set_level AF,SQL,IF,AF,ATT
   * set_func: AGC,NB,TSQL, VSC?
   * get_level, get_dcd, dtmf, ..
   * decode_event, set_ctcss, set_ctcss_sql
   * and also all the associated get_ functions.
   */
};


/*
 * Function definitions below
 */


