/*
 *  Hamlib Interface - toolbox header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: misc.h,v 1.11 2002-01-22 00:50:32 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _MISC_H
#define _MISC_H 1

#include <hamlib/rig.h>


/*
 * Carefull!! These marcos are NOT reentrant!
 * ie. they may not be executed atomically,
 * thus not ensure mutual exclusion.
 * Fix it when making Hamlib reentrant!  --SF
 */
#define Hold_Decode(rig) {(rig)->state.hold_decode = 1;}
#define Unhold_Decode(rig) {(rig)->state.hold_decode = 0;}


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
extern HAMLIB_EXPORT(unsigned char *) to_bcd(unsigned char bcd_data[], unsigned long long freq, int bcd_len);
extern HAMLIB_EXPORT(unsigned long long) from_bcd(const unsigned char bcd_data[], int bcd_len);

/*
 * same as to_bcd and from_bcd, but in Big Endian mode
 */
extern HAMLIB_EXPORT(unsigned char *) to_bcd_be(unsigned char bcd_data[], unsigned long long freq, int bcd_len);
extern HAMLIB_EXPORT(unsigned long long) from_bcd_be(const unsigned char bcd_data[], int bcd_len);

extern HAMLIB_EXPORT(int) freq_sprintf(char *str, freq_t freq);
extern HAMLIB_EXPORT(const char *) strmode(rmode_t mode);
extern HAMLIB_EXPORT(const char *) strvfo(vfo_t vfo);
extern HAMLIB_EXPORT(const char *) strfunc(setting_t);
extern HAMLIB_EXPORT(const char *) strlevel(setting_t);
extern HAMLIB_EXPORT(const char *) strparm(setting_t);
extern HAMLIB_EXPORT(const char *) strptrshift(rptr_shift_t);
extern HAMLIB_EXPORT(const char *) strvfop(vfo_op_t op);
extern HAMLIB_EXPORT(const char *) strscan(scan_t scan);

extern HAMLIB_EXPORT(rmode_t) parse_mode(const char *s);
extern HAMLIB_EXPORT(vfo_t) parse_vfo(const char *s);
extern HAMLIB_EXPORT(setting_t) parse_func(const char *s);
extern HAMLIB_EXPORT(setting_t) parse_level(const char *s);
extern HAMLIB_EXPORT(setting_t) parse_parm(const char *s);
extern HAMLIB_EXPORT(vfo_op_t) parse_vfo_op(const char *s);
extern HAMLIB_EXPORT(scan_t) parse_scan(const char *s);
extern HAMLIB_EXPORT(rptr_shift_t) parse_rptr_shift(const char *s);

/* check if it's any of CR or LF */
#define isreturn(c) ((c) == 10 || (c) == 13)

#endif /* _MISC_H */

