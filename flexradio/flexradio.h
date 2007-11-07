/*
 *  Hamlib FLEXRADIO backend - main header
 *  Copyright (c) 2004-2007 by Stephane Fillod
 *
 *	$Id: flexradio.h,v 1.1 2007-11-07 19:06:44 fillods Exp $
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

#ifndef _FLEXRADIO_H
#define _FLEXRADIO_H 1

#include "hamlib/rig.h"

extern const struct rig_caps sdr1k_rig_caps;
extern const struct rig_caps sdr1krfe_rig_caps;
extern const struct rig_caps dttsp_rig_caps;

#endif /* _FLEXRADIO_H */
