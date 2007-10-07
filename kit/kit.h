/*
 *  Hamlib KIT backend - main header
 *  Copyright (c) 2004-2005 by Stephane Fillod
 *
 *	$Id: kit.h,v 1.4 2007-10-07 20:31:24 fillods Exp $
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

#ifndef _KIT_H
#define _KIT_H 1

#include <hamlib/rig.h>

extern const struct rig_caps elektor304_caps;
extern const struct rig_caps elektor507_caps;
extern const struct rig_caps drt1_caps;
extern const struct rig_caps dwt_caps;
extern const struct rig_caps usrp0_caps;
extern const struct rig_caps usrp_caps;

#endif /* _KIT_H */
