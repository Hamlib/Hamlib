/*
 *  Hamlib Interface - CM108 GPIO communication header
 *  Copyright (c) 2000-2003 by Frank Singleton
 *  Copyright (c) 2000-2010 by Stephane Fillod
 *  Copyright (c) 2011 by Andrew Errington
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

#ifndef _CM108_H
#define _CM108_H 1

#include <hamlib/rig.h>
#include "iofunc.h"


__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int cm108_open(hamlib_port_t *p);
int cm108_close(hamlib_port_t *p);
int cm108_ptt_set(hamlib_port_t *p, ptt_t pttx);
int cm108_ptt_get(hamlib_port_t *p, ptt_t *pttx);
int cm108_dcd_get(hamlib_port_t *p, dcd_t *dcdx);

extern HAMLIB_EXPORT(int) cm108_write_data(hamlib_port_t *p,
                                           unsigned char data);

extern HAMLIB_EXPORT(int) cm108_write_control(hamlib_port_t *p,
                                              unsigned char control);

extern HAMLIB_EXPORT(int) cm108_read_data(hamlib_port_t *p,
                                          unsigned char *data);

extern HAMLIB_EXPORT(int) cm108_read_control(hamlib_port_t *p,
                                             unsigned char *control);

extern HAMLIB_EXPORT(int) cm108_read_status(hamlib_port_t *p,
                                            unsigned char *status);

extern HAMLIB_EXPORT(int) cm108_lock(hamlib_port_t *p);

extern HAMLIB_EXPORT(int) cm108_unlock(hamlib_port_t *p);

__END_DECLS

#endif /* _CM108_H */
