/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft900.h - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *           (C) Stephane Fillod 2002, 2003 (fillods at users.sourceforge.net)
 *           (C) Nate Bargmann 2002, 2003 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-900 using the "CAT" interface
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


#ifndef _FT900_H
#define _FT900_H 1

#define TRUE    1
#define FALSE   0
#define ON      TRUE
#define OFF     FALSE

#define FT900_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

/* Receiver caps */

#define FT900_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT900_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT900_AM_RX_MODES (RIG_MODE_AM)
#define FT900_FM_RX_MODES (RIG_MODE_FM)


/* TX caps */

#define FT900_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB ) /* 100 W class */
#define FT900_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */
#define FT900_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN) /* fix */


/*
 * Other features (used by rig_caps)
 *
 */

#define FT900_ANTS 0

/* Returned data length in bytes */

#define FT900_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT900_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FT900_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT900_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FT900_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FT900_ALL_DATA_LENGTH           1941    /* 0x10 P1 = 00 return size */

/* Timing values in mS */

#define FT900_PACING_INTERVAL                5
#define FT900_PACING_DEFAULT_VALUE           0
#define FT900_WRITE_DELAY                    5


/* Delay sequential fast writes */

#define FT900_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT900_DEFAULT_READ_TIMEOUT  1941 * ( 5 + (FT900_PACING_INTERVAL * FT900_PACING_DEFAULT_VALUE))

/* BCD coded frequency length */
#define FT900_BCD_DIAL  8
#define FT900_BCD_RIT   3


/*
 * 8N2 and 1 start bit = 11 bits at 4800 bps => effective byte
 * rate = 1 byte in 2.2917 msec => 1941 bytes in 4448 msec
 *
 * delay for 28 bytes = (2.2917 + pace_interval) * 28
 *
 * pace_interval          time to read 1941 bytes
 * ------------           ----------------------
 *
 *     0                       4448 msec    (backend default)
 *     1                       6389 msec
 *     2                       8330 msec
 *     5                       14153 msec
 *    255                      499.4 sec
 *
 */


/*
 * Native FT900 functions. More to come :-)
 *
 */

enum ft900_native_cmd_e {
  FT900_NATIVE_SPLIT_OFF = 0,
  FT900_NATIVE_SPLIT_ON,
  FT900_NATIVE_RECALL_MEM,
  FT900_NATIVE_VFO_TO_MEM,
  FT900_NATIVE_VFO_A,
  FT900_NATIVE_VFO_B,
  FT900_NATIVE_MEM_TO_VFO,
  FT900_NATIVE_CLARIFIER_OPS,
  FT900_NATIVE_FREQ_SET,
  FT900_NATIVE_MODE_SET,
  FT900_NATIVE_PACING,
  FT900_NATIVE_PTT_OFF,
  FT900_NATIVE_PTT_ON,
  FT900_NATIVE_MEM_CHNL,
  FT900_NATIVE_OP_DATA,
  FT900_NATIVE_VFO_DATA,
  FT900_NATIVE_MEM_CHNL_DATA,
  FT900_NATIVE_TUNER_OFF,
  FT900_NATIVE_TUNER_ON,
  FT900_NATIVE_TUNER_START,
  FT900_NATIVE_READ_METER,
  FT900_NATIVE_READ_FLAGS,
  FT900_NATIVE_SIZE             /* end marker, value indicates number of */
				                /* native cmd entries */
};

typedef enum ft900_native_cmd_e ft900_native_cmd_t;


/*
 * Internal MODES - when setting modes via FT900_NATIVE_MODE_SET
 *
 */

#define MODE_SET_LSB    0x00
#define MODE_SET_USB    0x01
#define MODE_SET_CW_W   0x02
#define MODE_SET_CW_N   0x03
#define MODE_SET_AM_W   0x04
#define MODE_SET_AM_N   0x05
#define MODE_SET_FM     0x06


