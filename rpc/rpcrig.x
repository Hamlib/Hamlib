/*
 *  Hamlib Interface - RPC definitions
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: rpcrig.x,v 1.1 2001-10-07 21:42:13 f4cfe Exp $
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

typedef unsigned int vfo_x;

/* Too bad, there's no long long support in RPC language */
struct freq_s {
	unsigned int f1;
	unsigned int f2;
};

typedef unsigned int model_x;

union freq_res switch (int rigstatus) {
case 0:
	freq_s freq;
default:
	void;
};

struct freq_arg {
	vfo_x vfo;
	freq_s freq;
};

program RIGPROG {
	version RIGVERS {
		model_x GETMODEL(void) = 1;
		/* string GETLIBVERSION(void) = 2 */
		int SETFREQ(freq_arg) = 10;
		freq_res GETFREQ(vfo_x) = 11;
	} = 1;
} = 0x20000099;


