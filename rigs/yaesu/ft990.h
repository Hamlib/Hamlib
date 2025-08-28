/*
 * hamlib - (C) Stephane Fillod 2002, 2003 (fillods at users.sourceforge.net)
 *
 * ft990.h - (C) Berndt Josef Wulf (wulf at ping.net.au)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-990 using the "CAT" interface
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


#ifndef _FT990_H
#define _FT990_H 1

// Global Definitions
#define TRUE    1
#define FALSE   0
#define ON      TRUE
#define OFF     FALSE

/* RX caps */

#define FT990_ALL_RX_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTFM|RIG_MODE_PKTLSB)
#define FT990_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTLSB)
#define FT990_RTTY_RX_MODES (RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define FT990_AM_RX_MODES (RIG_MODE_AM)
#define FT990_FM_RX_MODES (RIG_MODE_FM|RIG_MODE_PKTFM)


/* TX caps */

#define FT990_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_PKTFM|RIG_MODE_PKTLSB) /* 100 W class */
#define FT990_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */
#define FT990_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_LOCK|RIG_FUNC_TUNER) /* fix */


/* Other features */

#define FT990_VFO_ALL (RIG_VFO_A|RIG_VFO_B)
#define FT990_ANTS 0
#define FT990_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_CPY|RIG_OP_UP|RIG_OP_DOWN)

/* Returned data length in bytes */

#define FT990_ALL_DATA_LENGTH           1508    /* 0x10 P1 = 00 return size */
#define FT990_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT990_OP_DATA_LENGTH            32      /* 0x10 P1 = 02 return size */
#define FT990_VFO_DATA_LENGTH           32      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT990_MEM_CHNL_DATA_LENGTH      16      /* 0x10 P1 = 04, P4 = 0x00-0x59 return size */
#define FT990_READ_METER_LENGTH         5       /* 0xf7 return size */
#define FT990_STATUS_FLAGS_LENGTH       5       /* 0xfa return size */

/* BCD coded frequency length */

#define FT990_BCD_DIAL                  8
#define FT990_BCD_RIT                   3
#define FT990_BCD_RPTR_OFFSET           6

/* Timing values in mS */

#define FT990_PACING_INTERVAL           5
#define FT990_PACING_DEFAULT_VALUE      0
#define FT990_WRITE_DELAY               50


/* Delay sequential fast writes */

#define FT990_POST_WRITE_DELAY          5


/* Rough safe value for default timeout */

#define FT990_DEFAULT_READ_TIMEOUT FT990_ALL_DATA_LENGTH * ( 5 + (FT990_PACING_INTERVAL * FT990_PACING_DEFAULT_VALUE))

/*
 * The definitions below are copied from the kft990
 * project and are hereby made available to the
 * hamlib project. [BJW]
 */

//  OpCode Declarations
#define FT990_CMD_SPLIT       0x01
#define FT990_CMD_RECALLMEM   0x02
#define FT990_CMD_VFO2MEM     0x03
#define FT990_CMD_LOCK        0x04
#define FT990_CMD_SELVFOAB    0x05
#define FT990_CMD_MEM2VFO     0x06
#define FT990_CMD_UP          0x07
#define FT990_CMD_DOWN        0x08
#define FT990_CMD_CLARIFIER   0x09
#define FT990_CMD_SETVFOA     0x0a
#define FT990_CMD_SELOPMODE   0x0c
#define FT990_CMD_PACING      0x0e
#define FT990_CMD_PTT         0x0f
#define FT990_CMD_UPDATE      0x10
#define FT990_CMD_TUNER       0x81
#define FT990_CMD_START       0x82
#define FT990_CMD_RPT         0x84
#define FT990_CMD_VFOA2B      0x85
#define FT990_CMD_BW          0x8c
#define FT990_CMD_MEMSCANSKIP 0x8d
#define FT990_CMD_STEPVFO     0x8e
#define FT990_CMD_RDMETER     0xf7
#define FT990_CMD_DIMLEVEL    0xf8
#define FT990_CMD_RPTROFFSET  0xf9
#define FT990_CMD_RDFLAGS     0xfa

// Bandwidth Filter
#define FT990_BW_F2400        0x00
#define FT990_BW_F2000        0x01
#define FT990_BW_F500         0x02
#define FT990_BW_F250         0x03
#define FT990_BW_F6000        0x04
#define FT990_BW_FMPKTRTTY    0x80

// Operating Mode Status
#define FT990_MODE_LSB        0x00
#define FT990_MODE_USB        0x01
#define FT990_MODE_CW         0x02
#define FT990_MODE_AM         0x03
#define FT990_MODE_FM         0x04
#define FT990_MODE_RTTY       0x05
#define FT990_MODE_PKT        0x06

// Operation Mode Selection
#define FT990_OP_MODE_LSB     0x00
#define FT990_OP_MODE_USB     0x01
#define FT990_OP_MODE_CW2400  0x02
#define FT990_OP_MODE_CW500   0x03
#define FT990_OP_MODE_AM6000  0x04
#define FT990_OP_MODE_AM2400  0x05
#define FT990_OP_MODE_FM      0x06
#define FT990_OP_MODE_RTTYLSB 0x08
#define FT990_OP_MODE_RTTYUSB 0x09
#define FT990_OP_MODE_PKTLSB  0x0a
#define FT990_OP_MODE_PKTFM   0x0b

