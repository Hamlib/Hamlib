/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ic706.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an IC-706 using the "CI-V" interface.
 *
 *
 * $Id: ic706.c,v 1.1 2000-09-16 01:46:13 f4cfe Exp $  
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

#include <rig.h>
#include <riglist.h>
#include "icom.h"

static int ic706_init(RIG *rig);

#define IC706_ALL_RX_MODES (RIG_MODE_AM| RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB| RIG_MODE_RTTY| RIG_MODE_FM| RIG_MODE_WFM| RIG_MODE_NFM| RIG_MODE_NAM| RIG_MODE_CWR)

/* tx doesn't have WFM.
 * 100W in all modes but AM (40W)
 */ 
#define IC706_OTHER_TX_MODES (RIG_MODE_AM| RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB| RIG_MODE_RTTY| RIG_MODE_FM| RIG_MODE_NFM| RIG_MODE_NAM| RIG_MODE_CWR)
#define IC706_AM_TX_MODES (RIG_MODE_AM| RIG_MODE_NAM)

/*
 * ic706 rigs capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ic706_caps = {
  RIG_MODEL_IC706, "IC-706", "Icom", "0.1", RIG_STATUS_ALPHA,
  RIG_TYPE_MOBILE, 1200, 57600, 8, 1, RIG_PARITY_NONE, 
  RIG_HANDSHAKE_NONE, 2000, 3, 0,
  { {30000,199999999,IC706_ALL_RX_MODES,-1,-1}, {0,0,0,0,0}, }, /* rx range */
  { {1800000,1999999,IC706_OTHER_TX_MODES,5000,100000},	/* 100W class */
    {1800000,1999999,IC706_AM_TX_MODES,2000,40000},	/* 40W class */
    {3500000,3999999,IC706_OTHER_TX_MODES,5000,100000},
    {3500000,3999999,IC706_AM_TX_MODES,2000,40000},
    {7000000,7300000,IC706_OTHER_TX_MODES,5000,100000},
    {7000000,7300000,IC706_AM_TX_MODES,2000,40000},
    {10000000,10150000,IC706_OTHER_TX_MODES,5000,100000},
    {10000000,10150000,IC706_AM_TX_MODES,2000,40000},
    {14000000,14350000,IC706_OTHER_TX_MODES,5000,100000},
    {14000000,14350000,IC706_AM_TX_MODES,2000,40000},
    {18068000,18168000,IC706_OTHER_TX_MODES,5000,100000},
    {18068000,18168000,IC706_AM_TX_MODES,2000,40000},
    {21000000,21450000,IC706_OTHER_TX_MODES,5000,100000},
    {21000000,21450000,IC706_AM_TX_MODES,2000,40000},
    {24890000,24990000,IC706_OTHER_TX_MODES,5000,100000},
    {24890000,24990000,IC706_AM_TX_MODES,2000,40000},
    {28000000,29700000,IC706_OTHER_TX_MODES,5000,100000},
    {28000000,29700000,IC706_AM_TX_MODES,2000,40000},
    {50000000,54000000,IC706_OTHER_TX_MODES,5000,100000},
    {50000000,54000000,IC706_AM_TX_MODES,2000,40000},
    {144000000,148000000,IC706_OTHER_TX_MODES,5000,20000}, /* not sure.. */
    {144000000,148000000,IC706_AM_TX_MODES,2000,8000}, /* anyone? */
	{0,0,0,0,0} },
  ic706_init, icom_cleanup, NULL, NULL, NULL /* probe not supported yet */,
  icom_set_freq_main_vfo_hz
};

const struct rig_caps ic706mkiig_caps = {
  RIG_MODEL_IC706MKIIG, "IC-706MKIIG", "Icom", "0.1", RIG_STATUS_ALPHA,
  RIG_TYPE_MOBILE, 1200, 57600, 8, 1, RIG_PARITY_NONE,
  RIG_HANDSHAKE_NONE, 2000, 3, 0,
  { {30000,199999999,IC706_ALL_RX_MODES,-1,-1},	/* this trx also has UHF */
 	{400000000,470000000,IC706_ALL_RX_MODES,-1,-1}, {0,0,0,0,0}, },
  { {1800000,1999999,IC706_OTHER_TX_MODES,5000,100000},	/* 100W class */
    {1800000,1999999,IC706_AM_TX_MODES,2000,40000},	/* 40W class */
    {3500000,3999999,IC706_OTHER_TX_MODES,5000,100000},
    {3500000,3999999,IC706_AM_TX_MODES,2000,40000},
	{7000000,7300000,IC706_OTHER_TX_MODES,5000,100000},
    {7000000,7300000,IC706_AM_TX_MODES,2000,40000},
    {10000000,10150000,IC706_OTHER_TX_MODES,5000,100000},
    {10000000,10150000,IC706_AM_TX_MODES,2000,40000},
    {14000000,14350000,IC706_OTHER_TX_MODES,5000,100000},
    {14000000,14350000,IC706_AM_TX_MODES,2000,40000},
    {18068000,18168000,IC706_OTHER_TX_MODES,5000,100000},
    {18068000,18168000,IC706_AM_TX_MODES,2000,40000},
    {21000000,21450000,IC706_OTHER_TX_MODES,5000,100000},
    {21000000,21450000,IC706_AM_TX_MODES,2000,40000},
    {24890000,24990000,IC706_OTHER_TX_MODES,5000,100000},
    {24890000,24990000,IC706_AM_TX_MODES,2000,40000},
    {28000000,29700000,IC706_OTHER_TX_MODES,5000,100000},
    {28000000,29700000,IC706_AM_TX_MODES,2000,40000},
    {50000000,54000000,IC706_OTHER_TX_MODES,5000,100000},
    {50000000,54000000,IC706_AM_TX_MODES,2000,40000},
    {144000000,148000000,IC706_OTHER_TX_MODES,5000,50000}, /* 50W */
    {144000000,148000000,IC706_AM_TX_MODES,2000,20000}, /* AM VHF is 20W */
    {430000000,450000000,IC706_OTHER_TX_MODES,5000,20000},
    {430000000,450000000,IC706_AM_TX_MODES,2000,8000},
	{0,0,0,0,0}, },
  ic706_init, icom_cleanup, NULL, NULL, NULL /* probe not supported yet */,
  icom_set_freq_main_vfo_hz
};


/* this struct in an array could lead 
 * to an Icom generic ic_init function
 */
static const struct icom_priv_data ic706_priv = { 0x58, 0};

/*
 * Function definitions below
 */

/*
 * setup *priv 
 * serial port is already open (rig->state->fd)
 */
static int ic706_init(RIG *rig)
{
		struct icom_priv_data *p;

		if (!rig)
				return -1;

		p = (struct icom_priv_data*)malloc(sizeof(struct icom_priv_data));
		if (!p) {
				/* whoops! memory shortage! */
				return -2;
		}
		/* TODO: CI-V address should be customizable */

		/* init the priv_data from static struct 
		 *          + override with rig-specific preferences
		 */
		*p = ic706_priv;

		rig->state.priv = (void*)p;

		return 0;
}


