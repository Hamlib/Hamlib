/*
 *  Hamlib Dummy backend - main header
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: dummy.h,v 1.5 2001-12-28 20:28:03 fillods Exp $
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

#ifndef _DUMMY_H
#define _DUMMY_H 1


extern const struct rig_caps dummy_caps;

extern BACKEND_EXPORT(int) initrigs_dummy(void *be_handle);

#endif /* _DUMMY_H */