// Clarifier Operation
#define FT990_CLAR_TX_EN      0x01
#define FT990_CLAR_RX_EN      0x02
#define FT990_CLAR_RX_OFF     0x00
#define FT990_CLAR_RX_ON      0x01
#define FT990_CLAR_TX_OFF     0x80
#define FT990_CLAR_TX_ON      0x81
#define FT990_CLAR_CLEAR      0xff
#define FT990_CLAR_TUNE_UP    0x00
#define FT990_CLAR_TUNE_DOWN  0xff

// Repeater Shift Enable
#define FT990_RPT_POS_EN      0x04
#define FT990_RPT_NEG_EN      0x08
#define FT990_RPT_MASK        0x0C

// Status Flag 1 Masks
#define FT990_SF_SPLIT                0x01
#define FT990_SF_VFOB                 0x02
#define FT990_SF_FAST                 0x04
#define FT990_SF_CAT                  0x08
#define FT990_SF_TUNING               0x10
#define FT990_SF_KEY_ENTRY            0x20
#define FT990_SF_MEM_EMPTY            0x40
#define FT990_SF_XMIT                 0x80

// Status Flag 2 Masks
#define FT990_SF_MEM_SCAN_PAUSE       0x01
#define FT990_SF_MEM_CHECK            0x02
#define FT990_SF_MEM_SCAN             0x04
#define FT990_SF_LOCKED               0x08
#define FT990_SF_MTUNE                0x10
#define FT990_SF_VFO                  0x20
#define FT990_SF_MEM                  0x40
#define FT990_SF_GEN                  0x80

// Status Flag 3 Masks
#define FT990_SF_PTT                  0x01
#define FT990_SF_TX_INHIBIT           0x02
#define FT990_SF_KEY_TIMER            0x04
#define FT990_SF_MEM_TIMER            0x08
#define FT990_SF_PTT_INHIBIT          0x10
#define FT990_SF_XMIT_MON             0x20
#define FT990_SF_TUNER_ON             0x40
#define FT990_SF_SIDETONE             0x80

#define FT990_EMPTY_MEM               0x80

#define FT990_AMFILTER2400            0x80

// Flags Byte 1
typedef struct _ft990_flags1_t {
  unsigned split:       1;
  unsigned vfob:        1;
  unsigned fast:        1;
  unsigned cat:         1;
  unsigned tuning:      1;
  unsigned keyentry:    1;
  unsigned memempty:    1;
  unsigned xmit:        1;
} ft990_flags1_t;

// Flags Byte 2
typedef struct _ft990_flags2_t {
  unsigned memscanpause:1;
  unsigned memcheck:    1;
  unsigned memscan:     1;
  unsigned locked:      1;
  unsigned mtune:       1;
  unsigned vfo:         1;
  unsigned mem:         1;
  unsigned gen:         1;
} ft990_flags2_t;

// Flags Byte 3
typedef struct _ft990_status3_t {
  unsigned ptt:         1;
  unsigned txinhibit:   1;
  unsigned keytimer:    1;
  unsigned memtimer:    1;
  unsigned pttinhibit:  1;
  unsigned xmitmon:     1;
  unsigned tuneron:     1;
  unsigned sidetone:    1;
} ft990_flags3_t;

typedef union _ft990_flags1_u {
  ft990_flags1_t bits;
  unsigned char byte;
} ft990_flags1_u;

typedef union _ft990_flags2_u {
  ft990_flags2_t bits;
  unsigned char byte;
} ft990_flags2_u;

typedef union _ft990_flags3_u {
  ft990_flags3_t bits;
  unsigned char byte;
} ft990_flags3_u;

typedef struct _ft990_status_data_t {
  ft990_flags1_u flags1;
  ft990_flags2_u flags2;
  ft990_flags3_u flags3;
  unsigned char id1;
  unsigned char id2;
} ft990_status_data_t;

typedef struct _ft990_meter_data_t {
  unsigned char mdata1;
  unsigned char mdata2;
  unsigned char mdata3;
  unsigned char mdata4;
  unsigned char id1;
} ft990_meter_data_t;

typedef struct _ft990_op_data_t {
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
} ft990_op_data_t;

// Update Data Structure
typedef struct _ft990_update_data_t {
  unsigned char flag1;
  unsigned char flag2;
  unsigned char flag3;
  unsigned char channelnumber;
  ft990_op_data_t current_front;
  ft990_op_data_t current_rear;
  ft990_op_data_t vfoa;
  ft990_op_data_t vfob;
  ft990_op_data_t channel[90];
} ft990_update_data_t;

// Command Structure
typedef struct _ft990_command_t {
  unsigned char data[4];
  unsigned char opcode;
} ft990_command_t;

#endif /* _FT990_H */
