/*
 *  Hamlib Interface - toolbox
 *  Copyright (c) 2000-2002 by Stephane Fillod and Frank Singleton
 *
 *	$Id: misc.c,v 1.24 2003-01-06 22:08:45 fillods Exp $
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
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/types.h>
#include <unistd.h>

#include <hamlib/rig.h>

#include "misc.h"



static int rig_debug_level = RIG_DEBUG_TRACE;

/*
 * Do a hex dump of the unsigned char array.
 */

#define DUMP_HEX_WIDTH 16

void dump_hex(const unsigned char ptr[], size_t size)
{
  int i;
  char buf[DUMP_HEX_WIDTH+1];

  if (!rig_need_debug(RIG_DEBUG_TRACE))
		  return;

  buf[DUMP_HEX_WIDTH] = '\0';

  for(i=0; i<size; i++) {
    if (i % DUMP_HEX_WIDTH == 0)
      rig_debug(RIG_DEBUG_TRACE,"%.4x\t",i);

    rig_debug(RIG_DEBUG_TRACE," %.2x", ptr[i]);

	if (ptr[i] >= ' ' && ptr[i] < 0x7f)
		buf[i%DUMP_HEX_WIDTH] = ptr[i];
	else
		buf[i%DUMP_HEX_WIDTH] = '.';

    if (i % DUMP_HEX_WIDTH == DUMP_HEX_WIDTH-1)
      rig_debug(RIG_DEBUG_TRACE,"\t%s\n",buf);
  }

  /* Add some spaces in order to align right ASCII dump column */
  if ((i / DUMP_HEX_WIDTH) > 0) {
    int j;
    for (j = i % DUMP_HEX_WIDTH; j < DUMP_HEX_WIDTH; j++)
      rig_debug(RIG_DEBUG_TRACE,"   ");
  }

  if (i % DUMP_HEX_WIDTH != DUMP_HEX_WIDTH-1) {
  	buf[i % DUMP_HEX_WIDTH] = '\0';
    rig_debug(RIG_DEBUG_TRACE,"\t%s\n",buf);
  }

} 


/*
 * Convert a long long (eg. frequency in Hz) to 4-bit BCD digits, 
 * packed two digits per octet, in little-endian order.
 * bcd_len is the number of BCD digits, usually 10 or 8 in 1-Hz units, 
 *	and 6 digits in 100-Hz units for Tx offset data.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/ the 64bit freq)
 */
unsigned char *
to_bcd(unsigned char bcd_data[], unsigned long long freq, unsigned bcd_len)
{
	int i;
	unsigned char a;

	/* '450'/4-> 5,0;0,4 */
	/* '450'/3-> 5,0;x,4 */

	for (i=0; i < bcd_len/2; i++) {
			a = freq%10;
			freq /= 10;
			a |= (freq%10)<<4;
			freq /= 10;
			bcd_data[i] = a;
	}
	if (bcd_len&1) {
			bcd_data[i] &= 0xf0;
			bcd_data[i] |= freq%10;	/* NB: high nibble is left uncleared */
	}

	return bcd_data;
}

/*
 * Convert BCD digits to a long long (eg. frequency in Hz)
 * bcd_len is the number of BCD digits.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/ the 64bit freq)
 */
unsigned long long from_bcd(const unsigned char bcd_data[], unsigned bcd_len)
{
	int i;
	freq_t f = 0;

	if (bcd_len&1)
			f = bcd_data[bcd_len/2] & 0x0f;

	for (i=(bcd_len/2)-1; i >= 0; i--) {
			f *= 10;
			f += bcd_data[i]>>4;
			f *= 10;
			f += bcd_data[i] & 0x0f;
	}
	
	return f;
}

/*
 * Same as to_bcd, but in Big Endian mode
 */
