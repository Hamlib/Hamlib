/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2010
 *
 * ft747.h - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar (max232 + some capacitors :-)
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef _FT747_H
#define _FT747_H 1

/*
 * According to manual, the UPDATE data length should be 345
 * but some rigs are short by one byte.
 */
#define FT747_STATUS_UPDATE_DATA_LENGTH      344

#define FT747_PACING_DEFAULT_VALUE           0
#define FT747_WRITE_DELAY                    5 /* manual say 50 ms, but it doesn't work though */


/* Sequential fast writes confuse my FT747 without this delay */

#define FT747_POST_WRITE_DELAY               5


/*
 * 8N2 and 1 start bit = 11 bits at 4800 bps => effective byte rate = 1 byte in 2.2917 msec
 * => 345 bytes in 790 msec
 *
 * delay for 1 byte = 2.2917 + (pace_interval * 5)
 *
 * pace_interval          time to read 345 bytes
 * ------------           ----------------------
 *
 *     0                       790 msec
 *     1                       2515 msec
 *     2                       4240 msec
 *    255                      441 sec => 7 min 21 seconds
 *
 */



/*
 * The time the status block is cached (in millisec).
 * This optimises the common case of doing eg. rig_get_freq() and
 * rig_get_mode() in a row.
 *
 * The timeout is set to at least the time to transfer the block (790ms)
 * plus post write delay, plus some margin as get_freq was taking > 1000ms
 */
#define FT747_CACHE_TIMEOUT    1500 


#endif /* _FT747_H */
