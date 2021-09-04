/*
 * hamlib - (C) Stephane Fillod 2002, 2003 (fillods at users.sourceforge.net)
 *
 * ft1000d.h - (C) 2016 Sean Sharkey (g0oan at icloud.com)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-1000D using the "CAT" interface
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


#ifndef _FT1000D_H
#define _FT1000D_H 1

// Global Definitions
#define TRUE    1
#define FALSE   0
#define ON      TRUE
#define OFF     FALSE

/* RX caps */

#define FT1000D_ALL_RX_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTFM|RIG_MODE_PKTLSB)
#define FT1000D_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTLSB)
#define FT1000D_RTTY_RX_MODES (RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define FT1000D_AM_RX_MODES (RIG_MODE_AM)
#define FT1000D_FM_RX_MODES (RIG_MODE_FM|RIG_MODE_PKTFM)


/* TX caps */

#define FT1000D_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_PKTFM|RIG_MODE_PKTLSB) /* 100 W class */
#define FT1000D_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */
#define FT1000D_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_LOCK|RIG_FUNC_TUNER) /* fix */


/* Other features */

#define FT1000D_VFO_ALL (RIG_VFO_A|RIG_VFO_B)
#define FT1000D_ANTS 0
#define FT1000D_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_CPY|RIG_OP_UP|RIG_OP_DOWN)
// Added the below to store currently selected VFO as set by CAT command Opcode 05h
#define FT1000D_CURRENTLY_SELECTED_VFO

/* Returned data length in bytes */

#define FT1000D_ALL_DATA_LENGTH           1636    /* 0x10 P1 = 00 return size */
#define FT1000D_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT1000D_OP_DATA_LENGTH            16      /* 0x10 P1 = 02 return size */
#define FT1000D_VFO_DATA_LENGTH           32      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT1000D_MEM_CHNL_DATA_LENGTH      16      /* 0x10 P1 = 04, P4 = 0x00-0x59 return size */
#define FT1000D_READ_METER_LENGTH         5       /* 0xf7 return size */
#define FT1000D_STATUS_FLAGS_LENGTH       5       /* 0xfa return size */

/* BCD coded frequency length */

#define FT1000D_BCD_DIAL                  8
#define FT1000D_BCD_RIT                   3
#define FT1000D_BCD_RPTR_OFFSET           6

/* Timing values in mS */

#define FT1000D_PACING_INTERVAL           5
#define FT1000D_PACING_DEFAULT_VALUE      0
#define FT1000D_WRITE_DELAY               0


/* Delay sequential fast writes */

#define FT1000D_POST_WRITE_DELAY          5


/* Rough safe value for default timeout */

#define FT1000D_DEFAULT_READ_TIMEOUT FT1000D_ALL_DATA_LENGTH * ( 5 + (FT1000D_PACING_INTERVAL * FT1000D_PACING_DEFAULT_VALUE))

/*
 * The definitions below are copied from the kFT1000D
 * project and are hereby made available to the
 * hamlib project. [BJW]
 */

//  OpCode Declarations
#define FT1000D_CMD_SPLIT       0x01
#define FT1000D_CMD_RECALLMEM   0x02
#define FT1000D_CMD_VFO2MEM     0x03
#define FT1000D_CMD_LOCK        0x04
#define FT1000D_CMD_SELVFOAB    0x05
#define FT1000D_CMD_MEM2VFO     0x06
#define FT1000D_CMD_UP          0x07
#define FT1000D_CMD_DOWN        0x08
#define FT1000D_CMD_CLARIFIER   0x09
#define FT1000D_CMD_SETVFOA     0x0a
#define FT1000D_CMD_SELOPMODE   0x0c
#define FT1000D_CMD_PACING      0x0e
#define FT1000D_CMD_PTT         0x0f
#define FT1000D_CMD_UPDATE      0x10
#define FT1000D_CMD_TUNER       0x81
#define FT1000D_CMD_START       0x82
#define FT1000D_CMD_RPT         0x84
#define FT1000D_CMD_VFOA2B      0x85
#define FT1000D_CMD_SUBVFOFREQ  0X8a
#define FT1000D_CMD_BW          0x8c
#define FT1000D_CMD_MEMSCANSKIP 0x8d
#define FT1000D_CMD_STEPVFO     0x8e
#define FT1000D_CMD_RDMETER     0xf7
#define FT1000D_CMD_DIMLEVEL    0xf8
#define FT1000D_CMD_RPTROFFSET  0xf9
#define FT1000D_CMD_RDFLAGS     0xfa

