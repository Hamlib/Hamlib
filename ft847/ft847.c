/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft847.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 * $Id: ft847.c,v 1.8 2000-07-29 20:30:33 javabear Exp $  
 *
 */

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include "serial.h"
#include "ft847.h"

static unsigned char datain[5]; /* data read from rig */

static long int  calc_freq_from_packed4(unsigned char *in);



/*
 * Function definitions below
 */


/*
 * Open serial connection to rig.
 * returns fd.
 */


int rig_open(char *serial_port) {
  return open_port(serial_port);
}

/*
 * Closes serial connection to rig.
 * 
 */

int rig_close(int fd) {
  return close_port(fd);
}

/*
 * Implement OPCODES
 */

void cmd_cat_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x00 }; /* cat = on */
  write_block(fd,data);
}

void cmd_cat_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x80 }; /* cat = off */
  write_block(fd,data);
}

void cmd_ptt_on(int fd) {

#ifdef TX_ENABLED
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0f }; /* ptt = on */
  write_block(fd,data);
  printf("cmd_ptt_on complete \n");
#elsif
  printf("cmd_ptt_on disabled \n");
#endif

}


void cmd_ptt_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x88 }; /* ptt = off */
  write_block(fd,data);
}

void cmd_sat_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x4e }; /* sat = on */
  write_block(fd,data);
}

void cmd_sat_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x8e }; /* sat = off */
  write_block(fd,data);
}

void cmd_set_freq_main_vfo(int fd, unsigned char d1,  unsigned char d2, 
			   unsigned char d3, unsigned char d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x01 }; /* set freq, main vfo*/

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;
  write_block(fd,data);
}

void cmd_set_freq_sat_rx_vfo(int fd, unsigned char d1,  unsigned char d2, 
			     unsigned char d3, unsigned char  d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x11 }; /* set freq, sat rx vfo*/

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;
  write_block(fd,data);
}

void cmd_set_freq_sat_tx_vfo(int fd, unsigned char d1,  unsigned char d2, 
			     unsigned char d3, unsigned char  d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x21 }; /* set freq, sat tx vfo*/

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;
  write_block(fd,data);
}

void cmd_set_opmode_main_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x07 }; /* set opmode,  main vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_opmode_sat_rx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x17 }; /* set opmode, sat rx vfo */

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_opmode_sat_tx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x27 }; /* set opmode, sat tx vfo */

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_dcs_main_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0a }; /* set ctcss/dcs,  main vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_dcs_sat_rx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x1a }; /* set ctcss/dcs, sat rx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_dcs_sat_tx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x2a }; /* set ctcss/dcs, sat tx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_freq_main_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0b }; /* set ctcss freq,  main vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_freq_sat_rx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x1b }; /* set ctcss freq, sat rx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_freq_sat_tx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x2b }; /* set ctcss freq, sat rx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_dcs_code_main_vfo(int fd, unsigned char d1, unsigned char d2) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0c }; /* set dcs code,  main vfo*/

  data[0] = d1;
  data[1] = d2;
  write_block(fd,data);
}

void cmd_set_dcs_code_sat_rx_vfo(int fd, unsigned char d1, unsigned char d2) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x1c }; /* set dcs code, sat rx vfo*/

  data[0] = d1;
  data[1] = d2;
  write_block(fd,data);
}

void cmd_set_dcs_code_sat_tx_vfo(int fd, unsigned char d1, unsigned char d2) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x2c }; /* set dcs code, sat tx vfo*/

  data[0] = d1;
  data[1] = d2;
  write_block(fd,data);
}

void cmd_set_repeater_shift_minus(int fd) {
  static unsigned char data[] = { 0x09, 0x00, 0x00, 0x00, 0x09 }; /* set repeater shift minus */

  write_block(fd,data);
}

void cmd_set_repeater_shift_plus(int fd) {
  static unsigned char data[] = { 0x49, 0x00, 0x00, 0x00, 0x09 }; /* set repeater shift  */

  write_block(fd,data);
}

