/*
 *  Hamlib Interface - parallel communication header
 *  Copyright (c) 2000-2004 by Stephane Fillod and Frank Singleton
 *
 *	$Id: parallel.h,v 1.1 2004-10-02 20:37:24 fillods Exp $
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

#ifndef _PARALLEL_H
#define _PARALLEL_H 1

#include <hamlib/rig.h>
#include "iofunc.h"

__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int par_open(port_t *p);
int par_close(port_t *p);
int par_ptt_set(port_t *p, ptt_t pttx);
int par_ptt_get(port_t *p, ptt_t *pttx);
int par_dcd_get(port_t *p, dcd_t *dcdx);

extern HAMLIB_EXPORT(int) par_write_data(port_t *p, unsigned char data);
extern HAMLIB_EXPORT(int) par_write_control(port_t *p, unsigned char control);
extern HAMLIB_EXPORT(int) par_read_data(port_t *p, unsigned char *data);
extern HAMLIB_EXPORT(int) par_read_control(port_t *p, unsigned char *control);
extern HAMLIB_EXPORT(int) par_read_status(port_t *p, unsigned char *status);
extern HAMLIB_EXPORT(int) par_lock(port_t *p);
extern HAMLIB_EXPORT(int) par_unlock(port_t *p);

__END_DECLS

#endif /* _PARALLEL_H */
