/*
 *  Hamlib Interface - serial communication header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: serial.h,v 1.11 2001-07-13 19:08:15 f4cfe Exp $
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

