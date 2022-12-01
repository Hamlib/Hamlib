/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft2000.h - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2008
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-2000 using the "CAT" interface
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


#ifndef _FT2000_H
#define _FT2000_H 1

#define FT2000_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

/* Receiver caps */

#define FT2000_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_AMN|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
				RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|\
				RIG_MODE_FM|RIG_MODE_FMN)

#define FT2000_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
		RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)
#define FT2000_AM_RX_MODES (RIG_MODE_AM|RIG_MODE_AMN)
#define FT2000_FM_RX_MODES (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_PKTFM)
#define FT2000_PKTSSB_RX_MODES (RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)

/* TRX caps */

#define FT2000_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY| \
		                RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|\
		                RIG_MODE_FM|RIG_MODE_FMN) /* 100 W class */
#define FT2000_AM_TX_MODES (RIG_MODE_AM|RIG_MODE_AMN)    /* set 25W max */

#define FT2000_LEVELS (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|\
               RIG_LEVEL_ALC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_SWR|\
               RIG_LEVEL_RFPOWER|RIG_LEVEL_RF|RIG_LEVEL_SQL|\
               RIG_LEVEL_MICGAIN|RIG_LEVEL_IF|RIG_LEVEL_CWPITCH|\
               RIG_LEVEL_KEYSPD|RIG_LEVEL_AF|RIG_LEVEL_AGC|\
               RIG_LEVEL_METER|RIG_LEVEL_BKINDL|RIG_LEVEL_SQL|\
               RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_COMP|\
               RIG_LEVEL_ANTIVOX|RIG_LEVEL_NR|RIG_LEVEL_NOTCHF|\
               RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|\
               RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|\
               RIG_LEVEL_BAND_SELECT)

#define FT2000_FUNCS (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_LOCK|\
               RIG_FUNC_MON|RIG_FUNC_NB|RIG_FUNC_NB2|RIG_FUNC_NR|RIG_FUNC_VOX|\
               RIG_FUNC_FBKIN|RIG_FUNC_COMP|RIG_FUNC_ANF|RIG_FUNC_MN|\
               RIG_FUNC_RIT|RIG_FUNC_XIT|\
               RIG_FUNC_TUNER|RIG_FUNC_APF)

#define FT2000_VFO_OPS (RIG_OP_TUNE|RIG_OP_CPY|RIG_OP_XCHG|\
               RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|\
               RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_TOGGLE)

// From measurements on FT2000D by Stan UA3SAQ
// We'll reuse this for now and divide by 2 for FT2000
#define FT2000D_RFPOWER_METER_CAL { 12, \
        { \
            {0, 0.0f}, \
            {51, 10.0f}, \
            {67, 20.0f}, \
            {97, 40.0f}, \
            {123, 60.0f}, \
            {140, 80.0f}, \
            {161, 100.0f}, \
            {168, 120.0f}, \
            {184, 140.0f}, \
            {195, 160.0f}, \
            {215, 180.0f}, \
            {224, 200.0f}, \
        } \
    }

#define FT2000_STR_CAL { 16, \
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

#define FT2000_TX_ANTS  (RIG_ANT_1|RIG_ANT_2)

#define FT2000_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT2000_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FT2000_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT2000_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FT2000_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FT2000_ALL_DATA_LENGTH           649     /* 0x10 P1 = 00 return size */

/* Timing values in mS */

// #define FT2000_PACING_INTERVAL                5
// #define FT2000_PACING_DEFAULT_VALUE           0

/* Delay between bytes sent to FT-2000
 * Should not exceed value set in CAT TOT menu (rig default is 10 mSec)
 */
#define FT2000_WRITE_DELAY                    0


/* Delay sequential fast writes */

#define FT2000_POST_WRITE_DELAY               5

#endif /* _FT2000_H */
