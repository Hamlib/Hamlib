/*
 *  Hamlib Interface - event handling header
 *  Copyright (c) 2000-2003 by Stephane Fillod and Frank Singleton
 *
 *	$Id: event.h,v 1.3 2003-08-17 22:39:07 fillods Exp $
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

#ifndef _EVENT_H
#define _EVENT_H 1

#include <hamlib/rig.h>


int add_trn_rig(RIG *rig);
int remove_trn_rig(RIG *rig);

#endif /* _EVENT_H */

