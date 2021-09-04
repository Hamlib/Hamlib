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

#define FT900_POST_WRITE_DELAY               50

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

#endif /* _FT900_H */
