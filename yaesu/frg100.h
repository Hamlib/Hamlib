/*
 * frg100.h - (C) Michael Black <mdblack98@gmail.com> 2016
 *
 * This shared library provides an API for communicating
 * via serial interface to an FRG-100 using the "CAT" interface
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


#define FRG100_OP_DATA_LENGTH           19      /* 0x10 p1=02 return size */
#define FRG100_CMD_UPDATE               0x10

#define FRG100_MIN_CHANNEL      1
#define FRG100_MAX_CHANNEL      200


// Return codes
#define FRG100_CMD_RETCODE_OK           0x00
#define FRG100_CMD_RETCODE_ERROR        0xF0

typedef enum frg100_native_cmd_e {
    FRG100_NATIVE_RECALL_MEM = 0,       /* 0x02, p1=ch */
    FRG100_NATIVE_VFO_TO_MEM,           /* 0x03, p1=ch, p2=0 */
    FRG100_NATIVE_MEM_HIDE,             /* 0x03, p1=ch, p2=1 */
    FRG100_NATIVE_VFO_A,                /* 0x05 */
    FRG100_NATIVE_FREQ_SET,             /* 0x0a, p1:4=freq */
    FRG100_NATIVE_MODE_SET_LSB,         /* 0x0c, p1=0x00 */
    FRG100_NATIVE_MODE_SET_USB,         /* 0x0c, p1=0x01 */
    FRG100_NATIVE_MODE_SET_CW_W,        /* 0x0c, p1=0x02 */
    FRG100_NATIVE_MODE_SET_CW_N,        /* 0x0c, p1=0x03 */
    FRG100_NATIVE_MODE_SET_AM,          /* 0x0c, p1=0x04 */
    FRG100_NATIVE_MODE_SET_RTTY_LSB_W,  /* 0x0c, p1=0x08 */
    FRG100_NATIVE_MODE_SET_RTTY_USB_W,  /* 0x0c, p1=0x09 */
    FRG100_NATIVE_MODE_SET_H3E,         /* 0x0c, p1=0x0d */
    FRG100_NATIVE_MODE_SET_RTTY_LSB_N,  /* 0x0c, p1=0x0e */
    FRG100_NATIVE_MODE_SET_RTTY_USB_N,  /* 0x0c, p1=0x0f */
    FRG100_NATIVE_PTT_OFF,              /* 0x0f, p1=0 */
    FRG100_NATIVE_PTT_ON,               /* 0x0f, p1=1 */
    FRG100_NATIVE_UPDATE_MEM_CHNL,      /* 0x10, p1=1 */
    FRG100_NATIVE_UPDATE_OP_DATA,       /* 0x10, p1=2 */
    FRG100_NATIVE_UPDATE_VFO_DATA,      /* 0x10, p1=3 */
    FRG100_NATIVE_TX_POWER_LOW,         /* 0x18 */
    FRG100_NATIVE_TX_POWER_MID,         /* 0x28 */
    FRG100_NATIVE_TX_POWER_HI,          /* 0x48 */
    FRG100_NATIVE_CPY_RX_TO_TX,         /* 0x85 */
    FRG100_NATIVE_TX_FREQ_SET,          /* 0x8a, p1:4=freq */
    FRG100_NATIVE_OP_FREQ_STEP_UP,      /* 0x8e, p1=0 */
    FRG100_NATIVE_OP_FREQ_STEP_DOWN,    /* 0x8e, p1=1 */
    FRG100_NATIVE_READ_METER,           /* 0xf7 */
    FRG100_NATIVE_READ_FLAGS,           /* 0xfa */
    FRG100_NATIVE_SIZE
} frg100_native_cmd_t;

