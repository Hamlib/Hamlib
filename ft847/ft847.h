/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft847.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 *    $Id: ft847.h,v 1.4 2000-07-29 01:47:48 javabear Exp $  
 */

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
 * Visible functions in shared lib.
 *
 */

int rig_open(char *serial_port); /* return fd or -1 on error */
int rig_close(int fd);


/*
 * CAT command set
 *
 */


void cmd_cat_on(int fd);
void cmd_cat_off(int fd);
void cmd_ptt_on(int fd);
void cmd_ptt_off(int fd);
void cmd_sat_on(int fd);
void cmd_sat_off(int fd);

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
unsigned char cmd_get_freq_mode_status_main_vfo(int fd);
unsigned char cmd_get_freq_mode_status_sat_rx_vfo(int fd);
unsigned char cmd_get_freq_mode_status_sat_tx_vfo(int fd); 


/*
 * Set frequency in Hz.
 *
 */

void cmd_set_freq_main_vfo_hz(unsigned long int freq);

