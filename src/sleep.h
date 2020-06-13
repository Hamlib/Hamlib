/*
 *  Hamlib Interface - sleep header
 *  Copyright (c) 2020 by Michael Black W9MDB
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

#ifndef _HL_SLEEP_H
#define _HL_SLEEP_H 1

#include <hamlib/rig.h>
#include "iofunc.h"

__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int hl_usleep(rig_useconds_t usec);

__END_DECLS

#endif /* _HL_SLEEP_H */
