/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * Provides useful routines for read/write serial data for communicating
 * via serial interface .
 *
 * $Id: serial.c,v 1.6 2000-07-30 05:08:24 javabear Exp $  
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

static void dump_hex(unsigned char *ptr, int size, int width);

/*
 * Open serial port 
 *
 * Set to 4800 8N2
 *
 * input:
 *
 * serial_port - ptr to a char (string) indicating port
 *               to open (eg: "/dev/ttyS1").
 * 
 * returns:
 *
 * fd - the file descriptor on success or -1 on error.
 *
 */

int open_port(char *serial_port) {

  int fd; /* File descriptor for the port */
  struct termios options;
  
  
  fd = open(serial_port, O_RDWR | O_NOCTTY | O_NDELAY);

  if (fd == -1) {
    
    /* Could not open the port. */
    
    printf("open_port: Unable to open %s - ",serial_port);
    return -1;			/* bad */
  }
 
  fcntl(fd, F_SETFL, 0);	/*  */
 
  
  /*
   * Get the current options for the port...
   */
  
  tcgetattr(fd, &options);
  
  /*
   * Set the baud rates to 4800...
   */
  
  cfsetispeed(&options, B4800);
  cfsetospeed(&options, B4800);
  
  /*
   * Enable the receiver and set local mode...
   */
  
  options.c_cflag |= (CLOCAL | CREAD);
  
  /*
   * Set 8N2
   */

  options.c_cflag &= ~PARENB;
  options.c_cflag |= CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;

  /*
   * Chose raw input, no preprocessing please ..
   */

  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

  /*
   * Chose raw output, no preprocessing please ..
   */

  options.c_oflag &= ~OPOST;

  /*
   * Set the new options for the port...
   */
  
  tcsetattr(fd, TCSANOW, &options);
  
  return (fd);
}

/*
 * 'close_port()' - Close serial port 
 *
 * fd - file descriptor for open port
 *
 *
 * Returns success (0) or -1 on error.
 */

int close_port(int fd) {

  if (close(fd) <0 ) {
    printf("close_port: Unable to close port using fd %i - ",fd);
    return -1;			/* oops */
  }
  return 0;			/* ok */
}


/*
 * Provides delay  for block formation.
 * Should be 50-200 msec according to YAESU docs.
 */

void pause2() {
  usleep(50 * 1000);		/* 50 msec */
  return;
}

/*
 * Write a 5 character block to a file descriptor,
 * with a pause between each character.
 *
 * This is for Yaesu type rigs..require 5 character
 * sequence to bsent with 50-200msec between each char.
 *
 * input:
 *
 * fd - file descriptor to write to
 * data - pointer to a command sequence array
 *
 * returns:
 *
 *  0 = OK
 * -1 = NOT OK
 *
 */

int write_block(int fd, unsigned char *txbuffer) {
  int i;

  for (i=0; i<5; i++) {

    if(write(fd, &txbuffer[i] , 1) < 0) {
      fputs("write() of  byte failed!\n", stderr);
      return -1;
    }
    pause2();			/* 50 msec */
  }
  dump_hex(txbuffer,5,16);
  
  return 0;
}


/*
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 * 
 * Sleeps every second until number of bytes to read
 * is the amount requested.
 *
 * It the reads "num" bytes into rxbuffer.
 *
 */


int read_sleep(int fd, unsigned char *rxbuffer, int num ) {  
  int bytes = 0;
  int n = 0;

  printf("serial:read_sleep called with num = %i \n",num);

  while(1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    if (bytes == num)
      break;
    sleep(1);			/* wait 1 second and try again */
  }

  /* this should not block now */
  
  n = read(fd,rxbuffer,num);	/* grab num bytes from rig */

  dump_hex(rxbuffer,num,16);
  return n;			/* return bytes read */
}



/*
 * Do a hex dump of the unsigned char array.
 */

static void dump_hex(unsigned char *ptr, int size, int width) {
  int i =0;

  printf("\n");
  printf("serial: Memory Dump: size = %i, width = %i .\n",size,width);

  for(i=0; i<size; i++) {
    if (i % width == 0) {
      printf("\n");
      printf("0x%.4x  ",i);
    }
    printf(" 0x%.2x", *(ptr+i));

  }
  printf("\n");

  return;
} 


/*
 * Convert char to packed decimal
 * eg: 33 (0x21) => 0x33
 *
 */

char calc_packed_from_char(unsigned char dec ) {
 
  char d1,d2,pkd;

  d1 = dec/10;
  d2 = dec - (d1 * 10);
  pkd = (d1*16)+d2;
 
  return pkd;
}


/*
 * Convert packed decimal to decimal
 * eg: 0x33 (51) => 33 decimal
 *
 */

char calc_char_from_packed(unsigned char pkd ) {
 
  char d1,d2,dec;

  d1 = pkd/16;
  d2 = pkd - (d1 * 16);
  dec = (d1*10)+d2;

  return dec;
}











