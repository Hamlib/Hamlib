/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 *
 * ft920.h - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 *           (C) Nate Bargmann 2002 (n0nb@arrl.net)
 *           (C) Stephane Fillod 2002 (fillods@users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-920 using the "CAT" interface
 *
 *
 *    $Id: ft920.h,v 1.6 2002-11-15 13:15:25 n0nb Exp $  
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */


#ifndef _FT920_H
#define _FT920_H 1

#define FT920_VFO_ALL (RIG_VFO_A|RIG_VFO_B)


/*
 * Receiver caps 
 */

#define FT920_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT920_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT920_AM_RX_MODES (RIG_MODE_AM)
#define FT920_FM_RX_MODES (RIG_MODE_FM)


/* 
 * TX caps
 */ 

#define FT920_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB ) /* 100 W class */
#define FT920_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */
#define FT920_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN) /* fix */


/*
 * Returned data length in bytes
 *
 */

#define FT920_MEM_CHNL_LENGTH           1           /* 0x10 P1 = 01 return size */
#define FT920_STATUS_FLAGS_LENGTH       8           /* 0xfa return size */
#define FT920_VFO_DATA_LENGTH           28          /* 0x10 P1 = 02, 03 return size */
#define FT920_MEM_CHNL_DATA_LENGTH      14          /* 0x10 P1 = 04, P4 = 0x00-0x89 return size */


/*
 * Timing values in mS
 *
 */

#define FT920_PACING_INTERVAL                5 
#define FT920_PACING_DEFAULT_VALUE           1
#define FT920_WRITE_DELAY                    50


/* Delay sequential fast writes */

#define FT920_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT920_DEFAULT_READ_TIMEOUT  28 * ( 5 + (FT920_PACING_INTERVAL * FT920_PACING_DEFAULT_VALUE))


/*
 * 8N2 and 1 start bit = 11 bits at 4800 bps => effective byte rate = 1 byte in 2.2917 msec
 * => 28 bytes in 64 msec
 *
 * delay for 28 bytes = (2.2917 + pace_interval) * 28
 *
 * pace_interval          time to read 28 bytes
 * ------------           ----------------------
 *
 *     0                       64 msec
 *     1                       92 msec
 *     2                       120 msec
 *     5                       204 msec
 *    255                      7.2 sec
 *
 */


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
  FT920_NATIVE_M_TO_VFO,
  FT920_NATIVE_VFO_A_FREQ_SET,
  FT920_NATIVE_VFO_A_MODE_SET_LSB,
  FT920_NATIVE_VFO_A_MODE_SET_USB,
  FT920_NATIVE_VFO_A_MODE_SET_CW_USB,
  FT920_NATIVE_VFO_A_MODE_SET_CW_LSB,
  FT920_NATIVE_VFO_A_MODE_SET_AM,
  FT920_NATIVE_VFO_A_MODE_SET_FMW,
  FT920_NATIVE_VFO_A_MODE_SET_FMN,
  FT920_NATIVE_VFO_A_MODE_SET_DATA_LSB,
  FT920_NATIVE_VFO_A_MODE_SET_DATA_USB,
  FT920_NATIVE_VFO_A_MODE_SET_DATA_FM,
  FT920_NATIVE_VFO_B_MODE_SET_LSB,
  FT920_NATIVE_VFO_B_MODE_SET_USB,
  FT920_NATIVE_VFO_B_MODE_SET_CW_USB,
  FT920_NATIVE_VFO_B_MODE_SET_CW_LSB,
  FT920_NATIVE_VFO_B_MODE_SET_AM,
  FT920_NATIVE_VFO_B_MODE_SET_FMW,
  FT920_NATIVE_VFO_B_MODE_SET_FMN,
  FT920_NATIVE_VFO_B_MODE_SET_DATA_LSB,
  FT920_NATIVE_VFO_B_MODE_SET_DATA_USB,
  FT920_NATIVE_VFO_B_MODE_SET_DATA_FM,
  FT920_NATIVE_PACING,
  FT920_NATIVE_MEM_CHNL,
  FT920_NATIVE_OP_DATA,
  FT920_NATIVE_VFO_DATA,
  FT920_NATIVE_MEM_CHNL_DATA,
  FT920_NATIVE_VFO_B_FREQ_SET,
  FT920_NATIVE_VFO_A_PASSBAND_WIDE,
  FT920_NATIVE_VFO_A_PASSBAND_NAR,
  FT920_NATIVE_VFO_B_PASSBAND_WIDE,
  FT920_NATIVE_VFO_B_PASSBAND_NAR,
  FT920_NATIVE_STATUS_FLAGS,
  FT920_NATIVE_SIZE             /* end marker, value indicates number of */
				                /* native cmd entries */
};

typedef enum ft920_native_cmd_e ft920_native_cmd_t;


/*
 * Internal MODES - when setting modes via cmd_mode_set()
 *
 */

