/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ftx1.h - (C) Jeremy Miller KO4SSD 2025 (ko4ssd at ko4ssd.com)
 *
 * This shared library provides an API for communicating
 * via USB interface to an FTX-1 using the "CAT" interface
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


#ifndef _FTX1_H
#define _FTX1_H 1

#define FTX1_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

/* Receiver caps */

#define FTX1_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
        RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|\
        RIG_MODE_C4FM|RIG_MODE_FM|RIG_MODE_AMN|RIG_MODE_FMN)
#define FTX1_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
        RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)
#define FTX1_AM_RX_MODES (RIG_MODE_AM|RIG_MODE_AMN)
#define FTX1_FM_WIDE_RX_MODES (RIG_MODE_FM|RIG_MODE_PKTFM|RIG_MODE_C4FM)
#define FTX1_FM_RX_MODES (FTX1_FM_WIDE_RX_MODES|RIG_MODE_FMN)
#define FTX1_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR)
#define FTX1_RTTY_DATA_RX_MODES (RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB)

/* TRX caps */

#define FTX1_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|\
        RIG_MODE_FM|RIG_MODE_PKTFM|RIG_MODE_FMN) /* 100 W class */
#define FTX1_AM_TX_MODES (RIG_MODE_AM|RIG_MODE_AMN)    /* set 25W max */

#define FTX1_LEVELS (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_STRENGTH|\
               RIG_LEVEL_ALC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_SWR|\
               RIG_LEVEL_RFPOWER|RIG_LEVEL_RF|RIG_LEVEL_SQL|\
               RIG_LEVEL_MICGAIN|RIG_LEVEL_IF|RIG_LEVEL_CWPITCH|\
               RIG_LEVEL_KEYSPD|RIG_LEVEL_AF|RIG_LEVEL_AGC|\
               RIG_LEVEL_METER|RIG_LEVEL_BKINDL|RIG_LEVEL_BKIN_DLYMS|RIG_LEVEL_SQL|\
               RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_COMP|\
               RIG_LEVEL_ANTIVOX|RIG_LEVEL_NR|RIG_LEVEL_NB|RIG_LEVEL_NOTCHF|\
               RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|\
               RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|\
               RIG_LEVEL_BAND_SELECT)

#define FTX1_FUNCS (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_CSQL|RIG_FUNC_LOCK|\
               RIG_FUNC_MON|RIG_FUNC_NB|RIG_FUNC_NR|RIG_FUNC_VOX|\
               RIG_FUNC_FBKIN|RIG_FUNC_COMP|RIG_FUNC_ANF|RIG_FUNC_MN|\
               RIG_FUNC_RIT|RIG_FUNC_XIT|\
               RIG_FUNC_TUNER|RIG_FUNC_APF|RIG_FUNC_MUTE)

#define FTX1_VFO_OPS (RIG_OP_TUNE|RIG_OP_CPY|RIG_OP_XCHG|\
               RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|\
               RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_MCL)

// Borrowed from FLRig -- Thanks to Dave W1HKJ
#define FTX1_RFPOWER_METER_CAL \
    { \
        7, \
        { \
            {0, 0.0f}, \
            {10, 0.8f}, \
            {50, 8.0f}, \
            {100, 26.0f}, \
            {150, 54.0f}, \
            {200, 92.0f}, \
            {250, 140.0f}, \
        } \
    }

/* TBC */
#define FTX1_STR_CAL { 16, \
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


#define FTX1_ID_CAL { 7, \
    { \
        {   0, 0.0f }, \
        {  53, 5.0f }, \
        {  65, 6.0f }, \
        {  78, 7.0f }, \
        {  86, 8.0f }, \
        {  98, 9.0f }, \
        { 107, 10.0f } \
    } \
}

/* TBC */
#define FTX1_VD_CAL { 2, \
    { \
        {   0, 0.0f  }, \
        { 192, 13.8f }, \
    } \
}

#define FTX1_COMP_CAL { 9, \
    { \
        { 0,   0.0f  }, \
        { 40,  2.5f  }, \
        { 60,  5.0f  }, \
        { 85,  7.5f  }, \
        { 135, 10.0f }, \
        { 150, 12.5f }, \
        { 175, 15.0f }, \
        { 195, 17.5f }, \
        { 220, 20.0f } \
    } \
}

/*
 * Other features (used by rig_caps)
 *
 */

// The FTX1 does not have antenna selection
#define FTX1_ANTS  (RIG_ANT_CURR)

#define FTX1_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FTX1_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FTX1_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FTX1_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FTX1_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FTX1_ALL_DATA_LENGTH           649     /* 0x10 P1 = 00 return size */

/* Timing values in mS */

// #define FTX1_PACING_INTERVAL                5
// #define FTX1_PACING_DEFAULT_VALUE           0

/* Delay between bytes sent to FTX-1
 * Should not exceed value set in CAT TOT menu (rig default is 10 mSec)
 */
#define FTX1_WRITE_DELAY                    5


/* Delay sequential fast writes */

#define FTX1_POST_WRITE_DELAY               5

typedef struct
{
    char command[2];      /* depends on command "IF", "MR", "MW" "OI" */
    char memory_ch[3];    /* 001 -> 117 */
    char vfo_freq[9];     /* 9 digit value in Hz */
    char clarifier[5];    /* '+' | '-', 0000 -> 9999 Hz */
    char rx_clarifier;    /* '0' = off, '1' = on */
    char tx_clarifier;    /* '0' = off, '1' = on */
    char mode;            /* '1'=LSB, '2'=USB, '3'=CW, '4'=FM, '5'=AM, */
    /* '6'=RTTY-LSB, '7'=CW-R, '8'=DATA-LSB, */
    /* '9'=RTTY-USB,'A'=DATA-FM, 'B'=FM-N, */
    /* 'C'=DATA-USB, 'D'=AM-N, 'E'=C4FM */
    char vfo_memory;      /* '0'=VFO, '1'=Memory, '2'=Memory Tune, */
    /* '3'=Quick Memory Bank, '4'=QMB-MT, '5'=PMS, '6'=HOME */
    char tone_mode;       /* '0' = off, CTCSS '1' ENC, '2' ENC/DEC, */
    /* '3' = DCS ENC/DEC, '4' = ENC */
    char fixed[2];        /* Always '0', '0' */
    char repeater_offset; /* '0' = Simplex, '1' Plus, '2' minus */
    char terminator;      /* ';' */
} ftx1info;

/* Function prototypes for new commands */
static int ftx1_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
static int ftx1_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ftx1_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);
static int ftx1_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
static int ftx1_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
static int ftx1_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);

/* FTX-1 specific power control function */
int ftx1_set_power_control(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ftx1_get_power_control(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

/* FTX-1 specific frequency setting function */
int ftx1_set_freq(RIG *rig, vfo_t vfo, freq_t freq);

#endif /* _FTX1_H */ 