/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * misc.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com), 
 * 				Stephane Fillod 2000
 * Provides useful routines for data handling, used by backends
 * 	as well as by the frontend.
 *
 *    $Id: misc.h,v 1.3 2000-10-16 21:53:22 f4cfe Exp $  
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


#ifndef _MISC_H
#define _MISC_H 1


/*
 * Carefull!! These marcos are NOT reentrant!
 * ie. they may not be executed atomically,
 * thus not ensure mutual exclusion.
 * Fix it when making Hamlib reentrant!  --SF
 */
#define Hold_Decode(rig) {(rig)->state.hold_decode = 1;}
#define Unhold_Decode(rig) {(rig)->state.hold_decode = 0;}



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

void dump_hex(const unsigned char ptr[], size_t size);

/*
 * BCD conversion routines.
 * to_bcd converts a long long int to a little endian BCD array,
 * 	and return a pointer to this array.
 * from_bcd converts a little endian BCD array to long long int 
 *  reprensentation, and return it.
 * bcd_len is the number of digits in the BCD array.
 */
unsigned char *to_bcd(unsigned char bcd_data[], unsigned long long freq, int bcd_len);
unsigned long long from_bcd(const unsigned char bcd_data[], int bcd_len);

/*
 * same as to_bcd and from_bcd, but in Big Endian mode
 */
unsigned char *to_bcd_be(unsigned char bcd_data[], unsigned long long freq, int bcd_len);
unsigned long long from_bcd_be(const unsigned char bcd_data[], int bcd_len);

#endif /* _MISC_H */

