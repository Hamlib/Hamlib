/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com), 
 * 				  Stephane Fillod 2000
 * Provides useful routines for read/write serial data for communicating
 * via serial interface .
 *
 *    $Id: serial.h,v 1.10 2001-06-15 07:08:37 f4cfe Exp $  
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


extern HAMLIB_EXPORT(int) serial_open(port_t *rs);

extern HAMLIB_EXPORT(int) read_block(port_t *p, char *rxbuffer, size_t count);
extern HAMLIB_EXPORT(int) write_block(port_t *p, const char *txbuffer, size_t count);
extern HAMLIB_EXPORT(int) fread_block(port_t *p, char *rxbuffer, size_t count);

/* Hamlib internal use, see rig.c */
int ser_open(port_t *p);
int ser_close(port_t *p);
int ser_ptt_set(port_t *p, ptt_t pttx);
int ser_ptt_get(port_t *p, ptt_t *pttx);
int ser_dcd_get(port_t *p, dcd_t *dcdx);
int par_open(port_t *p);
int par_close(port_t *p);
int par_ptt_set(port_t *p, ptt_t pttx);
int par_ptt_get(port_t *p, ptt_t *pttx);
int par_dcd_get(port_t *p, dcd_t *dcdx);

#endif /* _SERIAL_H */

