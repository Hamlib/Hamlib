/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com), 
 * 				  Stephane Fillod 2000
 * Provides useful routines for read/write serial data for communicating
 * via serial interface .
 *
 *    $Id: serial.h,v 1.3 2000-10-09 01:17:20 javabear Exp $  
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


#ifndef _SERIAL_H
#define _SERIAL_H 1

#include <hamlib/rig.h>


int serial_open(struct rig_state *rs);

int read_sleep(int fd, unsigned char *rxbuffer, int num , int read_delay);
int read_block(int fd, unsigned char *rxbuffer, size_t count, int timeout);
int write_block(int fd, const unsigned char *txbuffer, size_t count, int write_delay, int post_write_delay);
int fread_block(FILE *stream, unsigned char *rxbuffer, size_t count, int timeout);

#endif /* _SERIAL_H */

