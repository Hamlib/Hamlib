/*
 *  Hamlib Dummy backend - main header
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: rot_dummy.h,v 1.1 2001-12-28 20:29:33 fillods Exp $
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

#ifndef _ROT_DUMMY_H
#define _ROT_DUMMY_H 1


extern const struct rot_caps dummy_rot_caps;

extern BACKEND_EXPORT(int) initrots_dummy(void *be_handle);

#endif /* _ROT_DUMMY_H */
