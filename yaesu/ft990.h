/*
 * hamlib - (C) Stephane Fillod 2002, 2003 (fillods at users.sourceforge.net)
 *
 * ft990.h - 
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-990 using the "CAT" interface
 *
 *
 *    $Id: ft990.h,v 1.1 2003-10-12 18:04:02 fillods Exp $  
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */


#ifndef _FT990_H
#define _FT990_H 1

#define FT990_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

/* Receiver caps */

#define FT990_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT990_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT990_AM_RX_MODES (RIG_MODE_AM)
#define FT990_FM_RX_MODES (RIG_MODE_FM)


/* TX caps */

#define FT990_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB ) /* 100 W class */
#define FT990_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */
#define FT990_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN) /* fix */


/*
 * Other features (used by rig_caps)
 *
 */

#define FT990_ANTS 0

/* Returned data length in bytes */

#define FT990_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT990_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FT990_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT990_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FT990_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FT990_ALL_DATA_LENGTH           1941    /* 0x10 P1 = 00 return size */

/* Timing values in mS */

#define FT990_PACING_INTERVAL                5
#define FT990_PACING_DEFAULT_VALUE           0
#define FT990_WRITE_DELAY                    50


/* Delay sequential fast writes */

#define FT990_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT990_DEFAULT_READ_TIMEOUT  1941 * ( 5 + (FT990_PACING_INTERVAL * FT990_PACING_DEFAULT_VALUE))


#endif /* _FT990_H */
