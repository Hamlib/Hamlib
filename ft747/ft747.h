/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft747.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar
 *
 *
 *    $Id: ft747.h,v 1.10 2000-09-24 03:36:47 javabear Exp $  
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


#ifndef _FT747_H
#define _FT747_H 1

#define FT747_CMD_LENGTH      5

/*
 * future - private data
 *
 */

struct ft747_priv_data {
  int dummy;			/* for test */
};

/* 
 * API local implementation 
 */

int ft747_init(RIG *rig);
int ft747_cleanup(RIG *rig);

int ft747_set_freq(RIG *rig, freq_t freq);
int ft747_get_freq(RIG *rig, freq_t *freq);

int ft747_set_mode(RIG *rig, rmode_t mode); /* select mode */
int ft747_get_mode(RIG *rig, rmode_t *mode); /* get mode */

int ft747_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
int ft747_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

int ft747_set_ptt(RIG *rig, ptt_t ptt);
int ft747_get_ptt(RIG *rig, ptt_t *ptt);


/*
 * Allow TX commands to be disabled
 *
 */

#undef TX_ENABLED


/* MODES - when setting modes via cmd_mode_set() */

#define MODE_SET_LSB    0x00
#define MODE_SET_USB    0x01
#define MODE_SET_CWW    0x02
#define MODE_SET_CWN    0x03
#define MODE_SET_AMW    0x04
#define MODE_SET_AMN    0x05
#define MODE_SET_FMW    0x06
#define MODE_SET_FMN    0x07


/*
 * Status Flags
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
 * Mode Bitmap. Bits 5 and 6 unused
 * When reading modes
 */

#define MODE_FM    0x01
#define MODE_AM    0x02
#define MODE_CW    0x04

#define MODE_USB   0x08
#define MODE_LSB   0x10
#define MODE_NAR   0x80

/*
 * Map band data value to band.
 *
 * Band "n" is from band_data[n] to band_data[n+1]
 */

const float band_data[11] = { 0.0, 0.1, 2.5, 4.0, 7.5, 10.5, 14.5, 18.5, 21.5, 25.0, 30.0 };


/*
 * Visible functions in shared lib.
 *
 */

/*
 * set commands
 */



#if 0

void cmd_set_split_yes(int fd);
void cmd_set_split_no(int fd);
void cmd_set_recall_memory(int fd, int mem);
void cmd_set_vfo_to_memory(int fd, int mem);
void cmd_set_dlock_off(int fd);
void cmd_set_dlock_on(int fd);
void cmd_set_select_vfo_a(int fd);
void cmd_set_select_vfo_b(int fd);
void cmd_set_memory_to_vfo(int fd, int mem);
void cmd_set_up500k(int fd);
void cmd_set_down500k(int fd);
void cmd_set_clarify_off(int fd);
void cmd_set_clarify_on(int fd);

/*
 *void cmd_set_freq(int fd, unsigned int freq);
 */

void cmd_set_mode(int fd, int mode);
void cmd_set_pacing(int fd, int delay);
void cmd_set_ptt_off(int fd);
void cmd_set_ptt_on(int fd);	/* careful.. */

/*
 * get commands
 */

void cmd_get_update_store(int fd, unsigned char *buffer); /* data external */

#endif


#endif /* _FT747_H */
