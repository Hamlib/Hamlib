/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft847.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 *    $Id: ft847.h,v 1.16 2000-10-09 01:17:19 javabear Exp $  
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

#ifndef _FT847_H
#define _FT847_H 1


#define FT847_CMD_LENGTH                     5
#define FT847_WRITE_DELAY                    50

/* Sequential fast writes may confuse FT847 without this delay */

#define FT847_POST_WRITE_DELAY               50 


/* Rough safe value for default timeout */

#define FT847_DEFAULT_READ_TIMEOUT           2000



/*
 * future - private data
 *
 */

struct ft847_priv_data {
  int dummy;			/* for test */
};



/* 
 * API local implementation 
 */

int ft847_init(RIG *rig);
int ft847_open(RIG *rig);

int ft847_cleanup(RIG *rig);
int ft847_close(RIG *rig);

int ft847_set_freq(RIG *rig, freq_t freq);
int ft847_get_freq(RIG *rig, freq_t *freq);

int ft847_set_mode(RIG *rig, rmode_t mode); /* select mode */
int ft847_get_mode(RIG *rig, rmode_t *mode); /* get mode */

int ft847_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
int ft847_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

int ft847_set_ptt(RIG *rig, ptt_t ptt);
int ft847_get_ptt(RIG *rig, ptt_t *ptt);




#if 0

/*
 * Allow TX commands to be disabled
 *
 */

#undef TX_ENABLED

/*
 * RX Status Flags
 */

const unsigned char  RXSF_DISC_CENTER     = (1<<5);
const unsigned char  RXSF_CTCSS_DCS_CODE  = (1<<6);
const unsigned char  RXSF_SQUELCH_STATUS  = (1<<7);
const unsigned char  RXSF_SMETER_MASK     = 0x1f; /* bottom 5 bits */

/*
 * TX Status Flags
 */

const unsigned char  TXSF_PTT_STATUS        = (1<<7);
const unsigned char  TXSF_POALC_METER_MASK  = 0x1f; /* bottom 5 bits */



/*
 * MODES for READING and SETTING
 */

#define MODE_LSB   0x00  
#define MODE_USB   0x01
#define MODE_CW    0x02
#define MODE_CWR   0x03
#define MODE_AM    0x04
#define MODE_FM    0x08
#define MODE_CWN   0x82
#define MODE_CWNR  0x83
#define MODE_AMN   0x84
#define MODE_FMN   0x88

/*
 * Modes for setting CTCSS/DCS Mode
 *
 */

const unsigned char DCS_ON            = 0x0a;
const unsigned char CTCSS_ENC_DEC_ON  = 0x2a; 
const unsigned char CTCSS_ENC_ON      = 0x4a; 
const unsigned char CTCSS_ENC_DEC_OFF = 0x2a; 




/*
 * Raw CAT command set
 *
 */



void cmd_set_cat_on(int fd);
void cmd_set_cat_off(int fd);

void cmd_set_ptt_on(int fd);
void cmd_set_ptt_off(int fd);

void cmd_set_sat_on(int fd);
void cmd_set_sat_off(int fd);

void cmd_set_freq_main_vfo(int fd, unsigned char d1,  unsigned char d2,
			   unsigned char d3, unsigned char d4);

void cmd_set_freq_sat_rx_vfo(int fd, unsigned char d1,  unsigned char d2,
			   unsigned char d3, unsigned char d4);

void cmd_set_freq_sat_tx_vfo(int fd, unsigned char d1,  unsigned char d2,
			   unsigned char d3, unsigned char d4);

void cmd_set_opmode_main_vfo(int fd, unsigned char d1);
void cmd_set_opmode_sat_rx_vfo(int fd, unsigned char d1);
void cmd_set_opmode_sat_tx_vfo(int fd, unsigned char d1);

void cmd_set_ctcss_dcs_main_vfo(int fd, unsigned char d1);
void cmd_set_ctcss_dcs_sat_rx_vfo(int fd, unsigned char d1);
void cmd_set_ctcss_dcs_sat_tx_vfo(int fd, unsigned char d1);

void cmd_set_ctcss_freq_main_vfo(int fd, unsigned char d1);
void cmd_set_ctcss_freq_sat_rx_vfo(int fd, unsigned char d1);
void cmd_set_ctcss_freq_sat_tx_vfo(int fd, unsigned char d1);

void cmd_set_dcs_code_main_vfo(int fd, unsigned char d1, unsigned char d2);
void cmd_set_dcs_code_sat_rx_vfo(int fd, unsigned char d1, unsigned char d2);
void cmd_set_dcs_code_sat_tx_vfo(int fd, unsigned char d1, unsigned char d2);

void cmd_set_repeater_shift_minus(int fd);
void cmd_set_repeater_shift_plus(int fd);
void cmd_set_repeater_shift_simplex(int fd);

void cmd_set_repeater_offset(int fd, unsigned char d1,  unsigned char d2,
			   unsigned char d3, unsigned char d4);

unsigned char cmd_get_rx_status(int fd);
unsigned char cmd_get_tx_status(int fd);

/*
 * Get frequency and mode info
 *
 */


long int cmd_get_freq_mode_status_main_vfo(int fd, unsigned char *mode);
long int cmd_get_freq_mode_status_sat_rx_vfo(int fd, unsigned char *mode);
long int cmd_get_freq_mode_status_sat_tx_vfo(int fd, unsigned char *mode);


/*
 * Set frequency in Hz and mode
 *
 */

void cmd_set_freq_main_vfo_hz(int fd,long int freq, unsigned char mode);
void cmd_set_freq_sat_rx_vfo_hz(int fd,long int freq, unsigned char mode);
void cmd_set_freq_sat_tx_vfo_hz(int fd,long int freq, unsigned char mode);


/*
 * Set Repeater offset in Hz.
 *
 */

void cmd_set_repeater_offset_hz(int fd,long int freq);

#endif /* 0 */



#endif /* _FT847_H */









