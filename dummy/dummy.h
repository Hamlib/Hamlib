/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * dummy.h - Copyright (C) 2001 Stephane Fillod.
 *
 * This shared library supports dummy backend, for debugging
 * purpose mainly.
 *
 *
 *	$Id: dummy.h,v 1.1 2001-02-14 00:59:02 f4cfe Exp $
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _DUMMY_H
#define _DUMMY_H 1

#include <hamlib/rig.h>

extern const struct rig_caps dummy_caps;

extern int init_dummy(void *be_handle);

#endif /* _DUMMY_H */
