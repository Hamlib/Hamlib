/*
 * ft1000mp.h - (C) Stephane Fillod 2002 (fillods@users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-1000MP using the "CAT" interface
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


#ifndef _FT1000MP_H
#define _FT1000MP_H 1

#define FT1000MP_STATUS_FLAGS_LENGTH      5          /* 0xfa return size */
#define FT1000MP_STATUS_UPDATE_LENGTH         16         /* 0x10 U = 02 return size */

#define FT1000MP_PACING_INTERVAL                5
#define FT1000MP_PACING_DEFAULT_VALUE           0
#define FT1000MP_WRITE_DELAY                    50


/* Delay sequential fast writes */

#define FT1000MP_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT1000MP_DEFAULT_READ_TIMEOUT  28 * ( 3 + (FT1000MP_PACING_INTERVAL * FT1000MP_PACING_DEFAULT_VALUE))


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
 * Native FT1000MP functions. More to come :-)
 *
 */

enum ft1000mp_native_cmd_e {
  FT1000MP_NATIVE_SPLIT_OFF = 0,
  FT1000MP_NATIVE_SPLIT_ON,
  FT1000MP_NATIVE_RECALL_MEM,
  FT1000MP_NATIVE_VFO_TO_MEM,
  FT1000MP_NATIVE_VFO_A,
  FT1000MP_NATIVE_VFO_B,
  FT1000MP_NATIVE_M_TO_VFO,
  FT1000MP_NATIVE_RIT_ON,
  FT1000MP_NATIVE_RIT_OFF,
  FT1000MP_NATIVE_XIT_ON,
  FT1000MP_NATIVE_XIT_OFF,
  FT1000MP_NATIVE_FREQA_SET,
  FT1000MP_NATIVE_FREQB_SET,
  FT1000MP_NATIVE_MODE_SET_LSB,
  FT1000MP_NATIVE_MODE_SET_USB,
  FT1000MP_NATIVE_MODE_SET_CW,
  FT1000MP_NATIVE_MODE_SET_CWR,
  FT1000MP_NATIVE_MODE_SET_AM,
  FT1000MP_NATIVE_MODE_SET_AMS,
  FT1000MP_NATIVE_MODE_SET_FM,
  FT1000MP_NATIVE_MODE_SET_FMW,
  FT1000MP_NATIVE_MODE_SET_RTTY_LSB,
  FT1000MP_NATIVE_MODE_SET_RTTY_USB,
  FT1000MP_NATIVE_MODE_SET_DATA_LSB,
  FT1000MP_NATIVE_MODE_SET_DATA_FM,
  FT1000MP_NATIVE_PACING,
  FT1000MP_NATIVE_PTT_OFF,
  FT1000MP_NATIVE_PTT_ON,
  FT1000MP_NATIVE_VFO_UPDATE,
  FT1000MP_NATIVE_CURR_VFO_UPDATE,
  FT1000MP_NATIVE_UPDATE,
  FT1000MP_NATIVE_SIZE            /* end marker, value indicates number of */
				                /* native cmd entries */

};

typedef enum ft1000mp_native_cmd_e ft1000mp_native_cmd_t;


/*
 * Internal MODES - when setting modes via cmd_mode_set()
 *
 */

#define MODE_SET_LSB	0x00
#define MODE_SET_USB	0x01
#define MODE_SET_CW	0x02
#define MODE_SET_CWR	0x03
#define MODE_SET_AM	0x04
#define MODE_SET_AMS	0x05
#define MODE_SET_FM	0x06
#define MODE_SET_FMW	0x07	/* what width is that? */
#define MODE_SET_RTTYL	0x08
#define MODE_SET_RTTYU	0x09
#define MODE_SET_PKTL	0x0a
#define MODE_SET_PKTF	0x0b


/*
 * Mode Bitmap.
 * When READING modes
 *
 */

#define MODE_LSB     0x00
#define MODE_USB     0x01
#define MODE_CW      0x02
#define MODE_AM      0x03
#define MODE_FM      0x04
#define MODE_RTTY    0x05
#define MODE_PKT     0x06

/* relevant bits: 5,6,7 */
#define MODE_MASK   0x07


/*
 * Status Flag Masks when reading
 *
 */

#define SF_DLOCK   0x01
#define SF_SPLITA  0x01
#define SF_SPLITB  0x02
#define SF_CLAR    0x04
#define SF_VFOAB   0x10         /* Status byte 0, bit 4, VFO A == 0, VFO B == 1 */
#define SF_VFOMR   0x10
#define SF_RXTX    0x20
#define SF_RESV    0x40
#define SF_PRI     0x80


/*
 * Local VFO CMD's, according to spec
 *
 */

#define FT1000MP_VFO_A                  0x00
#define FT1000MP_VFO_B                  0x01


/*
 * Some useful offsets in the status update map (offset)
 *
 */

#define FT1000MP_SUMO_DISPLAYED_STATUS           0x00
#define FT1000MP_SUMO_DISPLAYED_FREQ             0x01
#define FT1000MP_SUMO_VFO_A_FREQ                 0x01
#define FT1000MP_SUMO_VFO_B_FREQ                 0x11
#define FT1000MP_SUMO_VFO_A_CLAR                 0x05
#define FT1000MP_SUMO_VFO_B_CLAR                 0x15
#define FT1000MP_SUMO_VFO_A_MODE                 0x07
#define FT1000MP_SUMO_VFO_B_MODE                 0x17
#define FT1000MP_SUMO_VFO_A_IF                   0x08
#define FT1000MP_SUMO_VFO_B_IF                   0x18

/* mask extra mode bit from IF Filter status byte in VFO status
   block */
#define IF_MODE_MASK 0x80


/*
 * API local implementation
 *
 */

int ft1000mp_init(RIG *rig);
int ft1000mp_cleanup(RIG *rig);
int ft1000mp_open(RIG *rig);
int ft1000mp_close(RIG *rig);

int ft1000mp_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ft1000mp_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

int ft1000mp_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
int ft1000mp_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

int ft1000mp_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
int ft1000mp_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

int ft1000mp_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int ft1000mp_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int ft1000mp_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
int ft1000mp_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);

int ft1000mp_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int ft1000mp_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);

#endif /* _FT1000MP_H */
