/*
 *  Hamlib Uniden backend - main header
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: uniden.h,v 1.1 2001-07-14 16:45:40 f4cfe Exp $
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

#ifndef _UNIDEN_H
#define _UNIDEN_H 1

#if defined(__CYGWIN__)
#  undef HAMLIB_DLL
#  include <hamlib/rig.h>
#  include <cal.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <cal.h>
#endif


int uniden_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int uniden_set_mem(RIG *rig, vfo_t vfo, int ch);

extern const struct rig_caps bc895_caps;

extern int init_uniden(void *be_handle);


#endif /* _UNIDEN_H */

