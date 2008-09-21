/*
 *  Hamlib Interface - network communication header
 *  Copyright (c) 2000-2008 by Stephane Fillod
 *
 *	$Id: network.h,v 1.1 2008-09-21 19:30:35 fillods Exp $
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

#ifndef _NETWORK_H
#define _NETWORK_H 1

#include <hamlib/rig.h>
#include "iofunc.h"

__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int network_open(hamlib_port_t *p);

__END_DECLS

#endif /* _NETWORK_H */