// Bandwidth Filter
#define FT1000D_BW_F2400        0x00
#define FT1000D_BW_F2000        0x01
#define FT1000D_BW_F500         0x02
#define FT1000D_BW_F250         0x03
#define FT1000D_BW_F6000        0x04
#define FT1000D_BW_FMPKTRTTY    0x80
#define FT1000D_SUB_VFOB_BW_F2400        0x80 /* Added December 2016 */
#define FT1000D_SUB_VFOB_BW_F2000        0x81 /* Added December 2016 */
#define FT1000D_SUB_VFOB_BW_F500         0x82 /* Added December 2016 */
#define FT1000D_SUB_VFOB_BW_F250         0x83 /* Added December 2016 */
#define FT1000D_SUB_VFOB_BW_F6000        0x84 /* Added December 2016 */


// Operating Mode Status
#define FT1000D_MODE_LSB        0x00
#define FT1000D_MODE_USB        0x01
#define FT1000D_MODE_CW         0x02
#define FT1000D_MODE_AM         0x03
#define FT1000D_MODE_FM         0x04
#define FT1000D_MODE_RTTY       0x05
#define FT1000D_MODE_PKT        0x06

// Operation Mode Selection
#define FT1000D_OP_MODE_LSB     0x00
#define FT1000D_OP_MODE_USB     0x01
#define FT1000D_OP_MODE_CW2400  0x02
#define FT1000D_OP_MODE_CW500   0x03
#define FT1000D_OP_MODE_AM6000  0x04
#define FT1000D_OP_MODE_AM2400  0x05
#define FT1000D_OP_MODE_FM      0x06
#define FT1000D_OP_MODE_RTTYLSB 0x08
#define FT1000D_OP_MODE_RTTYUSB 0x09
#define FT1000D_OP_MODE_PKTLSB  0x0a
#define FT1000D_OP_MODE_PKTFM   0x0b

// Clarifier Operation
#define FT1000D_CLAR_TX_EN      0x01
#define FT1000D_CLAR_RX_EN      0x02
#define FT1000D_CLAR_RX_OFF     0x00
#define FT1000D_CLAR_RX_ON      0x01
#define FT1000D_CLAR_TX_OFF     0x80
#define FT1000D_CLAR_TX_ON      0x81
#define FT1000D_CLAR_CLEAR      0xff
#define FT1000D_CLAR_TUNE_UP    0x00
#define FT1000D_CLAR_TUNE_DOWN  0xff

// Repeater Shift Enable
#define FT1000D_RPT_POS_EN      0x04
#define FT1000D_RPT_NEG_EN      0x08
#define FT1000D_RPT_MASK        0x0C

// Status Flag 1 Masks
#define FT1000D_SF_SPLIT                0x01 /* Added December 2016 */
#define FT1000D_SF_DUAL                 0x02 /* Added December 2016 */
#define FT1000D_SF_ANT_TUNER_ACTIVE     0x04 /* Added December 2016 */
#define FT1000D_SF_CAT                  0x08 /* Added December 2016 */
#define FT1000D_SF_VFOB_INUSE           0x10 /* Added December 2016 APPEARS NOT TO WORK RIG DOES NOT SET BIT */
#define FT1000D_SF_KEY_ENTRY            0x20 /* Added December 2016 */
#define FT1000D_SF_MEM_EMPTY            0x40 /* Added December 2016 */
#define FT1000D_SF_XMIT                 0x80 /* Added December 2016 */

