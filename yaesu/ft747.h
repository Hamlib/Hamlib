/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2010
 *
 * ft747.h - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar (max232 + some capacitors :-)
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


#ifndef _FT747_H
#define _FT747_H 1

/*
 * According to manual, the UPDATE data length should be 345
 * but some rigs are short by one byte.
 */
#define FT747_STATUS_UPDATE_DATA_LENGTH      344

#define FT747_PACING_DEFAULT_VALUE           0
#define FT747_WRITE_DELAY                    5 /* manual say 50 ms, but it doesn't work though */


/* Sequential fast writes confuse my FT747 without this delay */

#define FT747_POST_WRITE_DELAY               5


/*
 * 8N2 and 1 start bit = 11 bits at 4800 bps => effective byte rate = 1 byte in 2.2917 msec
 * => 345 bytes in 790 msec
 *
 * delay for 1 byte = 2.2917 + (pace_interval * 5)
 *
 * pace_interval          time to read 345 bytes
 * ------------           ----------------------
 *
 *     0                       790 msec
 *     1                       2515 msec
 *     2                       4240 msec
 *    255                      441 sec => 7 min 21 seconds
 *
 */



/*
 * Native FT747 functions. This is what I have to work with :-)
 *
 */

enum ft747_native_cmd_e {
  FT_747_NATIVE_SPLIT_OFF = 0,
  FT_747_NATIVE_SPLIT_ON,
  FT_747_NATIVE_RECALL_MEM,
  FT_747_NATIVE_VFO_TO_MEM,
  FT_747_NATIVE_DLOCK_OFF,
  FT_747_NATIVE_DLOCK_ON,
  FT_747_NATIVE_VFO_A,
  FT_747_NATIVE_VFO_B,
  FT_747_NATIVE_M_TO_VFO,
  FT_747_NATIVE_UP_500K,
  FT_747_NATIVE_DOWN_500K,
  FT_747_NATIVE_CLARIFY_OFF,
  FT_747_NATIVE_CLARIFY_ON,
  FT_747_NATIVE_FREQ_SET,
  FT_747_NATIVE_MODE_SET_LSB,
  FT_747_NATIVE_MODE_SET_USB,
  FT_747_NATIVE_MODE_SET_CWW,
  FT_747_NATIVE_MODE_SET_CWN,
  FT_747_NATIVE_MODE_SET_AMW,
  FT_747_NATIVE_MODE_SET_AMN,
  FT_747_NATIVE_MODE_SET_FMW,
  FT_747_NATIVE_MODE_SET_FMN,
  FT_747_NATIVE_PACING,
  FT_747_NATIVE_PTT_OFF,
  FT_747_NATIVE_PTT_ON,
  FT_747_NATIVE_UPDATE,
  FT_747_NATIVE_SIZE		/* end marker, value indicates number of */
				/* native cmd entries */

};

typedef enum ft747_native_cmd_e ft747_native_cmd_t;



/* Internal MODES - when setting modes via cmd_mode_set() */

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
 */

#define MODE_FM     0x01
#define MODE_AM     0x02
#define MODE_CW     0x04
#define MODE_FMN    0x81
#define MODE_AMN    0x82
#define MODE_CWN    0x84
#define MODE_USB    0x08
#define MODE_LSB    0x10
#define MODE_NAR    0x80

/* All relevent bits */
#define MODE_MASK   0x9f


/*
 * Status Flag Masks when reading
 */

#define SF_DLOCK   0x01
#define SF_SPLIT   0x02
#define SF_CLAR    0x04
#define SF_VFOAB   0x08
#define SF_VFOMR   0x10
#define SF_RXTX    0x20
#define SF_RESV    0x40
#define SF_PRI     0x80


/*
 * Local VFO CMD's, according to spec
 */

#define FT747_VFO_A                  0x00
#define FT747_VFO_B                  0x01


/*
 * Some useful offsets in the status update map (offset)
 *
 * Manual appears to be full of mistakes regarding offsets etc.. -- FS
 *
 */

#define FT747_SUMO_DISPLAYED_MEM              0x17
#define FT747_SUMO_DISPLAYED_MODE             0x18
#define FT747_SUMO_DISPLAYED_STATUS           0x00
#define FT747_SUMO_DISPLAYED_FREQ             0x01
#define FT747_SUMO_VFO_A_FREQ                 0x09
#define FT747_SUMO_VFO_B_FREQ                 0x11



/*
 * API local implementation
 */

static int ft747_init(RIG *rig);
static int ft747_cleanup(RIG *rig);
static int ft747_open(RIG *rig);
static int ft747_close(RIG *rig);

static int ft747_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft747_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft747_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
static int ft747_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

static int ft747_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
static int ft747_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

static int ft747_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);

static int ft747_set_split(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int ft747_get_split(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);

static int ft747_set_mem(RIG *rig, vfo_t vfo, int ch);
static int ft747_get_mem(RIG *rig, vfo_t vfo, int *ch);

/*
 * The time the status block is cached (in millisec).
 * This optimises the common case of doing eg. rig_get_freq() and
 * rig_get_mode() in a row.
 *
 * The timeout is set to at least the time to transfer the block (790ms)
 * plus post write delay, plus some margin, but less than 1 second,
 * which may be typical polling period.
 */
#define FT747_CACHE_TIMEOUT     900


#endif /* _FT747_H */