#define MODE_SET_LSB    0x00
#define MODE_SET_USB    0x01
#define MODE_SET_CWW    0x02
#define MODE_SET_CWN    0x03
#define MODE_SET_AMW    0x04
#define MODE_SET_AMN    0x05
#define MODE_SET_FMW    0x06
#define MODE_SET_FMN    0x07



/*
 * Status Flag Masks when reading
 *
 */

#define SF_DLOCK   0x01
#define SF_CLAR    0x04
#define SF_RXTX    0x20
#define SF_RESV    0x40
#define SF_PRI     0x80


/*
 * Local VFO CMD's, according to spec
 *
 */

#define FT920_VFO_A                  0x00
#define FT920_VFO_B                  0x01


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
#define SF_SPLITA   0x01    /* Split operation with VFO-B on TX */
#define SF_SPLITB   0x02    /* Split operation with VFO-B on RX */
#define SF_VFOAB    0x03    /* bits 0 & 1, VFO A TX/RX == 0, VFO B TX/RX  == 3 */

#define FT920_SUMO_DISPLAYED_STATUS_1   0x01    /* Status flag byte 1 */
#define SF_QMB      0x08    /* Quick Memory Bank (QMB) selected */
#define SF_MT       0x10    /* Memory Tuning in progress */
#define SF_VFO      0x20    /* VFO operation selected */
#define SF_MR       0x40    /* Memory Mode selected */
#define SF_GC       0x80    /* General Coverage Reception selected */
#define SF_VFO_MASK 0x78    /* Ignore general coverage flag.  Is it needed? */

/*
 * Offsets for VFO record retrieved via 0x10 P1 = 02, 03
 *
 * The FT-920 returns frequency and mode data via three seperate commands.
 * CAT command 0x10, P1 = 02 returns the current main and sub displays' data (28 bytes)
 * CAT command 0x10, P1 = 03 returns VFO A data and the sub display data (sub display is always VFO B) (28 bytes)
 * CAT command 0x10, P1 = 04, P4 = 0x00-0x89 returns memory channel data (14 bytes)
 * In all cases the format is (from the FT-920 manual page 90):
 *
 * Offset       Value
 * 0x00         Band Selection          (not documented!)
 * 0x01         Operating Frequency     (Hex value of display--Not BCD!)
 * 0x05         Clarifier Offset        (Hex value)
 * 0x07         Mode Data
 * 0x08         Flag
 * 0x09         Filter Data 1
 * 0x0a         Filter Data 2
 * 0x0b         CTCSS Encoder Data
 * 0x0c         CTCSS Decoder Data
 * 0x0d         Memory recall Flag
 *
 * Memory Channel data has the same layout and offsets
 * VFO B data has the same layout, but the offset starts at 0x0e and
 * continues through 0x1b
 *
 */

#define FT920_SUMO_DISPLAYED_FREQ       0x01    /* Current main display, can be VFO A, Memory data, Memory tune */
#define FT920_SUMO_VFO_A_FREQ           0x01    /* VFO A frequency, not necessarily currently displayed! */
#define FT920_SUMO_DISPLAYED_MODE       0x07    /* Current main display mode */
#define FT920_SUMO_VFO_A_MODE           0x07    /* VFO A mode, not necessarily currently displayed! */
#define FT920_SUMO_VFO_B_FREQ           0x0f    /* Current sub display && VFO B */
#define FT920_SUMO_VFO_B_MODE           0x15    /* Current sub display && VFO B */
    

/*
 * Mode Bitmap from offset 0x07 or 0x16 in VFO Record.
 * Bits 5 and 6 ignored
 * used when READING modes from FT-920
 *
 */

#define MODE_LSB     0x00
#define MODE_CW_L    0x01       /* CW listening on LSB */
#define MODE_AM      0x02
#define MODE_FM      0x03
#define MODE_DATA_L  0x04       /* DATA on LSB */
#define MODE_DATA_U  0x05       /* DATA on USB (who does that? :) */
#define MODE_DATA_F  0x06       /* DATA on FM */
#define MODE_USB     0x40
#define MODE_CW_U    0x41       /* CW listening on USB */
/* Narrow filter selected */
#define MODE_LSBN    0x80
#define MODE_CW_LN   0x81
#define MODE_AMN     0x82
#define MODE_FMN     0x83
#define MODE_DATA_LN 0x84
#define MODE_DATA_UN 0x85
#define MODE_DATA_FN 0x86
#define MODE_USBN    0xc0
#define MODE_CW_UN   0xc1

/* All relevent bits */
#define MODE_MASK   0xc7


/* 
 * API local implementation
 *
 */

int ft920_init(RIG *rig);
int ft920_cleanup(RIG *rig);
int ft920_open(RIG *rig);
int ft920_close(RIG *rig);

int ft920_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ft920_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

int ft920_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
int ft920_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

int ft920_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
int ft920_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */


#endif /* _FT920_H */
