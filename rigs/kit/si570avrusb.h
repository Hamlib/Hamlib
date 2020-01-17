/***
 * SoftRock USB I2C host control program
 * Copyright (C) 2009 Andrew Nilsson (andrew.nilsson@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Based on powerSwitch.c by Christian Starkjohann,
 * and usbtemp.c by Mathias Dalheimer
 * of Objective Development Software GmbH (2005)
 * (see http://www.obdev.at/avrusb)
 *
 */

#ifndef _SI570AVRUSB_H
#define _SI570AVRUSB_H 1

/* DG8SAQ specific values */
#define SI570_I2C_ADDR 0x55
#define SI570_DCO_HIGH 5670.0
#define SI570_DCO_LOW 4850.0
#define SI570_NOMINAL_XTALL_FREQ 114.285
#define SI570_XTALL_DEVIATION_PPM 2000
#define SI570_DEFAULT_STARTUP_FREQ	56.32

#define REQUEST_READ_VERSION			0x00
#define REQUEST_SET_DDRB			0x01
#define REQUEST_SET_PORTB			0x04
#define REQUEST_READ_EEPROM			0x11
#define REQUEST_FILTERS				0x17
#define REQUEST_SET_FREQ			0x30
#define REQUEST_SET_FREQ_BY_VALUE	0x32
#define REQUEST_SET_XTALL_FREQ		0x33
#define REQUEST_SET_STARTUP_FREQ	0x34
#define REQUEST_READ_MULTIPLY_LO	0x39
#define REQUEST_READ_FREQUENCY		0x3A
#define REQUEST_READ_SMOOTH_TUNE_PPM    0x3B
#define REQUEST_READ_STARTUP		0x3C
#define REQUEST_READ_XTALL		0x3D
#define REQUEST_READ_REGISTERS		0x3F
//#define REQUEST_SET_STARTUP_FREQ	0x41
#define REQUEST_SET_PTT				0x50
#define REQUEST_READ_KEYS			0x51

struct solution {
	int HS_DIV;
	int N1;
	double f0;
	double RFREQ;
};

#endif	/* _SI570AVRUSB_H */
