/*
 *  Hamlib Interface - serial communication header
 *  Copyright (c) 2000-2005 by Stephane Fillod and Frank Singleton
 *
 *	$Id: serial.h,v 1.27 2005-04-03 12:27:17 fillods Exp $
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

__BEGIN_DECLS

extern HAMLIB_EXPORT(int) serial_open(hamlib_port_t *rs);
extern HAMLIB_EXPORT(int) serial_setup(hamlib_port_t *rs);
extern HAMLIB_EXPORT(int) serial_flush(hamlib_port_t *p);

/* Hamlib internal use, see rig.c */
int ser_open(hamlib_port_t *p);
int ser_close(hamlib_port_t *p);
extern HAMLIB_EXPORT(int) ser_set_rts(hamlib_port_t *p, int state);
extern HAMLIB_EXPORT(int) ser_get_rts(hamlib_port_t *p, int *state);
extern HAMLIB_EXPORT(int) ser_set_brk(hamlib_port_t *p, int state);
extern HAMLIB_EXPORT(int) ser_set_dtr(hamlib_port_t *p, int state);
extern HAMLIB_EXPORT(int) ser_get_dtr(hamlib_port_t *p, int *state);
extern HAMLIB_EXPORT(int) ser_get_cts(hamlib_port_t *p, int *state);
extern HAMLIB_EXPORT(int) ser_get_dsr(hamlib_port_t *p, int *state);
extern HAMLIB_EXPORT(int) ser_get_car(hamlib_port_t *p, int *state);

__END_DECLS

#endif /* _SERIAL_H */
