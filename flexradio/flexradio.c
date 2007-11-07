/*
 *  Hamlib Flexradio backend
 *  Copyright (c) 2003-2007 by Stephane Fillod
 *
 *	$Id: flexradio.c,v 1.1 2007-11-07 19:06:44 fillods Exp $
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
#include <math.h>

#include "hamlib/rig.h"
#include "flexradio.h"
#include "register.h"

DECLARE_INITRIG_BACKEND(flexradio)
{
	rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

	rig_register(&sdr1k_rig_caps);
	//rig_register(&sdr1krfe_rig_caps);
	rig_register(&dttsp_rig_caps);

	return RIG_OK;
}