unsigned char *
to_bcd_be(unsigned char bcd_data[], unsigned long long freq, unsigned bcd_len)
{
	int i;
	unsigned char a;

	/* '450'/4 -> 0,4;5,0 */
	/* '450'/3 -> 4,5;0,x */

	if (bcd_len&1) {
			bcd_data[bcd_len/2] &= 0x0f;
			bcd_data[bcd_len/2] |= (freq%10)<<4;	/* NB: low nibble is left uncleared */
			freq /= 10;
	}
	for (i=(bcd_len/2)-1; i >= 0; i--) {
			a = freq%10;
			freq /= 10;
			a |= (freq%10)<<4;
			freq /= 10;
			bcd_data[i] = a;
	}

	return bcd_data;
}

/*
 * Same as from_bcd, but in Big Endian mode
 */
unsigned long long from_bcd_be(const unsigned char bcd_data[], unsigned bcd_len)
{
	int i;
	freq_t f = 0;

	for (i=0; i < bcd_len/2; i++) {
			f *= 10;
			f += bcd_data[i]>>4;
			f *= 10;
			f += bcd_data[i] & 0x0f;
	}
	if (bcd_len&1) {
			f *= 10;
			f += bcd_data[bcd_len/2]>>4;
	}

	return f;
}

/*
 * rig_set_debug
 * Change the current debug level
 */
void rig_set_debug(enum rig_debug_level_e debug_level)
{
		rig_debug_level = debug_level;
}

/*
 * rig_need_debug
 * Usefull for dump_hex, etc.
 */
int rig_need_debug(enum rig_debug_level_e debug_level)
{
		return (debug_level <= rig_debug_level);
}

/*
 * rig_debug
 * Debugging messages are done through stderr
 * TODO: add syslog support if needed
 */
void rig_debug(enum rig_debug_level_e debug_level, const char *fmt, ...)
{
		va_list ap;

		if (debug_level <= rig_debug_level) {
				va_start(ap, fmt);
				/*
				 * Who cares about return code?
				 */
				vfprintf (stderr, fmt, ap);
				va_end(ap);
		}
}

#ifndef llabs
#define llabs(a) ((a)<0?-(a):(a))
#endif

/*
 * rig_freq_snprintf?
 * pretty print frequencies
 * str must be long enough. max can be as long as 17 chars
 */
int sprintf_freq(char *str, freq_t freq)
{
		double f;
		char *hz;

		if (llabs(freq) >= GHz(1)) {
				hz = "GHz";
				f = (double)freq/GHz(1);
		} else if (llabs(freq) >= MHz(1)) {
				hz = "MHz";
				f = (double)freq/MHz(1);
		} else if (llabs(freq) >= kHz(1)) {
				hz = "kHz";
				f = (double)freq/kHz(1);
		} else {
				hz = "Hz";
				f = (double)freq;
		}

		return sprintf (str, "%g %s", f, hz);
}

const char *strptrshift(rptr_shift_t shift)
{
	switch (shift) {
	case RIG_RPT_SHIFT_MINUS: return "+";
	case RIG_RPT_SHIFT_PLUS: return "-";

	case RIG_RPT_SHIFT_NONE: return "None";
	}
	return NULL;
}

const char *strstatus(enum rig_status_e status)
{
	switch (status) {
	case RIG_STATUS_ALPHA:
			return "Alpha";
	case RIG_STATUS_UNTESTED:
			return "Untested";
	case RIG_STATUS_BETA:
			return "Beta";
	case RIG_STATUS_STABLE:
			return "Stable";
	case RIG_STATUS_BUGGY:
			return "Buggy";
	case RIG_STATUS_NEW:
			return "New";
	}
	return "";
}

