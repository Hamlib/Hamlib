/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * Provides useful routines for read/write serial data for communicating
 * via serial interface .
 *
 *    $Id: serial.h,v 1.7 2000-09-16 01:29:35 f4cfe Exp $  
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

#include <rig.h>


int serial_open(struct rig_state *rs);

int open_port(char *serial_port);
int write_block(int fd, unsigned char *data);
int close_port(int fd);
int open_port2(struct rig_caps *rc);


/*
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 * 
 * Sleeps every second until number of bytes to read
 * is the amount requested.
 *
 */


int read_sleep(int fd, unsigned char *rxbuffer, int num );


int read_block(int fd, unsigned char *rxbuffer, size_t count, int timeout);
int write_block2(int fd, const unsigned char *txbuffer, size_t count, int write_delay);

/*
 * Convert char to packed decimal
 * eg: 33 (0x21) => 0x33
 *
 */

char calc_packed_from_char(unsigned char dec );


/*
 * Convert packed decimal to decimal
 * eg: 0x33 (51) => 33 decimal
 *
 */

char calc_char_from_packed(unsigned char pkd );

/*
 * Do a hex dump of the unsigned char array.
 */

void dump_hex(const unsigned char *ptr, int size, int width);


#endif /* _SERIAL_H */

