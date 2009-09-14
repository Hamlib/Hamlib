/*
 *  Hamlib Interface - io function header
 *  Copyright (c) 2000-2005 by Stephane Fillod and Frank Singleton
 *
 *	$Id: iofunc.h,v 1.5 2005-04-03 12:27:16 fillods Exp $
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

#ifndef _IOFUNC_H
#define _IOFUNC_H 1

#include <sys/types.h>
#include <hamlib/rig.h>


extern HAMLIB_EXPORT(int) port_open(hamlib_port_t *p);
extern HAMLIB_EXPORT(int) port_close(hamlib_port_t *p, rig_port_t port_type);

extern HAMLIB_EXPORT(int) read_block(hamlib_port_t *p, char *rxbuffer, size_t count);
extern HAMLIB_EXPORT(int) write_block(hamlib_port_t *p, const char *txbuffer, size_t count);
extern HAMLIB_EXPORT(int) read_string(hamlib_port_t *p, char *rxbuffer, size_t rxmax, const char *stopset, int stopset_len);

#endif /* _IOFUNC_H */

