/*
 *  Hamlib Tuner backend - main file
 *  Copyright (C) 2004 Stephane Fillodpab@users.sourceforge.net
 *
 *	$Id: tuner.c,v 1.1 2004-09-12 21:30:21 fillods Exp $
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

#include "tuner.h"	/* config.h */

#include <stdlib.h>

#include "hamlib/rig.h"
#include "register.h"


DECLARE_INITRIG_BACKEND(tuner)
{
	rig_debug(RIG_DEBUG_VERBOSE, "tuner: _init called\n");

#ifdef V4L_IOCTL
	rig_register(&v4l_caps);
#endif


	return RIG_OK;
}

