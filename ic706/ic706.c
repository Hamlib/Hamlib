/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ic706.c - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an IC-706 using the "CI-V" interface.
 *
 *
 * $Id: ic706.c,v 1.1 2000-09-14 00:44:21 f4cfe Exp $  
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
#include <rigcaps.h>
#include <riglist.h>
#include "ic706.h"

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
  RIG_MODEL_IC706, "IC-706", "Icom", "0.1", RIG_STATUS_NEW,
  RIG_TYPE_MOBILE, 1200, 57600, 8, 1, RIG_PARITY_NONE,
  { {30000,199999999,IC706_ALL_RX_MODES,-1,-1}, {0,0,0,0,0}, }, /* rx range */
  { {1800000,1999999,IC706_OTHER_TX_MODES,5000,100000},	/* 100W class */
    {1800000,1999999,IC706_AM_TX_MODES,2000,40000},	/* 40W class */
    {3500000,3999999,IC706_OTHER_TX_MODES,5000,100000},
    {3500000,3999999,IC706_AM_TX_MODES,2000,40000},
	/* TODO: complete HF list. */
    {144000000,148000000,IC706_OTHER_TX_MODES,5000,20000}, /* not sure.. */
    {144000000,148000000,IC706_AM_TX_MODES,2000,20000}, /* ?? */
	{0,0,0,0,0} },
  ic706_init, ic706_cleanup, NULL /* probe not supported */,
  ic706_set_freq_main_vfo_hz
};

const struct rig_caps ic706mkiig_caps = {
  RIG_MODEL_IC706MKIIG, "IC-706MKIIG", "Icom", "0.1", RIG_STATUS_NEW,
  RIG_TYPE_MOBILE, 1200, 57600, 8, 1, RIG_PARITY_NONE,
  { {30000,199999999,IC706_ALL_RX_MODES,-1,-1},	/* this trx also has UHF */
 	{400000000,470000000,IC706_ALL_RX_MODES,-1,-1}, {0,0,0,0,0}, },
  { {1800000,1999999,IC706_OTHER_TX_MODES,5000,100000},	/* 100W class */
    {1800000,1999999,IC706_AM_TX_MODES,2000,40000},	/* 40W class */
    {3500000,3999999,IC706_OTHER_TX_MODES,5000,100000},
    {3500000,3999999,IC706_AM_TX_MODES,2000,40000},
	/* TODO: complete HF list. */
    {144000000,148000000,IC706_OTHER_TX_MODES,5000,50000}, /* 50W */
    {144000000,148000000,IC706_AM_TX_MODES,2000,20000}, /* AM VHF is 20W */
    {430000000,450000000,IC706_OTHER_TX_MODES,5000,20000},
    {430000000,450000000,IC706_AM_TX_MODES,2000,8000},
	{0,0,0,0,0}, },
  ic706_init, ic706_cleanup, NULL /* probe not supported */,
  ic706_set_freq_main_vfo_hz
};


/*
 * Function definitions below
 */

/*
 * setup *priv 
 * serial port is already open (rig->state->fd)
 */
static int ic706_init(RIG *rig)
{
		struct ic706_priv_data *p;

		if (!rig)
				return -1;
		p = (struct ic706_priv_data*)malloc(sizeof(struct ic706_priv_data));
		if (!p) {
				/* whoops! memory shortage! */
				return -2;
		}
		/* TODO: CI-V address should be customizable */
		p->trx_civ_address = 0x58;	/* Transceiver's default address */
		p->ctrl_civ_address = 0xe0;

		rig->state.priv = (void*)p;

		return 0;
}

/*
 * the serial port is closed by the frontend
 */
static int ic706_cleanup(RIG *rig)
{
		if (!rig)
				return -1;

		if (rig->state.priv)
				free(rig->state.priv);
		rig->state.priv = NULL;

		return 0;
}

/*
 * TODO: implement (ie. rewrite it) this function properly,
 * 		right now it just serves as a backend example
 */
static int ic706_set_freq_main_vfo_hz(struct rig_state *rig_s, freq_t freq, rig_mode_t mode)
{
		struct ic706_priv_data *p;
		unsigned char buf[16];
		int cmd_len;

		if (!rig_s)
				return -1;

		p = (struct ic706_priv_data*)rig_s->priv;

		buf[0] = 0xfe;	/* Preamble */
		buf[1] = 0xfe;
		buf[2] = p->trx_civ_address;
		buf[3] = p->ctrl_civ_address;
		if (p->civ_731_mode) {
			buf[4] = freq2bcd(freq,0);	/* BCD freq, freq2bcd to be written!*/
			buf[5] = freq2bcd(freq,1);
			buf[6] = freq2bcd(freq,2);
			buf[7] = freq2bcd(freq,3);
			buf[8] = 0xfd;	/* EOM code */
			cmd_len = 9;
		} else {
			buf[4] = freq2bcd(freq,0);	/* BCD freq, freq2bcd to be written!*/
			buf[5] = freq2bcd(freq,1);
			buf[6] = freq2bcd(freq,2);
			buf[7] = freq2bcd(freq,3);
			buf[8] = freq2bcd(freq,4);
			buf[9] = 0xfd;	/* EOM code */
			cmd_len = 10;
		}

		/* should check return code and that write wrote cmd_len chars! */
		write(rig_s->fd, buf, cmd_len);

		/* wait for ACK ... etc.. */
		/* then set the mode ... */

		return 0;
}



