/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft847.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 *    $Id: ft847.h,v 1.2 2001-01-04 07:03:58 javabear Exp $  
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


#define FT847_WRITE_DELAY                    50

/* Sequential fast writes may confuse FT847 without this delay */

#define FT847_POST_WRITE_DELAY               50 


/* Rough safe value for default timeout */

#define FT847_DEFAULT_READ_TIMEOUT           2000



/*
 * Native FT847 functions. This is what I have to work with :-)
 *
 */

enum ft847_native_cmd_e {

  /* Set commands to the rig */

  FT_847_NATIVE_CAT_ON = 0,
  FT_847_NATIVE_CAT_OFF,

  FT_847_NATIVE_CAT_PTT_ON,
  FT_847_NATIVE_CAT_PTT_OFF,

  FT_847_NATIVE_CAT_SAT_MODE_ON,
  FT_847_NATIVE_CAT_SAT_MODE_OFF,

  FT_847_NATIVE_CAT_SET_FREQ_MAIN,
  FT_847_NATIVE_CAT_SET_FREQ_SAT_RX_VFO,
  FT_847_NATIVE_CAT_SET_FREQ_SAT_TX_VFO,

  FT_847_NATIVE_CAT_SET_MODE_MAIN_LSB, /* MAIN VFO */
  FT_847_NATIVE_CAT_SET_MODE_MAIN_USB,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_CW,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_CWR,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_AM,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_FM,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_CWN,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_CWRN,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_AMN,
  FT_847_NATIVE_CAT_SET_MODE_MAIN_FMN,

  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_LSB, /* SAT RX VFO */
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_USB,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CW,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CWR,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_AM,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_FM,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CWN,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CWRN,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_AMN,
  FT_847_NATIVE_CAT_SET_MODE_SAT_RX_FMN,

  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_LSB, /* SAT TX VFO */
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_USB,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CW,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CWR,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_AM,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_FM,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CWN,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CWRN,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_AMN,
  FT_847_NATIVE_CAT_SET_MODE_SAT_TX_FMN,

  FT_847_NATIVE_CAT_SET_DCS_ON_MAIN, /* MAIN CTCSS/DCS */
  FT_847_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON_MAIN,
  FT_847_NATIVE_CAT_SET_CTCSS_ENC_ON_MAIN,
  FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_MAIN,

  FT_847_NATIVE_CAT_SET_DCS_ON_SAT_RX, /* SAT RX CTCSS/DCS */
  FT_847_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON_SAT_RX,
  FT_847_NATIVE_CAT_SET_CTCSS_ENC_ON_SAT_RX,
  FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_SAT_RX,

  FT_847_NATIVE_CAT_SET_DCS_ON_SAT_TX, /* SAT TX CTCSS/DCS */
  FT_847_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON_SAT_TX,
  FT_847_NATIVE_CAT_SET_CTCSS_ENC_ON_SAT_TX,
  FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_SAT_TX,

  FT_847_NATIVE_CAT_SET_CTCSS_FREQ_MAIN, /* CTCSS Freq */
  FT_847_NATIVE_CAT_SET_CTCSS_FREQ_SAT_RX,
  FT_847_NATIVE_CAT_SET_CTCSS_FREQ_SAT_TX,

  FT_847_NATIVE_CAT_SET_DCS_CODE_MAIN, /* DCS code */
  FT_847_NATIVE_CAT_SET_DCS_CODE_SAT_RX,
  FT_847_NATIVE_CAT_SET_DCS_CODE_SAT_TX,

  FT_847_NATIVE_CAT_SET_RPT_SHIFT_MINUS, /* RPT SHIFT */
  FT_847_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
  FT_847_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,

  FT_847_NATIVE_CAT_SET_RPT_OFFSET, /* RPT Offset frequency */

  /* Get info from the rig */

  FT_847_NATIVE_CAT_GET_RX_STATUS,
  FT_847_NATIVE_CAT_GET_TX_STATUS,

  FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_MAIN,
  FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_SAT_RX,
  FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_SAT_TX,

  FT_847_NATIVE_SIZE		/* end marker, value indicates number of */
				/* native cmd entries */

};

typedef enum ft847_native_cmd_e ft847_native_cmd_t;



/*
 * ft847 instance - private data
 *
 */

struct ft847_priv_data {
  unsigned char current_vfo;	/* active VFO from last cmd , can be either RIG_VFO_A, SAT_RX, SAT_TX */
  unsigned char p_cmd[YAESU_CMD_LENGTH]; /* private copy of 1 constructed CAT cmd */
  yaesu_cmd_set_t pcs[FT_847_NATIVE_SIZE];		/* private cmd set */
  unsigned char rx_status;	/* tx returned data */
  unsigned char tx_status;	/* rx returned data */
  unsigned char fm_status_main; /* freq and mode ,returned data */
  unsigned char fm_status_satrx; /* freq and mode ,returned data */
  unsigned char fm_status_sattx; /* freq and mode ,returned data */

};



/* 
 * API local implementation 
 */

int ft847_init(RIG *rig);
int ft847_open(RIG *rig);

int ft847_cleanup(RIG *rig);
int ft847_close(RIG *rig);

int ft847_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ft847_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

int ft847_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
int ft847_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

int ft847_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
int ft847_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

int ft847_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int ft847_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);




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









