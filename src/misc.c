/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * misc.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com),
 * 				Stephane Fillod 2000
 * Provides useful routines for data handling, used by backend
 * 	as well as by the frontend.
 *
 * $Id: misc.c,v 1.8 2001-06-15 07:08:37 f4cfe Exp $  
 *
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif                                                                                                      

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/types.h>
#include <unistd.h>

#define HAMLIB_DLL
#include <hamlib/rig.h>

#include "misc.h"



static int rig_debug_level = RIG_DEBUG_TRACE;

/*
 * Do a hex dump of the unsigned char array.
 */

#define DUMP_HEX_WIDTH 16

void dump_hex(const unsigned char ptr[], size_t size)
{
  int i;
  char buf[DUMP_HEX_WIDTH+1];

  if (!rig_need_debug(RIG_DEBUG_TRACE))
		  return;

  buf[DUMP_HEX_WIDTH] = '\0';

  for(i=0; i<size; i++) {
    if (i % DUMP_HEX_WIDTH == 0)
      rig_debug(RIG_DEBUG_TRACE,"%.4x\t",i);

    rig_debug(RIG_DEBUG_TRACE," %.2x", ptr[i]);

	if (ptr[i] >= ' ' && ptr[i] < 0x7f)
		buf[i%DUMP_HEX_WIDTH] = ptr[i];
	else
		buf[i%DUMP_HEX_WIDTH] = '.';

    if (i % DUMP_HEX_WIDTH == DUMP_HEX_WIDTH-1)
      rig_debug(RIG_DEBUG_TRACE,"\t%s\n",buf);
  }
  /*
   * add some spaces in this case in order to align right ASCII dump column
   */
  if (i % DUMP_HEX_WIDTH != DUMP_HEX_WIDTH-1) {
  	buf[i % DUMP_HEX_WIDTH] = '\0';
    rig_debug(RIG_DEBUG_TRACE,"\t%s\n",buf);
  }

} 


/*
 * Convert a long long (eg. frequency in Hz) to 4-bit BCD digits, 
 * packed two digits per octet, in little-endian order.
 * bcd_len is the number of BCD digits, usually 10 or 8 in 1-Hz units, 
 *	and 6 digits in 100-Hz units for Tx offset data.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/ the 64bit freq)
 */
unsigned char *
to_bcd(unsigned char bcd_data[], unsigned long long freq, int bcd_len)
{
	int i;
	unsigned char a;

	/* '450'-> 0,5;4,0 */

	for (i=0; i < bcd_len/2; i++) {
			a = freq%10;
			freq /= 10;
			a |= (freq%10)<<4;
			freq /= 10;
			bcd_data[i] = a;
	}
	if (bcd_len&1) {
			bcd_data[i] &= 0xf0;
			bcd_data[i] |= freq%10;	/* NB: high nibble is left uncleared */
	}

	return bcd_data;
}

/*
 * Convert BCD digits to a long long (eg. frequency in Hz)
 * bcd_len is the number of BCD digits.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/ the 64bit freq)
 */
unsigned long long from_bcd(const unsigned char bcd_data[], int bcd_len)
{
	int i;
	freq_t f = 0;

	if (bcd_len&1)
			f = bcd_data[bcd_len/2] & 0x0f;

	for (i=(bcd_len/2)-1; i >= 0; i--) {
			f *= 10;
			f += bcd_data[i]>>4;
			f *= 10;
			f += bcd_data[i] & 0x0f;
	}
	
	return f;
}

/*
 * Same as to_bcd, but in Big Endian mode
 */
unsigned char *
to_bcd_be(unsigned char bcd_data[], unsigned long long freq, int bcd_len)
{
	int i;
	unsigned char a;

	/* '450'-> 0,4;5,0 */

	for (i=(bcd_len/2)-1; i >= 0; i--) {
			a = freq%10;
			freq /= 10;
			a |= (freq%10)<<4;
			freq /= 10;
			bcd_data[i] = a;
	}
	if (bcd_len&1) {
			bcd_data[0] &= 0xf0;
			bcd_data[0] |= freq%10;	/* NB: high nibble is left uncleared */
	}

	return bcd_data;
}

/*
 * Same as from_bcd, but in Big Endian mode
 */
unsigned long long from_bcd_be(const unsigned char bcd_data[], int bcd_len)
{
	int i;
	freq_t f = 0;

	if (bcd_len&1)
			f = bcd_data[0] & 0x0f;

	for (i=bcd_len&1; i < (bcd_len+1)/2; i++) {
			f *= 10;
			f += bcd_data[i]>>4;
			f *= 10;
			f += bcd_data[i] & 0x0f;
	}
	
	return f;
}

/*
 * rig_set_debug
 * Change the current debug level
 */
void rig_set_debug(enum rig_debug_level_e debug_level)
{
		rig_debug_level = debug_level;
}

/*
 * rig_need_debug
 * Usefull for dump_hex, etc.
 */
int rig_need_debug(enum rig_debug_level_e debug_level)
{
		return (debug_level <= rig_debug_level);
}

/*
 * rig_debug
 * Debugging messages are done through stderr
 * TODO: add syslog support if needed
 */
void rig_debug(enum rig_debug_level_e debug_level, const char *fmt, ...)
{
		va_list ap;

		if (debug_level <= rig_debug_level) {
				va_start(ap, fmt);
				/*
				 * Who cares about return code?
				 */
				vfprintf (stderr, fmt, ap);
				va_end(ap);
		}
}

#define llabs(a) ((a)<0?-(a):(a))

/*
 * rig_freq_snprintf
 * pretty print frequencies
 * str must be long enough. max can be as long as 17 chars
 */
int freq_sprintf(char *str, freq_t freq)
{
		double f;
		char *hz;

		if (llabs(freq) >= GHz(1)) {
				hz = "GHz";
				f = (double)freq/GHz(1);
		} else if (llabs(freq) >= MHz(1)) {
				hz = "MHz";
				f = (double)freq/MHz(1);
		} else if (llabs(freq) >= kHz(1)) {
				hz = "kHz";
				f = (double)freq/kHz(1);
		} else {
				hz = "Hz";
				f = (double)freq;
		}

		return sprintf (str, "%g%s", f, hz);
}

