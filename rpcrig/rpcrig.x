%/*
% *  Hamlib Interface - RPC definitions
% *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
% *
% *		$Id: rpcrig.x,v 1.2 2001-12-26 23:40:54 fillods Exp $
% *
% *   This library is free software; you can redistribute it and/or modify
% *   it under the terms of the GNU Library General Public License as
% *   published by the Free Software Foundation; either version 2 of
% *   the License, or (at your option) any later version.
% *
% *   This program is distributed in the hope that it will be useful,
% *   but WITHOUT ANY WARRANTY; without even the implied warranty of
% *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% *   GNU Library General Public License for more details.
% *
% *   You should have received a copy of the GNU Library General Public
% *   License along with this library; if not, write to the Free Software
% *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
% *
% */

/* This gets stuffed into the source files. */
#if RPC_HDR
%#include <rpc/xdr.h>
#endif

typedef unsigned int model_x;
typedef int vfo_x;
typedef hyper freq_x;
typedef unsigned int rmode_x;
typedef int pbwidth_x;
typedef unsigned long split_x;
typedef int ptt_x;
typedef int dcd_x;
typedef long vfo_op_x;
typedef long shortfreq_x;
typedef unsigned hyper setting_x;

struct mode_s {
	rmode_x mode;
	pbwidth_x width;
};

/* a union would have been better, but struct is simpler */
struct value_s {
	int i;
	float f;
};



struct freq_arg {
	vfo_x vfo;
	freq_x freq;
};
union freq_res switch (int rigstatus) {
case 0:
	freq_x freq;
default:
	void;
};

struct mode_arg {
	vfo_x vfo;
	mode_s mw;
};
union mode_res switch (int rigstatus) {
case 0:
	mode_s mw;
default:
	void;
};

union vfo_res switch (int rigstatus) {
case 0:
	vfo_x vfo;
default:
	void;
};

struct split_arg {
	vfo_x vfo;
	split_x split;
};
union split_res switch (int rigstatus) {
case 0:
	split_x split;
default:
	void;
};

struct ptt_arg {
	vfo_x vfo;
	ptt_x ptt;
};
union ptt_res switch (int rigstatus) {
case 0:
	ptt_x ptt;
default:
	void;
};
union dcd_res switch (int rigstatus) {
case 0:
	dcd_x dcd;
default:
	void;
};

struct setting_arg {
	vfo_x vfo;
	setting_x setting;
	value_s val;
};

union val_res switch (int rigstatus) {
case 0:
	value_s val;
default:
	void;
};

struct vfo_op_arg {
	vfo_x vfo;
	vfo_op_x op;
};


struct rigstate_s {
	int itu_region;
#if 0
	freq_range_t rx_range_list[FRQRANGESIZ];
	freq_range_t tx_range_list[FRQRANGESIZ];

	struct tuning_step_list tuning_steps[TSLSTSIZ];

	struct filter_list filters[FLTLSTSIZ];

	chan_t chan_list[CHANLSTSIZ];

	shortfreq_x max_rit;
	shortfreq_x max_xit;
	shortfreq_x max_ifshift;

	ann_x announces;

	int preamp<MAXDBLSTSIZ>;
	int attenuator<MAXDBLSTSIZ>;

#endif
	setting_x has_get_func;
	setting_x has_set_func;
	setting_x has_get_level;
	setting_x has_set_level;
	setting_x has_get_parm;
	setting_x has_set_parm;
};

union rigstate_res switch (int rigstatus) {
case 0:
	rigstate_s state;
default:
	void;
};



program RIGPROG {
	version RIGVERS {
		model_x GETMODEL(void) = 1;
		/* string GETLIBVERSION(void) = 2 */
		rigstate_res GETRIGSTATE(void) = 3;
		int SETFREQ(freq_arg) = 10;
		freq_res GETFREQ(vfo_x) = 11;
		int SETMODE(mode_arg) = 12;
		mode_res GETMODE(vfo_x) = 13;
		int SETVFO(vfo_x) = 14;
		vfo_res GETVFO(vfo_x) = 15;
		int SETSPLITFREQ(freq_arg) = 16;
		freq_res GETSPLITFREQ(vfo_x) = 17;
		int SETSPLITMODE(mode_arg) = 18;
		mode_res GETSPLITMODE(vfo_x) = 19;
		int SETSPLIT(split_arg) = 20;
		split_res GETSPLIT(vfo_x) = 21;
		int SETPTT(ptt_arg) = 22;
		ptt_res GETPTT(vfo_x) = 23;
		dcd_res GETDCD(vfo_x) = 24;
		int SETFUNC(setting_arg) = 25;
		val_res GETFUNC(setting_arg) = 26;
		int SETLEVEL(setting_arg) = 27;
		val_res GETLEVEL(setting_arg) = 28;
		int SETPARM(setting_arg) = 29;
		val_res GETPARM(setting_arg) = 30;
		int VFOOP(vfo_op_arg) = 31;
	} = 1;
} = 0x20000099;


#ifdef RPC_HDR
%
%#include <hamlib/rig.h>
%
%#define freq_t2x(t, x) do { *(x) = (t); } while(0)
%#define freq_x2t(x) ((freq_t)*(x))
%
%#define setting_t2x(t, x) do { *(x) = (t); } while(0)
%#define setting_x2t(x) ((setting_t)*(x))
%
%static inline void mode_t2s(rmode_t modet, pbwidth_t widtht, mode_s *modes)
%{
%	modes->mode = modet;
%	modes->width = widtht;
%}
%static inline void mode_s2t(mode_s *modes, rmode_t *modet, pbwidth_t *widtht)
%{
%	*modet = modes->mode;
%	*widtht = modes->width;
%}
#endif /* RPC_HDR */
