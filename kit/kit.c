/*
 *  Hamlib KIT backend - main file
 *  Copyright (c) 2004 by Stephane Fillod
 *
 *	$Id: kit.c,v 1.2 2004-08-19 21:02:47 fillods Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "register.h"

#include "kit.h"

/*
 * initrigs_kit is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(kit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "kit: _init called\n");

	rig_register(&elektor304_caps);
	rig_register(&drt1_caps);

	return RIG_OK;
}


