/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * Provides useful routines for read/write serial data for communicating
 * via serial interface .
 *
 *    $Id: serial.h,v 1.1 2000-07-25 01:01:00 javabear Exp $  
 */


int open_port(char *serial_port);
int write_block(int fd, unsigned char *data);
int close_port(int fd);

