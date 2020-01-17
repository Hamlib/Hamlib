/*
 *  Hamlib KIT backend - FUNcube Dongle USB tuner description
 *  Copyright (c) 2009-2011 by Stephane Fillod
 *
 *  Derived from usbsoftrock-0.5:
 *  Copyright (C) 2009 Andrew Nilsson (andrew.nilsson@gmail.com)
 *
 *  Author: Stefano Speretta, Innovative Solutions In Space BV
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _FUNCUBE_H
#define _FUNCUBE_H 1

#define VID									0x04D8
#define PID									0xFB56
#define PIDPLUS								0xFB31
#define VENDOR_NAME							"Hanlincrest Ltd.         "
#define PRODUCT_NAME						"FunCube Dongle"
#define PRODUCT_NAMEPLUS					"FunCube Dongle Pro+"

#define FUNCUBE_INTERFACE					0x02
#define FUNCUBE_CONFIGURATION				-1  /* no setup */
#define FUNCUBE_ALTERNATIVE_SETTING			0x00

#define INPUT_ENDPOINT						0x82
#define OUTPUT_ENDPOINT						0x02

// Commands
#define REQUEST_SET_FREQ					0x64
#define REQUEST_SET_FREQ_HZ					0x65
#define REQUEST_GET_FREQ_HZ					0x66

#define REQUEST_SET_LNA_GAIN 					0x6E
#define REQUEST_SET_MIXER_GAIN					0x72 // Taken from qthid code
#define REQUEST_SET_IF_GAIN				        0x75

#define REQUEST_GET_LNA_GAIN					0x96
#define REQUEST_GET_MIXER_GAIN					0x9A // Taken from qthid code
#define REQUEST_GET_IF_GAIN					0x9D // Taken from qthid code

#define REQUEST_GET_RSSI					0x68

#define FUNCUBE_SUCCESS						0x01

#endif	/* _FUNCUBE_H */
