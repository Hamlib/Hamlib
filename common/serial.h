/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * Provides useful routines for read/write serial data for communicating
 * via serial interface .
 *
 *    $Id: serial.h,v 1.2 2000-07-28 00:48:40 javabear Exp $  
 */


int open_port(char *serial_port);
int write_block(int fd, unsigned char *data);
int close_port(int fd);


/*
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 * 
 * Sleeps every second until number of bytes to read
 * is the amount requested.
 *
 */


int read_sleep(int fd, unsigned char *rxbuffer, int num );
