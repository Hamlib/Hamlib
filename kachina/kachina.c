/*
 *  Hamlib Kachina backend - main file
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: kachina.c,v 1.2 2001-12-20 08:02:44 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
#include <serial.h>
#include <misc.h>
#include <cal.h>

#include "kachina.h"


/*
 * protocol format
 */
#define STX		0x02
#define ETX		0x03
#define GDCMD	0xff
#define ERRCMD	0xfe

/*
 * modes in use by the "M" command
 */
#define M_AM	0x01
#define M_CW	0x02
#define M_FM	0x03
#define M_USB	0x04
#define M_LSB	0x05

#define DDS_CONST 2.2369621333
#define DDS_BASE 75000000

/* uppermost 2 bits of the high byte 
 * designating the antenna port in DDS calculation 
 */
#define PORT_AB	0x00
#define PORT_A	0x40
#define PORT_B	0x80
#define PORT_BA 0xc0

/*
 * kachina_transaction
 * We assume that rig!=NULL, rig->state!= NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
int kachina_transaction(RIG *rig, unsigned char cmd1, unsigned char cmd2)
{
	int count, retval;
	struct rig_state *rs;
	unsigned char buf4[4];

	rs = &rig->state;

	buf4[0] = STX;
	buf4[1] = cmd1;
	buf4[2] = cmd2;
	buf4[3] = ETX;

	retval = write_block(&rs->rigport, buf4, 4);
	if (retval != RIG_OK)
			return retval;

	count = fread_block(&rs->rigport, buf4, 1);
	if (count != 1)
			return count;

	return (buf4[0]==GDCMD) ? RIG_OK : -RIG_EPROTO;
}

int kachina_trans_n(RIG *rig, unsigned char cmd1, const char *data, int data_len)
{
	int cmd_len, count, retval;
	struct rig_state *rs;
	unsigned char buf[16];

	rs = &rig->state;

	buf[0] = STX;
	buf[1] = cmd1;
	memcpy(buf+2, data, data_len);
	buf[data_len+2] = ETX;

	cmd_len = data_len+3;

	retval = write_block(&rs->rigport, buf, cmd_len);
	if (retval != RIG_OK)
			return retval;

	count = fread_block(&rs->rigport, buf, 1);
	if (count != 1)
			return count;

	return (buf[0]==GDCMD) ? RIG_OK : -RIG_EPROTO;
}

/*
 * convert a frequency in Hz in the range of 30kHz to 30MHz
 * to DDS value, as expected by the Kachina.
 */
void inline freq2dds(freq_t freq, int ant_port, unsigned char fbuf[4])
{
		double dds;
		unsigned long dds_ulong;

		dds = DDS_CONST * ( DDS_BASE + freq);
		dds_ulong = (unsigned long)dds;

		/* 
		 * byte 0 transferred first,
		 * dds is big endian format
		 */
		fbuf[0] = ant_port | ((dds_ulong>>24) & 0x3f);
		fbuf[1] = (dds_ulong>>16) & 0xff;
		fbuf[2] = (dds_ulong>>8) & 0xff;
		fbuf[3] = dds_ulong & 0xff;
}

/*
 * kachina_set_freq
 * Assumes rig!=NULL
 */
int kachina_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		int retval;
		unsigned char freqbuf[4];

		freq2dds(freq, PORT_A, freqbuf);

		/* receive frequency */
		retval = kachina_trans_n(rig, 'R', freqbuf, 4);
		if (retval != RIG_OK)
				return retval;

		/* transmit frequency */
		retval = kachina_trans_n(rig, 'T', freqbuf, 4);
		if (retval != RIG_OK)
				return retval;

		return RIG_OK;
}

/*
 * kachina_set_mode
 * Assumes rig!=NULL
 *
 * FIXME: pbwidth handling
 */
int kachina_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		int retval;
		unsigned char k_mode;

		switch (mode) {
			case RIG_MODE_CW:       k_mode = M_CW; break;
			case RIG_MODE_USB:      k_mode = M_USB; break;
			case RIG_MODE_LSB:      k_mode = M_LSB; break;
			case RIG_MODE_FM:       k_mode = M_FM; break;
			case RIG_MODE_AM:       k_mode = M_AM; break;
			default:
				rig_debug(RIG_DEBUG_ERR,
								"kachina_set_mode: unsupported mode %d\n",
								mode);
				return -RIG_EINVAL;
		}

		retval = kachina_transaction (rig, 'M', k_mode);
		if (retval != RIG_OK)
			return retval;

		return retval;
}

/*
 * init_kachina is called by rig_backend_load
 */
int init_kachina(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "kachina: _init called\n");

		rig_register(&k505dsp_caps);

		return RIG_OK;
}

