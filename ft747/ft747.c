/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft747.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar
 *
 *
 * $Id: ft747.c,v 1.7 2000-07-30 05:06:24 javabear Exp $  
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
#include "ft747.h"



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

void cmd_set_split_yes(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x01 }; /* split = on */
  write_block(fd,data);
}

void cmd_set_split_no(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x01 }; /* split = off */
  write_block(fd,data);
}

void cmd_set_recall_memory(int fd, int mem) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x02 }; /* recall memory*/
  
  data[3] = mem;
  write_block(fd,data);
}

void cmd_set_vfo_to_memory(int fd, int mem) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x03 }; /* vfo to  memory*/
  
  data[3] = mem;
  write_block(fd,data);
}

void cmd_set_dlock_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x04 }; /* dial lock = off */
  write_block(fd,data);
}

void cmd_set_dlock_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x04 }; /* dial lock = on */
  write_block(fd,data);

}

void cmd_set_select_vfo_a(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x05 }; /* select vfo A */
  write_block(fd,data);
}

void cmd_set_select_vfo_b(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x05 }; /* select vfo B */
  write_block(fd,data);
}

void cmd_set_memory_to_vfo(int fd, int mem) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x06 }; /* memory to vfo*/
  
  data[3] = mem;
  write_block(fd,data);
}

void cmd_set_up500k(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x07 }; /* up 500 khz */
  write_block(fd,data);
}

void cmd_set_down500k(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x08 }; /* down 500 khz */
  write_block(fd,data);
}

void cmd_set_clarify_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x09 }; /* clarify off */
  write_block(fd,data);
  printf("cmd_clarify_off complete \n");
}

void cmd_set_clarify_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x09 }; /* clarify on */
  write_block(fd,data);
}

void cmd_set_freq(int fd, unsigned int freq) {
  printf("cmd_freq_set not implemented yet \n");
}


void cmd_set_mode(int fd, int mode) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0c }; /* mode set */

  data[3] = mode;
  write_block(fd,data);

}

void cmd_set_pacing(int fd, int delay) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0e }; /* pacing set */

  data[3] = delay;
  write_block(fd,data);

}

void cmd_set_ptt_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0f }; /* ptt off */
  write_block(fd,data);

}

void cmd_set_ptt_on(int fd) {

#ifdef TX_ENABLED
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x0f }; /* ptt on */
  write_block(fd,data);
  printf("cmd_ptt_on complete \n");
#elsif
  printf("cmd_ptt_on disabled \n");
#endif

}

/*
 * Read data from rig and store in buffer provided
 * by the user.
 */

void cmd_get_update_store(int fd, unsigned char *buffer) {
  int n;			/* counter */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x10 }; /* request update from rig */

  write_block(fd,data); 
  n = read_sleep(fd,buffer,345);	/* wait and read for 345 bytes to be read */

  return;
}




