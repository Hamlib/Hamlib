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


#endif /* _FT920_H */
