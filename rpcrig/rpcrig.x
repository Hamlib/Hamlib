%/*
% *  Hamlib Interface - RPC definitions
% *  Copyright (c) 2000-2002 by Stephane Fillod and Frank Singleton
% *
% *	$Id: rpcrig.x,v 1.11 2006-01-09 21:41:39 fillods Exp $
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
%#ifdef HAVE_CONFIG_H
%#include "config.h"
%#endif
%#include <rpc/xdr.h>
%#include <hamlib/rig.h>
#endif

typedef unsigned int model_x;
typedef int vfo_x;
typedef double freq_x;
typedef unsigned int rmode_x;
typedef int pbwidth_x;
typedef unsigned long split_x;
typedef int ptt_x;
typedef int dcd_x;
typedef long vfo_op_x;
typedef long shortfreq_x;
typedef unsigned long setting_x;
typedef long ant_x;
typedef long ann_x;
typedef int rptr_shift_x;
typedef int tone_x;
typedef long scan_x;
typedef long reset_x;
typedef long powerstat_x;

%#if __APPLE__
%static int _rpcsvcdirty;
%#endif

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

union powerstat_res switch (int rigstatus) {
case 0:
	powerstat_x powerstat;
default:
	void;
};

struct split_arg {
	vfo_x vfo;
	split_x split;
	vfo_x tx_vfo;
};
union split_res switch (int rigstatus) {
case 0:
	split_arg split;
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
	vfo_op_x vfo_op;
};

union rptrshift_res switch (int rigstatus) {
case 0:
	rptr_shift_x rptrshift;
default:
	void;
};

struct rptrshift_arg {
	vfo_x vfo;
	rptr_shift_x rptrshift;
};

union shortfreq_res switch (int rigstatus) {
case 0:
	shortfreq_x shortfreq;
default:
	void;
};

struct shortfreq_arg {
	vfo_x vfo;
	shortfreq_x shortfreq;
};

union tone_res switch (int rigstatus) {
case 0:
	tone_x tone;
default:
	void;
};

struct tone_arg {
	vfo_x vfo;
	tone_x tone;
};

union ant_res switch (int rigstatus) {
case 0:
	ant_x ant;
default:
	void;
};

struct ant_arg {
	vfo_x vfo;
	ant_x ant;
};

union ch_res switch (int rigstatus) {
case 0:
	int ch;
default:
	void;
};

struct ch_arg {
	vfo_x vfo;
	int ch;
};

struct scan_s {
	scan_x scan;
	int ch;
};

union scan_res switch (int rigstatus) {
case 0:
	scan_s scan;
default:
	void;
};

struct scan_arg {
	vfo_x vfo;
	scan_x scan;
	int ch;
};



struct	freq_range_s {
	freq_x start;
	freq_x end;
	rmode_x modes;
	int low_power;
	int high_power;
	vfo_x vfo;
	ant_x ant;
};
struct tuning_step_s {
	rmode_x modes;
	shortfreq_x ts;
};
struct filter_s {
	rmode_x modes;
	pbwidth_x width;
};
struct channel_cap_x {
	unsigned int caps;
	setting_x funcs;
	setting_x levels;
};
struct chan_s {
	int start;
	int end;
	unsigned int type;
	channel_cap_x mem_caps;
};

struct rigstate_s {
	int itu_region;

	shortfreq_x max_rit;
	shortfreq_x max_xit;
	shortfreq_x max_ifshift;

	ann_x announces;

	setting_x has_get_func;
	setting_x has_set_func;
	setting_x has_get_level;
	setting_x has_set_level;
	setting_x has_get_parm;
	setting_x has_set_parm;

	int preamp[MAXDBLSTSIZ];
	int attenuator[MAXDBLSTSIZ];

	freq_range_s rx_range_list[FRQRANGESIZ];
	freq_range_s tx_range_list[FRQRANGESIZ];

	tuning_step_s tuning_steps[TSLSTSIZ];

	filter_s filters[FLTLSTSIZ];

	chan_s chan_list[CHANLSTSIZ];
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
		int SETSPLITVFO(split_arg) = 20;
		split_res GETSPLITVFO(vfo_x) = 21;
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
		int SETRPTRSHIFT(rptrshift_arg) = 32;
		rptrshift_res GETRPTRSHIFT(vfo_x) = 33;
		int SETRPTROFFS(shortfreq_arg) = 34;
		shortfreq_res GETRPTROFFS(vfo_x) = 35;
		int SETCTCSSTONE(tone_arg) = 36;
		tone_res GETCTCSSTONE(vfo_x) = 37;
		int SETCTCSSSQL(tone_arg) = 38;
		tone_res GETCTCSSSQL(vfo_x) = 39;
		int SETDCSCODE(tone_arg) = 40;
		tone_res GETDCSCODE(vfo_x) = 41;
		int SETDCSSQL(tone_arg) = 42;
		tone_res GETDCSSQL(vfo_x) = 43;
		int SETRIT(shortfreq_arg) = 44;
		shortfreq_res GETRIT(vfo_x) = 45;
		int SETXIT(shortfreq_arg) = 46;
		shortfreq_res GETXIT(vfo_x) = 47;
		int SETTS(shortfreq_arg) = 48;
		shortfreq_res GETTS(vfo_x) = 49;
		int SCAN(scan_arg) = 50;
		int RESET(reset_x) = 51;
		int SETMEM(ch_arg) = 52;
		ch_res GETMEM(vfo_x) = 53;
		int SETANT(ant_arg) = 54;
		ant_res GETANT(vfo_x) = 55;
		int SETBANK(ch_arg) = 56;
		int SETPOWERSTAT(powerstat_x) = 58;
		powerstat_res GETPOWERSTAT(void) = 59;
	} = 1;
} = 0x20000099;


#ifdef RPC_HDR
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
