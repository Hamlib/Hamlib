/*
 *  Hamlib tcl/tk bindings - main file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: hamlibtcl.c,v 1.3 2002-01-22 00:53:01 fillods Exp $
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

#include <string.h>
#include <tcl.h>
#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include <misc.h>

#include "tclrig.h"
#include "tclrot.h"


int Hamlib_Init(Tcl_Interp *interp)
{
	int ret;

	if ((ret=Tcl_PkgProvide(interp, "Hamlib",  "1.0")) != TCL_OK)
			return ret;

  /* Register the commands in the package */

	Tcl_CreateObjCommand(interp, "rig", DoRigLib,
	       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateObjCommand(interp, "rot", DoRotLib,
	       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

