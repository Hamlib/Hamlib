/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft890.h - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *           (C) Stephane Fillod 2002, 2003 (fillods at users.sourceforge.net)
 *           (C) Nate Bargmann 2002, 2003 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-890 using the "CAT" interface
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


#ifndef _FT890_H
#define _FT890_H 1

#define TRUE    1
#define FALSE   0
#define ON      TRUE
#define OFF     FALSE

#define FT890_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

/* Receiver caps */

#define FT890_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT890_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT890_AM_RX_MODES (RIG_MODE_AM)
#define FT890_FM_RX_MODES (RIG_MODE_FM)


/* TX caps */

#define FT890_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB ) /* 100 W class */
#define FT890_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */
#define FT890_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN) /* fix */


/*
 * Other features (used by rig_caps)
 *
 */

#define FT890_ANTS 0

/* Returned data length in bytes */

#define FT890_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT890_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FT890_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT890_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FT890_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FT890_ALL_DATA_LENGTH           649     /* 0x10 P1 = 00 return size */

/* Timing values in mS */

#define FT890_PACING_INTERVAL                5
#define FT890_PACING_DEFAULT_VALUE           0
#define FT890_WRITE_DELAY                    50


/* Delay sequential fast writes */

#define FT890_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT890_DEFAULT_READ_TIMEOUT  649 * ( 5 + (FT890_PACING_INTERVAL * FT890_PACING_DEFAULT_VALUE))

/* BCD coded frequency length */
#define FT890_BCD_DIAL  8
#define FT890_BCD_RIT   3


/*
 * 8N2 and 1 start bit = 11 bits at 4800 bps => effective byte
 * rate = 1 byte in 2.2917 msec => 649 bytes in 1487 msec
 *
 * delay for 28 bytes = (2.2917 + pace_interval) * 28
 *
 * pace_interval          time to read 28 bytes
 * ------------           ----------------------
 *
 *     0                       1487 msec
 *     1                       2136 msec    (backend default)
 *     2                       2785 msec
 *     5                       4732 msec
 *    255                      167 sec
 *
 */


#endif /* _FT890_H */