// Status Flag 2 Masks
#define FT1000D_SF_MEM_SCAN_PAUSE       0x01
#define FT1000D_SF_MEM_CHECK            0x02
#define FT1000D_SF_MEM_SCAN             0x04
#define FT1000D_SF_LOCKED               0x08
#define FT1000D_SF_MTUNE                0x10
#define FT1000D_SF_VFO                  0x20
#define FT1000D_SF_MEM                  0x40
#define FT1000D_SF_GEN                  0x80

// Status Flag 3 Masks
#define FT1000D_SF_PTT                  0x01
#define FT1000D_SF_TX_INHIBIT           0x02
#define FT1000D_SF_KEY_TIMER            0x04
#define FT1000D_SF_MEM_TIMER            0x08
#define FT1000D_SF_PTT_INHIBIT          0x10
#define FT1000D_SF_XMIT_MON             0x20
#define FT1000D_SF_TUNER_ON             0x40
#define FT1000D_SF_SUB_VFOB_LOCKED      0x80

#define FT1000D_EMPTY_MEM               0x80

#define FT1000D_AMFILTER2400            0x80

// Flags Byte 1
typedef struct _ft1000d_flags1_t {
  unsigned split:       1;
  unsigned dualrx:      1;
  unsigned tuneact:     1;
  unsigned cat:         1;
  unsigned vfobinuse:   1;
  unsigned keyentry:    1;
  unsigned memempty:    1;
  unsigned xmit:        1;
} ft1000d_flags1_t;

// Flags Byte 2
typedef struct _ft1000d_flags2_t {
  unsigned memscanpause:1;
  unsigned memcheck:    1;
  unsigned memscan:     1;
  unsigned locked:      1;
  unsigned mtune:       1;
  unsigned vfo:         1;
  unsigned mem:         1;
  unsigned gen:         1;
} ft1000d_flags2_t;

// Flags Byte 3
typedef struct _ft1000d_status3_t {
  unsigned ptt:         1;
  unsigned txinhibit:   1;
  unsigned keytimer:    1;
  unsigned memtimer:    1;
  unsigned pttinhibit:  1;
  unsigned xmitmon:     1;
  unsigned tuneron:     1;
  unsigned subvfoblock: 1;
} ft1000d_flags3_t;

typedef union _ft1000d_flags1_u {
  ft1000d_flags1_t bits;
  unsigned char byte;
} ft1000d_flags1_u;

typedef union _ft1000d_flags2_u {
  ft1000d_flags2_t bits;
  unsigned char byte;
} ft1000d_flags2_u;

typedef union _ft1000d_flags3_u {
  ft1000d_flags3_t bits;
  unsigned char byte;
} ft1000d_flags3_u;

typedef struct _ft1000d_status_data_t {
  ft1000d_flags1_u flags1;
  ft1000d_flags2_u flags2;
  ft1000d_flags3_u flags3;
  unsigned char id1;
  unsigned char id2;
} ft1000d_status_data_t;

typedef struct _ft1000d_meter_data_t {
  unsigned char mdata1;
  unsigned char mdata2;
  unsigned char mdata3;
  unsigned char mdata4;
  unsigned char id1;
} ft1000d_meter_data_t;

typedef struct _ft1000d_op_data_t {
  unsigned char bpf;
  unsigned char basefreq[3];
  unsigned char status;
  unsigned char coffset[2];
  unsigned char mode;
  unsigned char filter;
  unsigned char lastssbfilter;
  unsigned char lastcwfilter;
  unsigned char lastrttyfilter;
  unsigned char lastpktfilter;
  unsigned char lastclariferstate;
  unsigned char skipscanamfilter;
  unsigned char amfm100;
} ft1000d_op_data_t;

// Update Data Structure
typedef struct _ft1000d_update_data_t {
  unsigned char flag1;
  unsigned char flag2;
  unsigned char flag3;
  unsigned char channelnumber;
  ft1000d_op_data_t current_front;
  ft1000d_op_data_t current_rear;
  ft1000d_op_data_t vfoa;
  ft1000d_op_data_t vfob;
  ft1000d_op_data_t channel[90];
} ft1000d_update_data_t;

// Command Structure
typedef struct _ft1000d_command_t {
  unsigned char data[4];
  unsigned char opcode;
} ft1000d_command_t;

#endif /* _FT1000D_H */
