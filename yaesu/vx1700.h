/*
 *  Copyright (c) 2010-2011 by Mikhail Kshevetskiy (mikhail.kshevetskiy@gmail.com)
 *
 *  Code based on VX-1700 CAT manual:
 *  http://www.vertexstandard.com/downloadFile.cfm?FileID=3397&FileCatID=135&FileName=VX-1700_CAT_MANUAL_10_14_2008.pdf&FileContentType=application%2Fpdf
 *
 *  WARNING: this manual have two errors
 *    1) Status Update Command (10h), U=01 returns  0..199 for channels 1..200
 *    2) Frequency Data (bytes 1--4 of 9-Byte VFO Data Assignment, Status Update
 *       Command (10h), U=02 and U=03) uses bytes 1--3 for frequency, byte 4 is
 *       not used and always zero. Thus bytes 0x15,0xBE,0x68,0x00 means
 *       frequency = 10 * 0x15BE68 = 10 * 1425000 = 14.25 MHz
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

#ifndef _VX1700_H
#define _VX1700_H 1

#include <hamlib/rig.h>
#include <tones.h>


#define	VX1700_MIN_CHANNEL	1
#define	VX1700_MAX_CHANNEL	200

#define	VX1700_MODES	(RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH)

#define	VX1700_VFO_ALL	(RIG_VFO_A|RIG_VFO_MEM)
#define	VX1700_ANTS	RIG_ANT_1
#define	VX1700_VFO_OPS	(RIG_OP_UP|RIG_OP_DOWN|RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define	VX1700_FILTER_WIDTH_NARROW	kHz(0.5)
#define	VX1700_FILTER_WIDTH_WIDE	kHz(2.2)
#define	VX1700_FILTER_WIDTH_SSB		kHz(2.2)
#define	VX1700_FILTER_WIDTH_AM		kHz(6.0)

/* Returned data length in bytes */
#define	VX1700_MEM_CHNL_LENGTH		1	/* 0x10 p1=01 return size */
#define	VX1700_OP_DATA_LENGTH		19	/* 0x10 p1=02 return size */
#define	VX1700_VFO_DATA_LENGTH		18	/* 0x10 p1=03 return size */
#define	VX1700_READ_METER_LENGTH	5	/* 0xf7 return size */
#define	VX1700_STATUS_FLAGS_LENGTH	5	/* 0xfa return size */

/* BCD coded frequency length */
#define	VX1700_BCD_DIAL			8


// VX-1700 native commands
typedef enum vx1700_native_cmd_e {
    VX1700_NATIVE_RECALL_MEM = 0,	/* 0x02, p1=ch */
    VX1700_NATIVE_VFO_TO_MEM,		/* 0x03, p1=ch, p2=0 */
    VX1700_NATIVE_MEM_HIDE,		/* 0x03, p1=ch, p2=1 */
    VX1700_NATIVE_VFO_A,		/* 0x05 */
    VX1700_NATIVE_FREQ_SET,		/* 0x0a, p1:4=freq */
    VX1700_NATIVE_MODE_SET_LSB,		/* 0x0c, p1=0x00 */
    VX1700_NATIVE_MODE_SET_USB,		/* 0x0c, p1=0x01 */
    VX1700_NATIVE_MODE_SET_CW_W,	/* 0x0c, p1=0x02 */
    VX1700_NATIVE_MODE_SET_CW_N,	/* 0x0c, p1=0x03 */
    VX1700_NATIVE_MODE_SET_AM,		/* 0x0c, p1=0x04 */
    VX1700_NATIVE_MODE_SET_RTTY_LSB_W,	/* 0x0c, p1=0x08 */
    VX1700_NATIVE_MODE_SET_RTTY_USB_W,	/* 0x0c, p1=0x09 */
    VX1700_NATIVE_MODE_SET_H3E,		/* 0x0c, p1=0x0d */
    VX1700_NATIVE_MODE_SET_RTTY_LSB_N,	/* 0x0c, p1=0x0e */
    VX1700_NATIVE_MODE_SET_RTTY_USB_N,	/* 0x0c, p1=0x0f */
    VX1700_NATIVE_PTT_OFF,		/* 0x0f, p1=0 */
    VX1700_NATIVE_PTT_ON,		/* 0x0f, p1=1 */
    VX1700_NATIVE_UPDATE_MEM_CHNL,	/* 0x10, p1=1 */
    VX1700_NATIVE_UPDATE_OP_DATA,	/* 0x10, p1=2 */
    VX1700_NATIVE_UPDATE_VFO_DATA,	/* 0x10, p1=3 */
    VX1700_NATIVE_TX_POWER_LOW,		/* 0x18 */
    VX1700_NATIVE_TX_POWER_MID,		/* 0x28 */
    VX1700_NATIVE_TX_POWER_HI,		/* 0x48 */
    VX1700_NATIVE_CPY_RX_TO_TX,		/* 0x85 */
    VX1700_NATIVE_TX_FREQ_SET,		/* 0x8a, p1:4=freq */
    VX1700_NATIVE_OP_FREQ_STEP_UP,	/* 0x8e, p1=0 */
    VX1700_NATIVE_OP_FREQ_STEP_DOWN,	/* 0x8e, p1=1 */
    VX1700_NATIVE_READ_METER,		/* 0xf7 */
    VX1700_NATIVE_READ_FLAGS,		/* 0xfa */
    VX1700_NATIVE_SIZE
} vx1700_native_cmd_t;

