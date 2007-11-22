/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft450.h - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-450 using the "CAT" interface
 *
 *
 *    $Id: ft450.h,v 1.1 2007-11-22 04:48:43 n0nb Exp $
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


#ifndef _FT450_H
#define _FT450_H 1

#define TRUE    1
#define FALSE   0
#define ON      TRUE
#define OFF     FALSE

#define FT450_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

/* Receiver caps */

#define FT450_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT450_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT450_AM_RX_MODES (RIG_MODE_AM)
#define FT450_FM_RX_MODES (RIG_MODE_FM)


/* TX caps */

#define FT450_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB ) /* 100 W class */
#define FT450_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */
#define FT450_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN) /* fix */


/*
 * Other features (used by rig_caps)
 *
 */

#define FT450_ANTS 0

#define FT450_MEM_CHNL_LENGTH           1       /* 0x10 P1 = 01 return size */
#define FT450_OP_DATA_LENGTH            19      /* 0x10 P1 = 03 return size */
#define FT450_VFO_DATA_LENGTH           18      /* 0x10 P1 = 03 return size -- A & B returned */
#define FT450_MEM_CHNL_DATA_LENGTH      19      /* 0x10 P1 = 04, P4 = 0x01-0x20 return size */
#define FT450_STATUS_FLAGS_LENGTH       5       /* 0xf7, 0xfa return size */
#define FT450_ALL_DATA_LENGTH           649     /* 0x10 P1 = 00 return size */

/* Timing values in mS */

#define FT450_PACING_INTERVAL                5
#define FT450_PACING_DEFAULT_VALUE           0
#define FT450_WRITE_DELAY                    50


/* Delay sequential fast writes */

#define FT450_POST_WRITE_DELAY               5


/*
 * API local implementation
 *
 */

//static int ft450_init(RIG *rig);
//static int ft450_cleanup(RIG *rig);
//static int ft450_open(RIG *rig);
//static int ft450_close(RIG *rig);

//static int ft450_set_freq(RIG *rig, vfo_t vfo, freq_t freq);

//static int ft450_set_vfo(RIG *rig, vfo_t vfo);

#endif /* _FT450_H */
