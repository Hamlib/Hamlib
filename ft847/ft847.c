/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft847.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 * $Id: ft847.c,v 1.3 2000-07-26 00:34:25 javabear Exp $  
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



unsigned char cmd_get_rx_status(int fd) {
  int bytes;			/* read from rig */
  int i,n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xe7 }; /* get receiver status */

  write_block(fd,data);

  /*
   * Sleep regularly until the buffer contains 1 byte
   * This should handle most cases.
   */

  bytes = 0;
  while(1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    if (bytes == 1)
      break;
    sleep(1);
    
  }

  /* this should not block now */
  
  n = read(fd,datain,1);	/* grab 1 byte from rig */

  printf("Byte read = %x \n", datain[0]);

  return datain[0];


}

unsigned char cmd_get_tx_status(int fd) {
  int bytes;			/* read from rig */
  int i,n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xf7 }; /* get tx status */

  write_block(fd,data);

  /*
   * Sleep regularly until the buffer contains all 1 bytes
   * This should handle most cases.
   */

  bytes = 0;
  while(bytes < 1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    sleep(1);
    
  }

  /* this should not block now */
  
  n = read(fd,datain,1);	/* grab 1 byte from rig */

  printf("i = %i ,datain[i] = %x \n", i, datain[i]);

  return datain[0];

}

unsigned char cmd_get_freq_mode_status_main_vfo(int fd) {
  int bytes;			/* read from rig */
  int i,n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x03 }; /* get freq and mode status */
								  /* main vfo*/

  write_block(fd,data);

  /*
   * Sleep regularly until the buffer contains all 1 bytes
   * This should handle most cases.
   */

  bytes = 0;
  while(bytes < 1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    sleep(1);
    
  }

  /* this should not block now */
  
  n = read(fd,datain,1);	/* grab 1 byte from rig */

  printf("i = %i ,datain[i] = %x \n", i, datain[i]);

  return datain[0];

}

unsigned char cmd_get_freq_mode_status_sat_rx_vfo(int fd) {
  int bytes;			/* read from rig */
  int i,n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x13 }; /* get freq and mode status */
								  /* sat rx vfo*/

  write_block(fd,data);

  /*
   * Sleep regularly until the buffer contains all 1 bytes
   * This should handle most cases.
   */

  bytes = 0;
  while(bytes < 1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    sleep(1);
    
  }

  /* this should not block now */
  
  n = read(fd,datain,1);	/* grab 1 byte from rig */

  printf("i = %i ,datain[i] = %x \n", i, datain[i]);

  return datain[0];

}

unsigned char cmd_get_freq_mode_status_sat_tx_vfo(int fd) {
  int bytes;			/* read from rig */
  int i,n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x13 }; /* get freq and mode status */
								  /* sat tx vfo*/

  write_block(fd,data);

  /*
   * Sleep regularly until the buffer contains all 1 bytes
   * This should handle most cases.
   */

  bytes = 0;
  while(bytes < 1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    sleep(1);
    
  }

  /* this should not block now */
  
  n = read(fd,datain,1);	/* grab 1 byte from rig */

  printf("i = %i ,datain[i] = %x \n", i, datain[i]);

  return datain[0];

}





