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
 *    $Id: ft920.h,v 1.5 2002-11-04 05:16:19 n0nb Exp $  
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

#define FT920_STATUS_UPDATE_DATA_LENGTH      8          /* 0xfa return size */
#define FT920_VFO_UPDATE_DATA_LENGTH         28         /* 0x10 P1 = 03 return size */

#define FT920_PACING_INTERVAL                5 
#define FT920_PACING_DEFAULT_VALUE           1
#define FT920_WRITE_DELAY                    50


/* Delay sequential fast writes */

#define FT920_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT920_DEFAULT_READ_TIMEOUT  28 * ( 3 + (FT920_PACING_INTERVAL * FT920_PACING_DEFAULT_VALUE))


/*
 * 8N2 and 1 start bit = 11 bits at 4800 bps => effective byte rate = 1 byte in 2.2917 msec
 * => 28 bytes in 64 msec
 *
 * delay for 1 byte = 2.2917 + (pace_interval * 5) 
 *
 * pace_interval          time to read 345 bytes
 * ------------           ----------------------
 *
 *     0                       64 msec
 *     1                       321 msec
 *     2                       642 msec
 *    255                      16.4 sec
 *
 */



/*
 * Native FT920 functions. More to come :-)
 *
 */

enum ft920_native_cmd_e {
  FT_920_NATIVE_SPLIT_OFF = 0,
  FT_920_NATIVE_SPLIT_ON,
  FT_920_NATIVE_RECALL_MEM,
  FT_920_NATIVE_VFO_TO_MEM,
  FT_920_NATIVE_VFO_A,
  FT_920_NATIVE_VFO_B,
  FT_920_NATIVE_M_TO_VFO,
  FT_920_NATIVE_FREQ_SET,
  FT_920_NATIVE_MODE_SET_LSB,
  FT_920_NATIVE_MODE_SET_USB,
  FT_920_NATIVE_MODE_SET_CW_USB,
  FT_920_NATIVE_MODE_SET_CW_LSB,
  FT_920_NATIVE_MODE_SET_AMW,
  FT_920_NATIVE_MODE_SET_AMN,
  FT_920_NATIVE_MODE_SET_FMW,
  FT_920_NATIVE_MODE_SET_FMN,
  FT_920_NATIVE_MODE_SET_DATA_LSB,
  FT_920_NATIVE_MODE_SET_DATA_LSB1,
  FT_920_NATIVE_MODE_SET_DATA_USB,
  FT_920_NATIVE_MODE_SET_DATA_FM,
  FT_920_NATIVE_PACING,
  FT_920_NATIVE_VFO_UPDATE,
  FT_920_NATIVE_UPDATE,
  FT_920_NATIVE_SIZE            /* end marker, value indicates number of */
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
 * Mode Bitmap. Bits 5 and 6 unused
 * When READING modes
 *
 */

#define MODE_LSB     0x00
#define MODE_CW_L    0x01
#define MODE_AM      0x02
#define MODE_FM      0x03
#define MODE_DATA_L  0x04
#define MODE_DATA_U  0x05
#define MODE_DATA_F  0x06
#define MODE_USB     0x40
#define MODE_CW_U    0x41
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
 * Status Flag Masks when reading
 *
 */

#define SF_DLOCK   0x01
#define SF_SPLITA  0x01
#define SF_SPLITB  0x02
#define SF_CLAR    0x04
#define SF_VFOAB   0x03         /* Status byte 0, bit 4, VFO A == 0, VFO B == 1 */
#define SF_VFOMR   0x10
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
 * Some useful offsets in the status update map (offset) 
 *
 */

#define FT920_SUMO_DISPLAYED_MODE             0x07
#define FT920_SUMO_DISPLAYED_STATUS           0x00    
#define FT920_SUMO_DISPLAYED_FREQ             0x01    
#define FT920_SUMO_VFO_A_FREQ                 0x01
#define FT920_SUMO_VFO_B_FREQ                 0x0f
    


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
