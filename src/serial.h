/*
 *  Hamlib Interface - serial communication header
 *  Copyright (c) 2000-2002 by Stephane Fillod and Frank Singleton
 *
 *		$Id: serial.h,v 1.20 2003-08-17 22:39:07 fillods Exp $
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
#include "iofunc.h"


extern HAMLIB_EXPORT(int) serial_open(port_t *rs);
extern HAMLIB_EXPORT(int) serial_setup(port_t *rs);
extern HAMLIB_EXPORT(int) serial_flush(port_t *p);

/* Hamlib internal use, see rig.c */
int ser_open(port_t *p);
int ser_close(port_t *p);
int ser_ptt_set(port_t *p, ptt_t pttx);
int ser_ptt_get(port_t *p, ptt_t *pttx);
int ser_dcd_get(port_t *p, dcd_t *dcdx);
extern HAMLIB_EXPORT(int) ser_set_rts(port_t *p, int state);
extern HAMLIB_EXPORT(int) ser_get_rts(port_t *p, int *state);
int ser_set_dtr(port_t *p, int state);
int ser_get_dtr(port_t *p, int *state);
extern HAMLIB_EXPORT(int) ser_get_dcd(port_t *p, int *state);
int par_open(port_t *p);
int par_close(port_t *p);
int par_ptt_set(port_t *p, ptt_t pttx);
int par_ptt_get(port_t *p, ptt_t *pttx);
int par_dcd_get(port_t *p, dcd_t *dcdx);

extern HAMLIB_EXPORT(int) par_write_data(port_t *p, unsigned char data);
extern HAMLIB_EXPORT(int) par_write_control(port_t *p, unsigned char control);
extern HAMLIB_EXPORT(int) par_read_data(port_t *p, unsigned char *data);
extern HAMLIB_EXPORT(int) par_read_control(port_t *p, unsigned char *control);

#endif /* _SERIAL_H */

