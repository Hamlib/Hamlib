/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ftdx101.h - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *             (C) Stephane Fillod 2008-2010
 *             (C) Mikael Nousiainen 2020
 *
 * This shared library provides an API for communicating
 * via serial interface to an FTDX101(D/MP) using the "CAT" interface
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


#ifndef _FTDX101_H
#define _FTDX101_H 1

#define FTDX101_VFO_ALL (RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM)

/* Receiver caps */

#define FTDX101_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_AMN|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
        RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|\
        RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_PKTFMN)

#define FTDX101_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
        RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)
#define FTDX101_AM_RX_MODES (RIG_MODE_AM|RIG_MODE_AMN)
#define FTDX101_FM_RX_MODES (RIG_MODE_FM|RIG_MODE_PKTFM|RIG_MODE_FMN|RIG_MODE_PKTFMN)
#define FTDX101_CW_RTTY_PKT_RX_MODES (RIG_MODE_RTTY|RIG_MODE_RTTYR|\
        RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_CW|RIG_MODE_CWR)

/* TRX caps */

#define FTDX101_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_AMN|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY| \
        RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_PKTFMN) /* 100 W class */
#define FTDX101_AM_TX_MODES (RIG_MODE_AM|RIG_MODE_AMN)    /* set 25W max */

#define FTDX101_LEVELS (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|\
               RIG_LEVEL_ALC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_SWR|\
               RIG_LEVEL_RFPOWER|RIG_LEVEL_RF|RIG_LEVEL_SQL|\
               RIG_LEVEL_MICGAIN|RIG_LEVEL_IF|RIG_LEVEL_CWPITCH|\
               RIG_LEVEL_KEYSPD|RIG_LEVEL_AF|RIG_LEVEL_AGC|\
               RIG_LEVEL_METER|RIG_LEVEL_BKINDL|RIG_LEVEL_SQL|\
               RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_COMP|\
               RIG_LEVEL_ANTIVOX|RIG_LEVEL_NR|RIG_LEVEL_NB|RIG_LEVEL_NOTCHF|\
               RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|\
               RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_TEMP_METER|\
               RIG_LEVEL_BAND_SELECT)

#define FTDX101_FUNCS (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_LOCK|\
               RIG_FUNC_MON|RIG_FUNC_NB|RIG_FUNC_NR|RIG_FUNC_VOX|\
               RIG_FUNC_FBKIN|RIG_FUNC_COMP|RIG_FUNC_ANF|RIG_FUNC_MN|\
               RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_TUNER|RIG_FUNC_APF)

/* TBC */
#define FTDX101_VFO_OPS (RIG_OP_TUNE|RIG_OP_CPY|RIG_OP_XCHG|\
               RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|\
               RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_TOGGLE)

#define FTDX101D_RFPOWER_METER_CAL \
    { \
        6, \
        { \
            {0,   0.0f}, \
            {38,  0.5f}, \
            {94,  0.25f}, \
            {147, 0.50f}, \
            {176, 0.75f}, \
            {205, 1.0f}, \
        } \
    }

#define FTDX101D_RFPOWER_METER_WATTS_CAL \
    { \
        6, \
        { \
            {0,   0.0f}, \
            {38,  5.0f}, \
            {94,  25.0f}, \
            {147, 50.0f}, \
            {176, 75.0f}, \
            {205, 100.0f}, \
        } \
    }

#define FTDX101MP_RFPOWER_METER_CAL \
    { \
        11, \
        { \
            {0,   0.00f}, \
            {69,  0.05f}, \
            {111, 0.10f}, \
            {129, 0.15f}, \
            {143, 0.20f}, \
            {158, 0.25f}, \
            {184, 0.35f}, \
            {200, 0.40f}, \
            {211, 0.45f}, \
            {222, 0.50f}, \
            {255, 1.0f}, \
        } \
    }
#define FTDX101MP_RFPOWER_METER_WATTS_CAL \
    { \
        13, \
        { \
            {0,    0.0f}, \
            {30,   5.0f}, \
            {69,  20.0f}, \
            {98,  40.0f}, \
            {119, 60.0f}, \
            {139, 80.0f}, \
            {160, 100.0f}, \
            {173, 120.0f}, \
            {185, 140.0f}, \
            {198, 160.0f}, \
            {210, 180.0f}, \
            {225, 200.0f}, \
            {255, 210.0f}, \
        } \
    }

// Based on testing with G3VPX Ian Sumner
#define FTDX101D_SWR_CAL \
    { \
        8, \
        { \
            {0, 1.0f}, \
            {26, 1.2f}, \
            {52, 1.5f}, \
            {89, 2.0f}, \
            {126, 3.0f}, \
            {173, 4.0f}, \
            {236, 5.0f}, \
            {255, 25.0f}, \
        } \
    }

#define FTDX101D_STR_CAL { 12, \
                { \
                    { 0,  -60 }, /* S0 */ \
                    { 17, -54 }, /* S0 */ \
                    { 25, -48 }, \
                    { 34, -42 }, \
                    { 51, -36 }, \
                    { 68, -30 }, \
                    { 85, -24 }, \
                    { 102, -18 }, \
                    { 119, -12 }, \
                    { 136, -6 }, \
                    { 160,  0 },  /* S9 */ \
                    { 255,  60 }, /* +60 */ \
                } }


/*
 * Other features (used by rig_caps)
 */

#define FTDX101_TX_ANTS  (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)

#define FTDX101_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FTDX101_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FTDX101_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FTDX101_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FTDX101_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FTDX101_ALL_DATA_LENGTH           649     /* 0x10 P1 = 00 return size */

/* Timing values in mS */

/* Delay between bytes sent
 * Should not exceed value set in CAT TOT menu (rig default is 10 mSec)
 */
#define FTDX101_WRITE_DELAY                    0


/* Delay sequential fast writes */

#define FTDX101_POST_WRITE_DELAY               5

#endif /* _FTDX101_H */
