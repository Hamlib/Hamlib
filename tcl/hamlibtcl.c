/*
 *  Hamlib tcl/tk bindings - main file
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: hamlibtcl.c,v 1.2 2001-07-13 19:08:15 f4cfe Exp $
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

/*
 * since there's no long long support in tcl API, 
 * freq_t is mapped to double object, which has enough
 * significant digits for frequencies up to tens of GHz.
 */


int DoRig(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[]);
int DoRigLib(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[]);


int
DoRigLib(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *resultPtr;
	int    error;
	char *subcmd;

	/* objc gives the number of "objects" = arguments to
	the command */
	if (objc != 2 && objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "cmd ?model?");
		return TCL_ERROR;
	}
	
	/* Process the arguments
	objv is an array holding the arguments */

  /* Use Tcl_GetIntFromObj to put an integer value
       into a variable from the argument list */

	subcmd = Tcl_GetString(objv[1]);
	resultPtr = Tcl_GetObjResult(interp);

	/* TODO: make use of flex here! */

	if (!strcasecmp(subcmd, "version")) {
		Tcl_SetStringObj(resultPtr, hamlib_version, -1);
	} else if (!strcasecmp(subcmd, "help")) {
		Tcl_SetStringObj(resultPtr, "no help yet!", -1);
	} else if (!strcasecmp(subcmd, "init")) {
		rig_model_t model;
		RIG *tcl_rig;
		char rigcmd_name[16];
		static int rigcmd_cpt=0;

		error = Tcl_GetIntFromObj(interp, objv[2], (int*)&model);
		if (error != TCL_OK)
				return error;
		tcl_rig = rig_init(model);
		if (!tcl_rig)
				return TCL_ERROR;
		sprintf(rigcmd_name, "rig%d", rigcmd_cpt++);
		Tcl_CreateObjCommand(interp, rigcmd_name, DoRig,
	       (ClientData)tcl_rig, (Tcl_CmdDeleteProc *)NULL);
		Tcl_SetStringObj(resultPtr, rigcmd_name, -1);
	} else {
		return TCL_ERROR;
	}
	return TCL_OK;
}

int
DoRig(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *resultPtr;
	int    error;
	char *subcmd;
	RIG *tcl_rig = (RIG*)clientData;

	if (!tcl_rig)
			return TCL_ERROR;

#if 0
	/* objc gives the number of "objects" = arguments to
	the command */
	if (!(objc == 2 || objc == 3)) {
		Tcl_WrongNumArgs(interp, 1, objv, "var1 var2 ?var3?");
		return TCL_ERROR;
	}
#endif
	
	/* Process the arguments
	objv is an array holding the arguments */

  /* Use Tcl_GetIntFromObj to put an integer value
       into a variable from the argument list */

	subcmd = Tcl_GetString(objv[1]);
	resultPtr = Tcl_GetObjResult(interp);

	/* TODO: make use of flex here! */

	if (!strcasecmp(subcmd, "open")) {
		rig_open(tcl_rig);
		return TCL_OK;
	} else if (!strcasecmp(subcmd, "set_freq")) {
		double d;
		freq_t freq;

		error = Tcl_GetDoubleFromObj(interp, objv[2], &d);
		if (error != TCL_OK)
			return error;
		freq = (freq_t)d;

		rig_set_freq(tcl_rig, RIG_VFO_CURR, freq);
		return TCL_OK;
	} else if (!strcasecmp(subcmd, "get_strength")) {
		int strength;

		rig_get_strength(tcl_rig, RIG_VFO_CURR, &strength);
		Tcl_SetIntObj(resultPtr, strength);
		return TCL_OK;
	} else if (!strcasecmp(subcmd, "close")) {

		rig_close(tcl_rig);
		return TCL_OK;
	} else if (!strcasecmp(subcmd, "cleanup")) {

		rig_cleanup(tcl_rig);
		return TCL_OK;
	} else {
		return TCL_ERROR;
	}
	return TCL_OK;
}



int Hamlib_Init(Tcl_Interp *interp)
{
	int ret;

	if ((ret=Tcl_PkgProvide(interp, "Hamlib",  "1.0")) != TCL_OK)
			return ret;

  /* Register the commands in the package */

	Tcl_CreateObjCommand(interp, "rig", DoRigLib,
	       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

