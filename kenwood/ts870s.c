/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ts870s.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a Kenwood radio
 * using the serial interface.
 *
 *
 * $Id: ts870s.c,v 1.5 2001-02-11 23:13:13 f4cfe Exp $  
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


#define TS870S_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_AM_TX_MODES RIG_MODE_AM

#define TS870S_FUNC_ALL (RIG_FUNC_TSQL)

#define TS870S_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

/*
 * ts870s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 */
const struct rig_caps ts870s_caps = {
  RIG_MODEL_TS870S, "TS-870S", "Kenwood", "0.1", "GPL",
  RIG_STATUS_UNTESTED, RIG_TYPE_TRANSCEIVER, 
  RIG_PTT_RIG, 1200, 57600, 8, 1, RIG_PARITY_NONE, RIG_HANDSHAKE_NONE, 
  0, 0, 200, 3, 
  RIG_FUNC_NONE, TS870S_FUNC_ALL, TS870S_LEVEL_ALL, TS870S_LEVEL_ALL, 
  { RIG_DBLST_END, },	/* FIXME! */
  { RIG_DBLST_END, },
  9999,
  0, RIG_TRN_ON,
  1000, 0, 0,

  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  { RIG_FRNG_END, },
  {
	{KHz(100),MHz(30),TS870S_ALL_MODES,-1,-1},
	RIG_FRNG_END,
  }, /* rx range */
  {
    {KHz(1800),MHz(2)-1,TS870S_OTHER_TX_MODES,5000,100000},	/* 100W class */
    {KHz(1800),MHz(2)-1,TS870S_AM_TX_MODES,2000,25000},		/* 25W class */
    {KHz(3500),MHz(4)-1,TS870S_OTHER_TX_MODES,5000,100000},
    {KHz(3500),MHz(4)-1,TS870S_AM_TX_MODES,2000,25000},
    {MHz(7),KHz(7300),TS870S_OTHER_TX_MODES,5000,100000},
    {MHz(7),KHz(7300),TS870S_AM_TX_MODES,2000,25000},
    {KHz(10100),KHz(10150),TS870S_OTHER_TX_MODES,5000,100000},
    {KHz(10100),KHz(10150),TS870S_AM_TX_MODES,2000,25000},
    {MHz(14),KHz(14350),TS870S_OTHER_TX_MODES,5000,100000},
    {MHz(14),KHz(14350),TS870S_AM_TX_MODES,2000,25000},
    {KHz(18068),KHz(18168),TS870S_OTHER_TX_MODES,5000,100000},
    {KHz(18068),KHz(18168),TS870S_AM_TX_MODES,2000,25000},
    {MHz(21),KHz(21450),TS870S_OTHER_TX_MODES,5000,100000},
    {MHz(21),KHz(21450),TS870S_AM_TX_MODES,2000,25000},
    {KHz(24890),KHz(24990),TS870S_OTHER_TX_MODES,5000,100000},
    {KHz(24890),KHz(24990),TS870S_AM_TX_MODES,2000,25000},
    {MHz(28),KHz(29700),TS870S_OTHER_TX_MODES,5000,100000},
    {MHz(28),KHz(29700),TS870S_AM_TX_MODES,2000,25000},
	RIG_FRNG_END,
  }, /* tx range */
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
	 RIG_TS_END,
	},
  NULL, NULL, NULL, NULL, NULL /* probe not supported yet */,
  kenwood_set_freq, kenwood_get_freq, kenwood_set_mode, kenwood_get_mode, NULL,
};

/*
 * Function definitions below
 */