//  OpCode Declarations
#define	VX1700_CMD_RECALLMEM		0x02
#define	VX1700_CMD_VFO2MEM		0x03
#define	VX1700_CMD_SEL_VFOA		0x05
#define	VX1700_CMD_SET_VFOA		0x0a
#define	VX1700_CMD_SEL_OP_MODE		0x0c
#define	VX1700_CMD_PTT			0x0f
#define	VX1700_CMD_UPDATE		0x10
#define	VX1700_CMD_RX2TX		0x85
#define	VX1700_CMD_STEP_VFO		0x8e
#define	VX1700_CMD_RD_METER		0xf7
#define	VX1700_CMD_RD_FLAGS		0xfa

// Return codes
#define	VX1700_CMD_RETCODE_OK		0x00
#define	VX1700_CMD_RETCODE_ERROR	0xF0

// Operating Mode Status
#define	VX1700_MODE_LSB			0x00
#define	VX1700_MODE_USB			0x01
#define	VX1700_MODE_CW_W		0x02
#define	VX1700_MODE_CW_N		0x03
#define	VX1700_MODE_AM			0x04
#define	VX1700_MODE_RTTY		0x05

// Operation Mode Selection
#define	VX1700_OP_MODE_LSB		0x00
#define	VX1700_OP_MODE_USB		0x01
#define	VX1700_OP_MODE_CW_W		0x02
#define	VX1700_OP_MODE_CW_N		0x03
#define	VX1700_OP_MODE_AM		0x04
#define	VX1700_OP_MODE_RTTY_LSB_W	0x08
#define	VX1700_OP_MODE_RTTY_USB_W	0x09
#define	VX1700_OP_MODE_H3E		0x0d
#define	VX1700_OP_MODE_RTTY_LSB_N	0x0e
#define	VX1700_OP_MODE_RTTY_USB_N	0x0f

// Status Flag 1 Masks
#define	VX1700_SF_LOCKED		0x01	/* LOCK is activated */
#define	VX1700_SF_MEM			0x20	/* Memory Mode       */
#define	VX1700_SF_VFO			0x80	/* VFO Mode          */

// Status Flag 2 Masks
#define	VX1700_SF_PTT_BY_CAT		0x01	/* PTT closed by CAT           */
#define	VX1700_SF_MEM_SCAN_PAUSE	0x02	/* Scanning paused             */
#define	VX1700_SF_MEM_SCAN		0x04	/* Scanning enabled            */
#define	VX1700_SF_RTTY_FILTER_NARROW	0x08	/* Narrow RTTY filter selected */
#define	VX1700_SF_CW_FILTER_NARROW	0x10	/* Narrow CW filter selected   */
#define	VX1700_SF_RTTY_USB		0x20	/* USB selected for RTTY       */

// Status Flag 3 Masks
#define	VX1700_SF_10W_TX		0x20	/* 10 Watt TX output selected */
#define	VX1700_SF_TUNER_ON		0x20	/* Antenna Tuner working      */
#define	VX1700_SF_TRANSMISSION_ON	0x80	/* Transmission in progress   */


/* HAMLIB API implementation */
static int          vx1700_init(RIG *rig);
static int          vx1700_open(RIG *rig);
static int          vx1700_cleanup(RIG *rig);
static const char * vx1700_get_info(RIG *rig);

static int vx1700_set_vfo(RIG *rig, vfo_t vfo);
static int vx1700_get_vfo(RIG *rig, vfo_t *vfo);
static int vx1700_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int vx1700_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int vx1700_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int vx1700_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int vx1700_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int vx1700_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int vx1700_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int vx1700_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int vx1700_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int vx1700_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int vx1700_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int vx1700_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int vx1700_set_mem(RIG *rig, vfo_t vfo, int ch);
static int vx1700_get_mem(RIG *rig, vfo_t vfo, int *ch);
static int vx1700_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

#endif /* _VX1700_H */
