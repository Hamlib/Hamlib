/*
 *  Hamlib Interface - parallel communication header
 *  Copyright (c) 2000-2003 by Frank Singleton
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#ifndef _PARALLEL_H
#define _PARALLEL_H 1

#include <hamlib/rig.h>
#include "iofunc.h"

#ifdef HAVE_PARALLEL
#ifdef HAVE_LINUX_PARPORT_H
#  include <linux/parport.h>
#endif
#endif

#ifndef PARPORT_CONTROL_STROBE
#  define PARPORT_CONTROL_STROBE    0x1
#endif

#ifndef PARPORT_CONTROL_AUTOFD
#  define PARPORT_CONTROL_AUTOFD    0x2
#endif

#ifndef PARPORT_CONTROL_INIT
#  define PARPORT_CONTROL_INIT      0x4
#endif

#ifndef PARPORT_CONTROL_SELECT
#  define PARPORT_CONTROL_SELECT    0x8
#endif

#ifndef PARPORT_STATUS_ERROR
#  define PARPORT_STATUS_ERROR      0x8
#endif

#ifndef PARPORT_STATUS_SELECT
#  define PARPORT_STATUS_SELECT     0x10
#endif

#ifndef PARPORT_STATUS_PAPEROUT
#  define PARPORT_STATUS_PAPEROUT   0x20
#endif

#ifndef PARPORT_STATUS_ACK
#  define PARPORT_STATUS_ACK        0x40
#endif

#ifndef PARPORT_STATUS_BUSY
#  define PARPORT_STATUS_BUSY       0x80
#endif

__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int par_open(hamlib_port_t *p);
int par_close(hamlib_port_t *p);
int par_ptt_set(hamlib_port_t *p, ptt_t pttx);
int par_ptt_get(hamlib_port_t *p, ptt_t *pttx);
int par_dcd_get(hamlib_port_t *p, dcd_t *dcdx);

extern HAMLIB_EXPORT(int) par_write_data(hamlib_port_t *p, unsigned char data);
extern HAMLIB_EXPORT(int) par_write_control(hamlib_port_t *p,
                                            unsigned char control);

extern HAMLIB_EXPORT(int) par_read_data(hamlib_port_t *p, unsigned char *data);
extern HAMLIB_EXPORT(int) par_read_control(hamlib_port_t *p,
                                           unsigned char *control);

extern HAMLIB_EXPORT(int) par_read_status(hamlib_port_t *p,
                                          unsigned char *status);

extern HAMLIB_EXPORT(int) par_lock(hamlib_port_t *p);
extern HAMLIB_EXPORT(int) par_unlock(hamlib_port_t *p);

__END_DECLS

#endif /* _PARALLEL_H */