void cmd_set_repeater_shift_simplex(int fd) {
  static unsigned char data[] = { 0x89, 0x00, 0x00, 0x00, 0x09 }; /* set repeater simplex  */

  write_block(fd,data);
}

void cmd_set_repeater_offset(int fd, unsigned char d1,  unsigned char d2, 
			     unsigned char d3, unsigned char  d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xf9 }; /* set repeater offset  */

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;
  write_block(fd,data);
}


/*
 * Get data rx from the RIG...only 1 byte
 *
 */


unsigned char cmd_get_rx_status(int fd) {
  int n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xe7 }; /* get receiver status */

  write_block(fd,data);
  n = read_sleep(fd,datain,1);	/* wait and read for 1 byte to be read */

  printf("datain[0] = %x \n",datain[0]);

  return datain[0];

}

/*
 * Get data tx from the RIG...only 1 byte
 *
 */

unsigned char cmd_get_tx_status(int fd) {
  int n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xf7 }; /* get tx status */

  write_block(fd,data);
  n = read_sleep(fd,datain,1);	/* wait and read for 1 byte to be read */

  printf("datain[0] = %x \n",datain[0]);

  return datain[0];

}

/*
 * Get freq and mode data from the RIG main VFO...only 5 bytes
 *
 */

long int cmd_get_freq_mode_status_main_vfo(int fd, unsigned char *mode) {
  long int f;
  
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x03 }; /* get freq and mode status */
								  /* main vfo*/
  write_block(fd,data);
  read_sleep(fd,datain,5);	/* wait and read for 5 byte to be read */
  f = calc_freq_from_packed4(datain); /* 1st 4 bytes */
  *mode = datain[4];		      /* last byte */
  
  return f;			/* return freq in Hz */

}

/*
 * Get freq and mode data from the RIG  sat RX vfo ...only 5 bytes
 *
 */

long int cmd_get_freq_mode_status_sat_rx_vfo(int fd, unsigned char *mode ) {
  long int f;

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x13 }; /* get freq and mode status */
								  /* sat rx vfo*/
  write_block(fd,data);
  read_sleep(fd,datain,5);	/* wait and read for 5 byte to be read */
  f = calc_freq_from_packed4(datain); /* 1st 4 bytes */
  *mode = datain[4];		      /* last byte */
  
  printf("frequency/mode = %.2x%.2x%.2x%.2x %.2x \n",  datain[0], datain[1], datain[2],datain[3],datain[4]); 

  return f;			/* return freq in Hz */

}

/*
 * Get freq and mode data from the RIG sat TX VFO...only 5 bytes
 *
 */

long int cmd_get_freq_mode_status_sat_tx_vfo(int fd, unsigned char *mode) {
  long int f;

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x23 }; /* get freq and mode status */
								  /* sat tx vfo*/
  write_block(fd,data);
  read_sleep(fd,datain,5);	/* wait and read for 5 byte to be read */
  f = calc_freq_from_packed4(datain); /* 1st 4 bytes */
  *mode = datain[4];		      /* last byte */

  return f;			/* return freq in Hz */

}



/*
 * Set frequency in Hz.
 *
 */

void cmd_set_freq_main_vfo_hz(unsigned long int freq) {
  unsigned char d1,d2,d3,d4;

 

}


/*
 * Private helper functions....
 *
 */


/*
 * Decode decimal from packed decimal (4 bytes, 8 digits) 
 * and return frequency in Hz.
 *
 */

static long int  calc_freq_from_packed4(unsigned char *in) {
  long int f;			/* frequnecy in Hz */
  unsigned char sfreq[9];	/* 8 digits + \0 */

  printf("frequency/mode = %.2x%.2x%.2x%.2x %.2x \n",  datain[0], datain[1], datain[2],datain[3],datain[4]); 

  snprintf(sfreq,9,"%.2x%.2x%.2x%.2x",in[0], in[1], in[2], in[3]);
  f = atol(sfreq);
  f = f *10;			/* yaesu returns freq to 10 Hz, so must */
				/* scale to return Hz*/
  printf("frequency = %ld  Hz\n",  f); 
     
  return f;			/* Hz */
}