int sprintf_mode(char *str, rmode_t mode)
{
		int i, len=0;

		*str = '\0';
		if (mode == RIG_MODE_NONE)
				return 0;

		for (i = 0; i < 30; i++) {
				const char *ms = strrmode(mode & (1UL<<i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}

int sprintf_func(char *str, setting_t func)
{
		int i, len=0;

		*str = '\0';
		if (func == RIG_FUNC_NONE)
				return 0;

		for (i = 0; i < 60; i++) {
				const char *ms = strfunc(func & rig_idx2setting(i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}


int sprintf_level(char *str, setting_t level)
{
		int i, len=0;

		*str = '\0';
		if (level == RIG_LEVEL_NONE)
				return 0;

		for (i = 0; i < 60; i++) {
				const char *ms = strlevel(level & rig_idx2setting(i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}


int sprintf_parm(char *str, setting_t parm)
{
		int i, len=0;

		*str = '\0';
		if (parm == RIG_PARM_NONE)
				return 0;

		for (i = 0; i < 60; i++) {
				const char *ms = strparm(parm & rig_idx2setting(i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}


int sprintf_vfop(char *str, vfo_op_t op)
{
		int i, len=0;

		*str = '\0';
		if (op == RIG_OP_NONE)
				return 0;

		for (i = 0; i < 30; i++) {
				const char *ms = strvfop(op & (1UL<<i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}


int sprintf_scan(char *str, scan_t rscan)
{
		int i, len=0;

		*str = '\0';
		if (rscan == RIG_SCAN_NONE)
				return 0;

		for (i = 0; i < 30; i++) {
				const char *ms = strscan(rscan & (1UL<<i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}



static struct { 
		rmode_t mode;
		const char *str;
} mode_str[] = {
	{ RIG_MODE_AM, "AM" },
	{ RIG_MODE_FM, "FM" },
	{ RIG_MODE_CW, "CW" },
	{ RIG_MODE_CWR, "CWR" },
	{ RIG_MODE_USB, "USB" },
	{ RIG_MODE_LSB, "LSB" },
	{ RIG_MODE_RTTY, "RTTY" },
	{ RIG_MODE_RTTYR, "RTTYR" },
	{ RIG_MODE_WFM, "WFM" },
	{ RIG_MODE_NONE, NULL },
};


rmode_t parse_mode(const char *s)
{
	int i;

	for (i=0 ; mode_str[i].str != NULL; i++)
			if (!strcmp(s, mode_str[i].str))
					return mode_str[i].mode;
	return RIG_MODE_NONE;
}

const char * strrmode(rmode_t mode)
{
	int i;

	if (mode == RIG_MODE_NONE)
		return "";

	for (i=0; mode_str[i].str != NULL; i++)
		if (mode == mode_str[i].mode)
			return mode_str[i].str;

	return NULL;
}


static struct { 
		vfo_t vfo ;
		const char *str;
} vfo_str[] = {
		{ RIG_VFO_A, "VFOA" },
		{ RIG_VFO_B, "VFOB" },
		{ RIG_VFO_C, "VFOC" },
		{ RIG_VFO_CURR, "currVFO" },
		{ RIG_VFO_MEM, "MEM" },
		{ RIG_VFO_VFO, "VFO" },
		{ RIG_VFO_MAIN, "Main" },
		{ RIG_VFO_SUB, "Sub" },
		{ RIG_VFO_NONE, NULL },
};

vfo_t parse_vfo(const char *s)
{
	int i;

	for (i=0; vfo_str[i].str != NULL; i++)
		if (!strcmp(s, vfo_str[i].str))
			return vfo_str[i].vfo;
	return RIG_VFO_NONE;
}

const char *strvfo(vfo_t vfo)
{
	int i;

	if (vfo == RIG_VFO_NONE)
		return "";

	for (i=0; vfo_str[i].str != NULL; i++)
		if (vfo == vfo_str[i].vfo)
			return vfo_str[i].str;

	return NULL;
}


static struct { 
		setting_t func; 
		const char *str;
} func_str[] = {
	{ RIG_FUNC_FAGC, "FAGC" },
	{ RIG_FUNC_NB, "NB" },
	{ RIG_FUNC_COMP, "COMP" },
	{ RIG_FUNC_VOX, "VOX" },
	{ RIG_FUNC_TONE, "TONE" },
	{ RIG_FUNC_TSQL, "TSQL" },
	{ RIG_FUNC_SBKIN, "SBKIN" },
	{ RIG_FUNC_FBKIN, "FBKIN" },
	{ RIG_FUNC_ANF, "ANF" },
	{ RIG_FUNC_NR, "NR" },
	{ RIG_FUNC_AIP, "AIP" },
	{ RIG_FUNC_MON, "MON" },
	{ RIG_FUNC_MN, "MN" },
	{ RIG_FUNC_RNF, "RNF" },
	{ RIG_FUNC_ARO, "ARO" },
	{ RIG_FUNC_LOCK, "LOCK" },
	{ RIG_FUNC_MUTE, "MUTE" },
	{ RIG_FUNC_VSC, "VSC" },
	{ RIG_FUNC_REV, "REV" },
	{ RIG_FUNC_SQL, "SQL" },
	{ RIG_FUNC_BC, "BC" },
	{ RIG_FUNC_MBC, "MBC" },
	{ RIG_FUNC_LMP, "LMP" },
	{ RIG_FUNC_AFC, "AFC" },
	{ RIG_FUNC_SATMODE, "SATMODE" },
	{ RIG_FUNC_SCOPE, "SCOPE" },
	{ RIG_FUNC_RESUME, "RESUME" },
	{ RIG_FUNC_TBURST, "TBURST" },
	{ RIG_FUNC_NONE, NULL },
};

setting_t parse_func(const char *s)
{
	int i;

	for (i=0 ; func_str[i].str != NULL; i++)
			if (!strcmp(s, func_str[i].str))
					return func_str[i].func;
	return RIG_FUNC_NONE;
}

const char *strfunc(setting_t func)
{
	int i;

	if (func == RIG_FUNC_NONE)
		return "";

	for (i=0; func_str[i].str != NULL; i++)
		if (func == func_str[i].func)
			return func_str[i].str;

	return NULL;
}


static struct { 
		setting_t level;
		const char *str;
} level_str[] = {
	{ RIG_LEVEL_PREAMP, "PREAMP" },
	{ RIG_LEVEL_ATT, "ATT" },
	{ RIG_LEVEL_VOX, "VOX" },
	{ RIG_LEVEL_AF, "AF" },
	{ RIG_LEVEL_RF, "RF" },
	{ RIG_LEVEL_SQL, "SQL" },
	{ RIG_LEVEL_IF, "IF" },
	{ RIG_LEVEL_APF, "APF" },
	{ RIG_LEVEL_NR, "NR" },
	{ RIG_LEVEL_PBT_IN, "PBT_IN" },
	{ RIG_LEVEL_PBT_OUT, "PBT_OUT" },
	{ RIG_LEVEL_CWPITCH, "CWPITCH" },
	{ RIG_LEVEL_RFPOWER, "RFPOWER" },
	{ RIG_LEVEL_MICGAIN, "MICGAIN" },
	{ RIG_LEVEL_KEYSPD, "KEYSPD" },
	{ RIG_LEVEL_NOTCHF, "NOTCHF" },
	{ RIG_LEVEL_COMP, "COMP" },
	{ RIG_LEVEL_AGC, "AGC" },
	{ RIG_LEVEL_BKINDL, "BKINDL" },
	{ RIG_LEVEL_BALANCE, "BAL" },
	{ RIG_LEVEL_METER, "METER" },
	{ RIG_LEVEL_VOXGAIN, "VOXGAIN" },
	{ RIG_LEVEL_ANTIVOX, "ANTIVOX" },

	{ RIG_LEVEL_SWR, "SWR" },
	{ RIG_LEVEL_ALC, "ALC" },
	{ RIG_LEVEL_SQLSTAT, "SQLSTAT" },
	{ RIG_LEVEL_STRENGTH, "STRENGTH" },
	{ RIG_LEVEL_NONE, NULL },
};

setting_t parse_level(const char *s)
{
	int i;

	for (i=0 ; level_str[i].str != NULL; i++)
			if (!strcmp(s, level_str[i].str))
					return level_str[i].level;
	return RIG_LEVEL_NONE;
}

const char *strlevel(setting_t level)
{
	int i;

	if (level == RIG_LEVEL_NONE)
		return "";

	for (i=0; level_str[i].str != NULL; i++)
		if (level == level_str[i].level)
			return level_str[i].str;

	return NULL;
}


static struct { 
		setting_t parm;
		const char *str;
} parm_str[] = {
	{ RIG_PARM_ANN, "ANN" },
	{ RIG_PARM_APO, "APO" },
	{ RIG_PARM_BACKLIGHT, "BACKLIGHT" },
	{ RIG_PARM_BEEP, "BEEP" },
	{ RIG_PARM_TIME, "TIME" },
	{ RIG_PARM_BAT, "BAT" },
	{ RIG_PARM_NONE, NULL },
};

setting_t parse_parm(const char *s)
{
	int i;

	for (i=0 ; parm_str[i].str != NULL; i++)
			if (!strcmp(s, parm_str[i].str))
					return parm_str[i].parm;
	return RIG_PARM_NONE;
}

const char *strparm(setting_t parm)
{
	int i;

	if (parm == RIG_PARM_NONE)
		return "";

	for (i=0; parm_str[i].str != NULL; i++)
		if (parm == parm_str[i].parm)
			return parm_str[i].str;

	return NULL;
}


static struct { 
		vfo_op_t vfo_op;
		const char *str;
} vfo_op_str[] = {
	{ RIG_OP_CPY, "CPY" },
	{ RIG_OP_XCHG, "XCHG" },
	{ RIG_OP_FROM_VFO, "FROM_VFO" },
	{ RIG_OP_TO_VFO, "TO_VFO" },
	{ RIG_OP_MCL, "MCL" },
	{ RIG_OP_UP, "UP" },
	{ RIG_OP_DOWN, "DOWN" },
	{ RIG_OP_BAND_UP, "BAND_UP" },
	{ RIG_OP_BAND_DOWN, "BAND_DOWN" },
	{ RIG_OP_LEFT, "LEFT" },
	{ RIG_OP_RIGHT, "RIGHT" },
	{ RIG_OP_NONE, NULL },
};

vfo_op_t parse_vfo_op(const char *s)
{
	int i;

	for (i=0 ; vfo_op_str[i].str != NULL; i++)
			if (!strcmp(s, vfo_op_str[i].str))
					return vfo_op_str[i].vfo_op;
	return RIG_OP_NONE;
}

const char *strvfop(vfo_op_t op)
{
	int i;

	if (op == RIG_OP_NONE)
		return "";

	for (i=0; vfo_op_str[i].str != NULL; i++)
		if (op == vfo_op_str[i].vfo_op)
			return vfo_op_str[i].str;

	return NULL;
}


static struct { 
		scan_t rscan;
		const char *str;
} scan_str[] = {
	{ RIG_SCAN_STOP, "STOP" },
	{ RIG_SCAN_MEM, "MEM" },
	{ RIG_SCAN_SLCT, "SLCT" },
	{ RIG_SCAN_PRIO, "PRIO" },
	{ RIG_SCAN_PROG, "PROG" },
	{ RIG_SCAN_DELTA, "DELTA" },
	{ RIG_SCAN_VFO, "VFO" },
	{ RIG_SCAN_NONE, NULL },
};

scan_t parse_scan(const char *s)
{
	int i;

	for (i=0 ; scan_str[i].str != NULL; i++) {
		if (strcmp(s, scan_str[i].str) == 0) {
			return scan_str[i].rscan;
		}
	}

	return RIG_SCAN_NONE;
}

const char *strscan(scan_t rscan)
{
	int i;

	if (rscan == RIG_SCAN_NONE)
		return "";

	for (i=0; scan_str[i].str != NULL; i++)
		if (rscan == scan_str[i].rscan)
			return scan_str[i].str;

	return NULL;
}


rptr_shift_t parse_rptr_shift(const char *s)
{
	if (strcmp(s, "+") == 0)
		return RIG_RPT_SHIFT_PLUS;
	else if (strcmp(s, "-") == 0)
		return RIG_RPT_SHIFT_MINUS;
	else
		return RIG_RPT_SHIFT_NONE;
}

