/*
 *  Hamlib Interface - io function header
 *  Copyright (c) 2000,2001,2002 by Stephane Fillod and Frank Singleton
 *
 *		$Id: iofunc.h,v 1.2 2002-03-10 23:41:39 fillods Exp $
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

#include <hamlib/rig.h>


extern HAMLIB_EXPORT(int) read_block(port_t *p, char *rxbuffer, size_t count);
extern HAMLIB_EXPORT(int) write_block(port_t *p, const char *txbuffer, size_t count);
extern HAMLIB_EXPORT(int) fread_block(port_t *p, char *rxbuffer, size_t count);
extern HAMLIB_EXPORT(int) read_string(port_t *p, char *rxbuffer, size_t rxmax, const char *stopset, int stopset_len);

#endif /* _IOFUNC_H */

