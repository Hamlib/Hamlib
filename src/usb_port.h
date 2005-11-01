/*
 *  Hamlib Interface - USB communication header
 *  Copyright (c) 2005 by Stephane Fillod
 *
 *	$Id: usb_port.h,v 1.1 2005-11-01 23:02:02 fillods Exp $
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

#ifndef _USB_PORT_H
#define _USB_PORT_H 1

#include <hamlib/rig.h>
#include "iofunc.h"

__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int usb_port_open(hamlib_port_t *p);
int usb_port_close(hamlib_port_t *p);

__END_DECLS

#endif /* _USB_PORT_H */
