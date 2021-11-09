/*
 *  Hamlib Elecraft backend--support extensions to Kenwood commands
 *  Copyright (C) 2010,2011 by Nate Bargmann, n0nb@n0nb.us
 *
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

#ifndef _ELECRAFT_H
#define _ELECRAFT_H 1

#include <hamlib/rig.h>

/* The Elecraft Programmer's Reference details the extension level that
 * a K2 or K3 may have in effect which modify certain commands.
 */

enum elec_ext_id_e {
	K20 = 0,	/* K2 Normal mode */
	K21,		/* K2 Normal/rtty_off */
	K22,		/* K2 Extended mode */
	K23,		/* K2 Extended mode/rtty_off */
	K30,		/* K3 Normal mode */
	K31,		/* K3 Extended mode */
	XG3,		/* XG3 */
	EXT_LEVEL_NONE
};

struct elec_ext_id_str {
	enum elec_ext_id_e level;
	const char *id;
};

/* Data sub-modes are provided from the K3 via the DT command */
enum k3_data_submodes_e {
	K3_MODE_DATA_A = 0,	/* DT0; */
	K3_MODE_AFSK_A,		/* DT1; */
	K3_MODE_FSK_D,		/* DT2; */
	K3_MODE_PSK_D		/* DT3; */
};


/* Private tokens used for ext_lvl function in Elecraft backends.
 * Extra levels which are rig specific should be coded in
 * the individual rig files and token #s >= 101.
 *
 * See Private Elecraft extra levels definitions in elecraft.c
 */
#define TOK_IF_FREQ    TOKEN_BACKEND(101)	/* K3 FI command - IF center frequency (K3/K3S only) */
#define TOK_TX_STAT    TOKEN_BACKEND(102)	/* K3 TQ command - transmit query (K3/K3S/KX3/KX2) */
#define TOK_RIT_CLR    TOKEN_BACKEND(103)	/* K3 RC command - RIT/XIT clear (K3/K3S/KX3/KX2) */
#define TOK_ESSB       TOKEN_BACKEND(104)	/* K3 ES command - ESSB mode (K3/K3S/KX3/KX2) */
#define TOK_RX_ANT     TOKEN_BACKEND(105)	/* K3 AR command - RX antenna on/off (K3/K3S only) */
#define TOK_LINK_VFOS  TOKEN_BACKEND(106)	/* K3 LN command - link VFOs on/off (K3/K3S only) */
#define TOK_TX_METER   TOKEN_BACKEND(107)	/* K3 TM command - Transmit meter mode, SWR/ALC (K3/K3S only) */
#define TOK_IF_NB      TOKEN_BACKEND(108)	/* K3 NL command - IF noise blanker level (K3/K3S only) */

/* Token structure assigned to .cfgparams in rig_caps */
extern const struct confparams elecraft_ext_levels[];


/* Elecraft extension function declarations */
int elecraft_open(RIG *rig);

/* S-meter calibration tables */

/* K3 defines 16 values--0-15.
 * Table is RASTR value from SM command and dB relative to S9 == 0
 * (see rig_get_level() in src/settings.c
 */
#define K3_SM_CAL { 16, \
	{ \
		{ 0, -54 }, \
		{ 1, -42 }, \
		{ 2, -36 }, \
		{ 3, -24 }, \
		{ 4, -12 }, \
		{ 5, -6 }, \
		{ 6, 0 }, \
		{ 7, 10 }, \
		{ 8, 15 }, \
		{ 9, 20 }, \
		{ 10, 30 }, \
		{ 11, 35 }, \
		{ 12, 40 }, \
		{ 13, 50 }, \
		{ 14, 55 }, \
		{ 15, 60 }, \
	} }


/* K3 defines 100 values--0-100 in high resolution mode.
 * Table is RASTR value from SMH command and dB relative to S9 == 0
 * (see rig_get_level() in src/settings.c
 */
#define K3_SMH_CAL { 22, \
	{ \
		{ 0, -54 }, \
		{ 5, -48 }, \
		{ 9, -42 }, \
		{ 14, -36 }, \
		{ 22, -30 }, \
		{ 24, -24 }, \
		{ 28, -18 }, \
		{ 33, -12 }, \
		{ 38, -6 }, \
		{ 42, 0 }, \
		{ 47, 5 }, \
		{ 53, 10 }, \
		{ 58, 15 }, \
		{ 63, 20 }, \
		{ 68, 25 }, \
		{ 73, 30 }, \
		{ 78, 35 }, \
		{ 83, 40 }, \
		{ 88, 45 }, \
		{ 93, 50 }, \
		{ 98, 55 }, \
		{ 103, 60 }, \
	} }

// K4 is the only we know that has this as of 2021-11-09
int elecraft_get_vfo_tq(RIG *rig, vfo_t *vfo);

#endif /* _ELECRAFT_H */