/*
 * Internal Clarifier parms - when setting clarifier via
 * FT900_NATIVE_CLARIFIER_OPS
 *
 * The manual seems to be incorrect with regard to P1 and P2 values
 * P1 = 0x00    clarifier off
 * P1 = 0x01    clarifier on
 * P1 = 0xff    clarifier set
 * P2 = 0x00    clarifier up
 * P2 = 0xff    clarifier down
 */

/* P1 values */
#define CLAR_RX_OFF     0x00
#define CLAR_RX_ON      0x01
#define CLAR_SET_FREQ   0xff

/* P2 values */
#define CLAR_OFFSET_PLUS    0x00
#define CLAR_OFFSET_MINUS   0xff


/*
 * Some useful offsets in the status update flags (offset)
 * SUMO--Status Update Memory Offset?
 *
 * SF_ bit tests are now grouped with flag bytes for ease of reference
 *
 * FIXME: complete flags and bits
 *
 * CAT command 0xFA requests the FT-900 to return its status flags.
 * These flags consist of 3 bytes (plus 2 filler bytes) and are documented
 * in the FT-900 manual on page 33.
 *
 */

#define FT900_SUMO_DISPLAYED_STATUS_0   0x00    /* Status flag byte 0 */
#define SF_GC       (1<<1)              /* General Coverage Reception selected */
#define SF_SPLIT    (1<<2)              /* Split active */
#define SF_MCK      (1<<3)              /* memory Checking in progress */
#define SF_MT       (1<<4)              /* Memory Tuning in progress */
#define SF_MR       (1<<5)              /* Memory Mode selected */
#define SF_A        (0<<6)              /* bit 6 clear, VFO A */
#define SF_B        (1<<6)              /* bit 6 set, VFO B */
#define SF_VFO      (1<<7)              /* bit 7 set, VFO A or B active */

#define SF_VFOA     (SF_VFO|SF_A)       /* bit 7 set, bit 6 clear, VFO A */
#define SF_VFOB     (SF_VFO|SF_B)       /* bit 7 set, bit 6 set, VFO B */
#define SF_VFO_MASK (SF_VFOB)           /* bits 6 and 7 */
#define SF_MEM_MASK (SF_MCK|SF_MT|SF_MR)    /* bits 3, 4 and 5 */


#define FT900_SUMO_DISPLAYED_STATUS_1   0x01    /* Status flag byte 1 */


#define FT900_SUMO_DISPLAYED_STATUS_2   0x02    /* Status flag byte 1 */
#define SF_PTT_OFF  (0<<7)              /* bit 7 set, PTT open */
#define SF_PTT_ON   (1<<7)              /* bit 7 set, PTT closed */
#define SF_PTT_MASK (SF_PTT_ON)


/*
 * Offsets for VFO record retrieved via 0x10 P1 = 02, 03, 04
 *
 * The FT-900 returns frequency and mode data via three seperate commands.
 * CAT command 0x10, P1 = 02 returns the current main and sub displays' data (19 bytes)
 * CAT command 0x10, P1 = 03 returns VFO A & B data  (18 bytes)
 * CAT command 0x10, P1 = 04, P4 = 0x01-0x20 returns memory channel data (19 bytes)
 * In all cases the format is (from the FT-900 manual page 32):
 *
 * Offset       Value
 * 0x00         Band Selection          (BPF selection: 0x00 - 0x30 (bit 7 =1 on a blanked memory))
 * 0x01         Operating Frequency     (Hex value of display--Not BCD!)
 * 0x04         Clarifier Offset        (signed value between -999d (0xfc19) and +999d (0x03e7))
 * 0x06         Mode Data
 * 0x07         CTCSS tone code         (0x00 - 0x20)
 * 0x08         Flags                   (Operating flags -- manual page 33)
 *
 * Memory Channel data has the same layout and offsets as the operating
 * data record.
 * When either of the 19 byte records is read (P1 = 02, 04), the offset is
 * +1 as the leading byte is the memory channel number.
 * The VFO data command (P1 = 03) returns 18 bytes and the VFO B data has
 * the same layout, but the offset starts at 0x09 and continues through 0x12
 *
 */

