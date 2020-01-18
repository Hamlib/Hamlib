/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft2000.h - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2008
 *
 * ft1200.h - (C) David Fannin 2015 (kk6df at arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-1200 using the "CAT" interface
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


#ifndef _FT1200_H
#define _FT1200_H 1

#if 0
#define TRUE    1
#define FALSE   0
#define ON      TRUE
#define OFF     FALSE
#endif

#define FT1200_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

/* Receiver caps */

#define FT1200_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
				RIG_MODE_RTTY|RIG_MODE_RTTYR|\
		                RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM)

#define FT1200_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
		RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)
#define FT1200_AM_RX_MODES (RIG_MODE_AM)
#define FT1200_FM_RX_MODES (RIG_MODE_FM|RIG_MODE_PKTFM)
#define FT1200_CW_RTTY_PKT_RX_MODES (RIG_MODE_RTTY|RIG_MODE_RTTYR|\
        RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_CW|RIG_MODE_CWR)




/* TRX caps */

#define FT1200_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY| \
		                RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM) /* 100 W class */
#define FT1200_AM_TX_MODES (RIG_MODE_AM)    /* set 25W max */

/* TBC */
#define FT1200_LEVELS (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|\
               RIG_LEVEL_ALC|RIG_LEVEL_RAWSTR|RIG_LEVEL_SWR|\
               RIG_LEVEL_RFPOWER|RIG_LEVEL_RF|RIG_LEVEL_SQL|\
               RIG_LEVEL_MICGAIN|RIG_LEVEL_IF|RIG_LEVEL_CWPITCH|\
               RIG_LEVEL_KEYSPD|RIG_LEVEL_AF|RIG_LEVEL_AGC|\
               RIG_LEVEL_METER|RIG_LEVEL_BKINDL|RIG_LEVEL_SQL|\
               RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_COMP|\
               RIG_LEVEL_ANTIVOX|RIG_LEVEL_NR|RIG_LEVEL_NOTCHF)

/* TBC */
#define FT1200_FUNCS (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_LOCK|\
               RIG_FUNC_MON|RIG_FUNC_NB|RIG_FUNC_NR|RIG_FUNC_VOX|\
               RIG_FUNC_FBKIN|RIG_FUNC_COMP|RIG_FUNC_ANF|RIG_FUNC_MN|\
               RIG_FUNC_TUNER)

/* TBC */
#define FT1200_VFO_OPS (RIG_OP_TUNE|RIG_OP_CPY|RIG_OP_XCHG|\
               RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|\
               RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_TOGGLE)

/* TBC */
#define FT1200_STR_CAL { 16, \
	       { \
			{   0, -54 }, /*  S0 */ \
			{  12, -48 }, /*  S1 */ \
			{  27, -42 }, /*  S2 */ \
			{  40, -36 }, /*  S3 */ \
			{  55, -30 }, /*  S4 */ \
			{  65, -24 }, /*  S5 */ \
			{  80, -18 }, /*  S6 */ \
			{  95, -12 }, /*  S7 */ \
			{ 112,  -6 }, /*  S8 */ \
			{ 130,   0 }, /*  S9 */ \
			{ 150,  10 }, /* +10 */ \
			{ 172,  20 }, /* +20 */ \
			{ 190,  30 }, /* +30 */ \
			{ 220,  40 }, /* +40 */ \
			{ 240,  50 }, /* +50 */ \
			{ 255,  60 }, /* +60 */ \
		} }


/*
 * Other features (used by rig_caps)
 *
 */

#define FT1200_TX_ANTS  (RIG_ANT_1|RIG_ANT_2)

#define FT1200_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT1200_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FT1200_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT1200_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FT1200_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FT1200_ALL_DATA_LENGTH           649     /* 0x10 P1 = 00 return size */

/* Timing values in mS */

// #define FT2000_PACING_INTERVAL                5
// #define FT2000_PACING_DEFAULT_VALUE           0

/* Delay between bytes sent to FT-2000
 * Should not exceed value set in CAT TOT menu (rig default is 10 mSec)
 */
#define FT1200_WRITE_DELAY                    0


/* Delay sequential fast writes */

#define FT1200_POST_WRITE_DELAY               5

#endif /* _FT1200_H */
