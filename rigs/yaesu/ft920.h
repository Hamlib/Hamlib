/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft920.h - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *           (C) Nate Bargmann 2002, 2003, 2007 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2002 (fillods at users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-920 using the "CAT" interface
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


#ifndef _FT920_H
#define _FT920_H    1

#define TRUE        1
#define FALSE       0

#define FT920_VFO_ALL   (RIG_VFO_A|RIG_VFO_B)

/* Receiver caps */
#define FT920_ALL_RX_MODES   (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_FM|RIG_MODE_WFM)
#define FT920_SSB_CW_RX_MODES   (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT920_AM_RX_MODES       (RIG_MODE_AM)
#define FT920_FM_RX_MODES       (RIG_MODE_FM|RIG_MODE_WFM)


/* TX caps */
#define FT920_OTHER_TX_MODES    (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM )  /* 100 W class */
#define FT920_AM_TX_MODES       (RIG_MODE_AM )                              /* set 25W max */


/* Other features */
#define FT920_ANTS  0   /* FIXME: declare Ant A & B and RX input */
#define FT920_FUNC_ALL  (RIG_FUNC_TUNER | RIG_FUNC_LOCK) /* fix */

/* Returned data length in bytes */
#define FT920_MEM_CHNL_LENGTH       1   /* 0x10 P1 = 01 return size */
#define FT920_STATUS_FLAGS_LENGTH   8   /* 0xfa return size */
#define FT920_VFO_DATA_LENGTH       28  /* 0x10 P1 = 02, 03 return size */
#define FT920_MEM_CHNL_DATA_LENGTH  14  /* 0x10 P1 = 04, P4 = 0x00-0x89 return size */


/* Delay sequential fast writes
 *
 * It is thought that it takes the rig about 60 mS to process a command
 * so a default of 80 mS should be long enough to prevent commands from
 * stacking up.
 */
#define FT920_POST_WRITE_DELAY      80

/*
 * 8N2 and 1 start bit = 11 bits at 4800 bps => effective byte rate = 1 byte
 * in 2.2917 msec => 28 bytes in 64 msec
 *
 * delay for 28 bytes = (2.2917 + pace_interval) * 28
 *
 * pace_interval    time to read 28 bytes
 * -------------    ---------------------
 *
 *      0               64 msec
 *      1               92 msec
 *      2               120 msec
 *      5               204 msec
 *      255             7.2 sec
 *
 */

/* Timing values in mS */
#define FT920_PACING_DEFAULT_VALUE  0   /* time between characters from 920 */
#define FT920_WRITE_DELAY           0   /* time between characters to 920 */

/* Rough safe value for default timeout */
#define FT920_DEFAULT_READ_TIMEOUT  28 * ( 5 + FT920_PACING_DEFAULT_VALUE)


/* BCD coded frequency length */
#define FT920_BCD_DIAL  8
#define FT920_BCD_RIT   3


/*
 * Native FT920 functions. More to come :-)
 *
 */

enum ft920_native_cmd_e {
    FT920_NATIVE_SPLIT_OFF = 0,
    FT920_NATIVE_SPLIT_ON,
    FT920_NATIVE_RECALL_MEM,
    FT920_NATIVE_VFO_TO_MEM,
    FT920_NATIVE_VFO_A,
    FT920_NATIVE_VFO_B,
    FT920_NATIVE_MEM_TO_VFO,
    FT920_NATIVE_CLARIFIER_OPS,
    FT920_NATIVE_VFO_A_FREQ_SET,
    FT920_NATIVE_MODE_SET,
    FT920_NATIVE_PACING,
    FT920_NATIVE_PTT_OFF,
    FT920_NATIVE_PTT_ON,
    FT920_NATIVE_MEM_CHNL,
    FT920_NATIVE_OP_DATA,
    FT920_NATIVE_VFO_DATA,
    FT920_NATIVE_MEM_CHNL_DATA,
    FT920_NATIVE_TUNER_BYPASS,
    FT920_NATIVE_TUNER_INLINE,
    FT920_NATIVE_TUNER_START,
    FT920_NATIVE_VFO_B_FREQ_SET,
    FT920_NATIVE_VFO_A_PASSBAND_WIDE,
    FT920_NATIVE_VFO_A_PASSBAND_NAR,
    FT920_NATIVE_VFO_B_PASSBAND_WIDE,
    FT920_NATIVE_VFO_B_PASSBAND_NAR,
    FT920_NATIVE_STATUS_FLAGS,
    FT920_NATIVE_SIZE   /* end marker, value indicates number of */
                        /* native cmd entries */
};

typedef enum ft920_native_cmd_e ft920_native_cmd_t;


/*
 * Internal MODES - when setting modes via FT920_NATIVE_MODE_SET
 *
 */

/* VFO A */
#define MODE_SET_A_LSB      0x00
#define MODE_SET_A_USB      0x01
#define MODE_SET_A_CW_U     0x02
#define MODE_SET_A_CW_L     0x03
#define MODE_SET_A_AM_W     0x04
#define MODE_SET_A_AM_N     0x05
#define MODE_SET_A_FM_W     0x06
#define MODE_SET_A_FM_N     0x07
#define MODE_SET_A_DATA_L   0x08
#define MODE_SET_A_DATA_U   0x0a
#define MODE_SET_A_DATA_F   0x0b

/* VFO B */
#define MODE_SET_B_LSB      0x80
#define MODE_SET_B_USB      0x81
#define MODE_SET_B_CW_U     0x82
#define MODE_SET_B_CW_L     0x83
#define MODE_SET_B_AM_W     0x84
#define MODE_SET_B_AM_N     0x85
#define MODE_SET_B_FM_W     0x86
#define MODE_SET_B_FM_N     0x87
#define MODE_SET_B_DATA_L   0x88
#define MODE_SET_B_DATA_U   0x8a
#define MODE_SET_B_DATA_F   0x8b


/*
 * Internal Clarifier parms - when setting clarifier via
 * FT920_NATIVE_CLARIFIER_OPS
 *
 */

/* P1 values */
#define CLAR_RX_OFF     0x00
#define CLAR_RX_ON      0x01
#define CLAR_TX_OFF     0x80
#define CLAR_TX_ON      0x81
#define CLAR_SET_FREQ   0xff

/* P2 values */
#define CLAR_OFFSET_PLUS    0x00
#define CLAR_OFFSET_MINUS   0xff


/* Tuner status values used to set the
 * tuner state and indicate tuner status.
 */
#define TUNER_BYPASS    0
#define TUNER_INLINE    1
#define TUNER_TUNING    2

/*
 * Local VFO CMD's, according to spec
 *
 */

//#define FT920_VFO_A   0x00
//#define FT920_VFO_B   0x01


/*
 * Some useful offsets in the status update flags (offset)
 * SUMO--Status Update Memory Offset?
 *
 * SF_ bit tests are now grouped with flag bytes for ease of reference
 *
 * FIXME: complete flags and bits
 *
 * CAT command 0xFA, P1 = 01 requests the FT-920 to return its status flags.
 * These flags consist of 8 bytes and are documented in the FT-920 manual
 * on page 89.
 *
 */

#define FT920_SUMO_DISPLAYED_STATUS_0   0x00    /* Status flag byte 0 */
#define SF_VFOA     0x00    /* bits 0 & 1, VFO A TX/RX == 0 */
#define SF_SPLITA   (1<<0)  /* Split operation with VFO-B on TX */
#define SF_SPLITB   (1<<1)  /* Split operation with VFO-B on RX */
#define SF_VFOB     (SF_SPLITA|SF_SPLITB)   /* bits 0 & 1, VFO B TX/RX  == 3 */
#define SF_TUNER_TUNE   (1<<2)  /* Antenna tuner On and Tuning for match*/
#define SF_PTT_OFF  (0<<7)  /* Receive mode (PTT line open) */
#define SF_PTT_ON   (1<<7)  /* Transmission in progress (PTT line grounded) */
#define SF_PTT_MASK (SF_PTT_ON)

#define FT920_SUMO_DISPLAYED_STATUS_1   0x01    /* Status flag byte 1 */
#define SF_QMB      (1<<3)  /* Quick Memory Bank (QMB) selected */
#define SF_MT       (1<<4)  /* Memory Tuning in progress */
#define SF_VFO      (1<<5)  /* VFO operation selected */
#define SF_MR       (1<<6)  /* Memory Mode selected */
#define SF_GC       (1<<7)  /* General Coverage Reception selected */
#define SF_VFO_MASK (SF_QMB|SF_MT|SF_VFO|SF_MR)

#define FT920_SUMO_DISPLAYED_STATUS_2   0x02    /* Status flag byte 2 */
#define SF_TUNER_INLINE (1<<1)  /* Antenna tuner is inline or bypass */
#define SF_VFOB_LOCK    (1<<2)  /* VFO B tuning lock status */
#define SF_VFOA_LOCK    (1<<3)  /* VFO A tuning lock status */

/*
 * Offsets for VFO record retrieved via 0x10 P1 = 02, 03
 *
 * The FT-920 returns frequency and mode data via three seperate commands.
 * CAT command 0x10, P1 = 02 returns the current main and sub displays' data (28 bytes)
 * CAT command 0x10, P1 = 03 returns VFO A data and the sub display data (sub display is always VFO B) (28 bytes)
 * CAT command 0x10, P1 = 04, P4 = 0x00-0x89 returns memory channel data (14 bytes)
 * In all cases the format is (from the FT-920 manual page 90):
 *
 * Offset   Value
 * 0x00     Band Selection      (not documented!)
 * 0x01     Operating Frequency (Hex value of display--Not BCD!)
 * 0x05     Clarifier Offset    (Hex value)
 * 0x07     Mode Data
 * 0x08     Flag
 * 0x09     Filter Data 1
 * 0x0a     Filter Data 2
 * 0x0b     CTCSS Encoder Data
 * 0x0c     CTCSS Decoder Data
 * 0x0d     Memory recall Flag
 *
 * Memory Channel data has the same layout and offsets
 * VFO B data has the same layout, but the offset starts at 0x0e and
 * continues through 0x1b
 *
 */

#define FT920_SUMO_DISPLAYED_FREQ   0x01    /* Current main display, can be VFO A, Memory data, Memory tune */
#define FT920_SUMO_VFO_A_FREQ       0x01    /* VFO A frequency, not necessarily currently displayed! */
#define FT920_SUMO_DISPLAYED_CLAR   0x05    /* RIT/XIT offset -- current display */
#define FT920_SUMO_VFO_A_CLAR       0x05    /* RIT/XIT offset -- VFO A */
#define FT920_SUMO_DISPLAYED_MODE   0x07    /* Current main display mode */
#define FT920_SUMO_VFO_A_MODE       0x07    /* VFO A mode, not necessarily currently displayed! */
#define FT920_SUMO_VFO_B_FREQ       0x0f    /* Current sub display && VFO B */
#define FT920_SUMO_VFO_B_CLAR       0x13    /* RIT/XIT offset -- VFO B */
#define FT920_SUMO_VFO_B_MODE       0x15    /* Current sub display && VFO B */

/*
 * Mode Bitmap from offset 0x07 or 0x16 in VFO Record.
 * Bits 5 and 6 ignored
 * used when READING modes from FT-920
 *
 */

#define MODE_LSB        0x00
#define MODE_CW_L       0x01    /* CW listening on LSB */
#define MODE_AM         0x02
#define MODE_FM         0x03
#define MODE_DATA_L     0x04    /* DATA on LSB */
#define MODE_DATA_U     0x05    /* DATA on USB (who does that? :) */
#define MODE_DATA_F     0x06    /* DATA on FM */
#define MODE_USB        0x40
#define MODE_CW_U       0x41    /* CW listening on USB */
/* Narrow filter selected */
#define MODE_LSBN       0x80    /* Not sure this actually exists */
#define MODE_CW_LN      0x81
#define MODE_AMN        0x82
#define MODE_FMN        0x83
#define MODE_DATA_LN    0x84
#define MODE_DATA_UN    0x85
#define MODE_DATA_FN    0x86
#define MODE_USBN       0xc0    /* Not sure this actually exists */
#define MODE_CW_UN      0xc1

/* All relevent bits */
#define MODE_MASK       0xc7


/*
 * Command string parameter offsets
 */

#define P1  3
#define P2  2
#define P3  1
#define P4  0


/*
 * API local implementation
 *
 */

static int ft920_init(RIG *rig);
static int ft920_cleanup(RIG *rig);
static int ft920_open(RIG *rig);
static int ft920_close(RIG *rig);

static int ft920_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft920_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft920_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft920_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft920_set_vfo(RIG *rig, vfo_t vfo);
static int ft920_get_vfo(RIG *rig, vfo_t *vfo);

static int ft920_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int ft920_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);

static int ft920_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int ft920_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);

static int ft920_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width);
static int ft920_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width);

static int ft920_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft920_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);

static int ft920_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
static int ft920_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);

/* not documented in my FT-920 manual, but it works! - N0NB */
static int ft920_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft920_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);

static int ft920_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft920_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);

#endif /* _FT920_H */