#define FT900_SUMO_MEM_CHANNEL          0x00    /* Memory Channel from 0xfa, P1 = 1 */
#define FT900_SUMO_DISPLAYED_FREQ       0x02    /* Current main display, can be VFO A, Memory data, Memory tune (3 bytes) */
#define FT900_SUMO_DISPLAYED_CLAR       0x05    /* RIT offset -- current display */
#define FT900_SUMO_DISPLAYED_MODE       0x07    /* Current main display mode */
#define FT900_SUMO_DISPLAYED_FLAG       0x09

#define FT900_SUMO_VFO_A_FREQ           0x01    /* VFO A frequency, not necessarily currently displayed! */
#define FT900_SUMO_VFO_A_CLAR           0x04    /* RIT offset -- VFO A */
#define FT900_SUMO_VFO_A_MODE           0x06    /* VFO A mode, not necessarily currently displayed! */
#define FT900_SUMO_VFO_A_FLAG           0x08

#define FT900_SUMO_VFO_B_FREQ           0x0a    /* Current sub display && VFO B */
#define FT900_SUMO_VFO_B_CLAR           0x0d    /* RIT offset -- VFO B */
#define FT900_SUMO_VFO_B_MODE           0x0f    /* Current sub display && VFO B */
#define FT900_SUMO_VFO_B_FLAG           0x11


/*
 * Read meter offset
 *
 * FT-900 returns the level of the S meter when in RX and ALC or PO or SWR
 * when in TX.  The level is replicated in the first four bytes sent by the
 * rig with the final byte being a constant 0xf7
 *
 * The manual states that the returned value will range between 0x00 and 0xff
 * while "in practice the highest value returned will be around 0xf0".  The
 * manual is silent when this value is returned as my rig returns 0x00 for
 * S0, 0x44 for S9 and 0x9D for S9 +60.
 *
 */

#define FT900_SUMO_METER                0x00    /* Meter level */


/*
 * Narrow filter selection flag from offset 0x08 or 0x11
 * in VFO/Memory Record
 *
 * used when READING modes from FT-900
 *
 */

#define FLAG_AM_N   (1<<6)
#define FLAG_CW_N   (1<<7)
#define FLAG_MASK   (FLAG_AM_N|FLAG_CW_N)


/*
 * Mode Bitmap from offset 0x06 or 0x0f in VFO/Memory Record.
 *
 * used when READING modes from FT-900
 *
 */

#define MODE_LSB     0x00
#define MODE_USB     0x01
#define MODE_CW      0x02
#define MODE_AM      0x03
#define MODE_FM      0x04

/* All relevant bits */
#define MODE_MASK   (MODE_LSB|MODE_USB|MODE_CW|MODE_AM|MODE_FM)


/*
 * Command string parameter offsets
 */

#define P1  3
#define P2  2
#define P3  1
#define P4  0

/*
 * Two calibration sets for the smeter/power readings 
 */

#define FT900_STR_CAL_SMETER { 3, \
        { \
                {   0, -54 }, /* S0    */ \
                {0x44,   0 }, /* S9    */ \
                {0x9d,  60 }, /* +60dB */ \
        } }

#define FT900_STR_CAL_POWER { 5, \
        { \
                {   0,   0 }, /*  0W  */ \
                {0x44,  10 }, /* 10W  */ \
                {0x69,  25 }, /* 25W  */ \
                {0x92,  50 }, /* 50W  */ \
                {0xCE, 100 }, /* 100W */ \
        } }

/*
 * API local implementation
 *
 */

static int ft900_init(RIG *rig);
static int ft900_cleanup(RIG *rig);
static int ft900_open(RIG *rig);
static int ft900_close(RIG *rig);

static int ft900_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft900_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft900_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft900_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft900_set_vfo(RIG *rig, vfo_t vfo);
static int ft900_get_vfo(RIG *rig, vfo_t *vfo);

static int ft900_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft900_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);

static int ft900_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int ft900_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);

static int ft900_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft900_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);

static int ft900_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);

static int ft900_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

static int ft900_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

#endif /* _FT900_H */
