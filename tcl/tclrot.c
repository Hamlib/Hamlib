/*
 *  Hamlib tcl/tk bindings - rotator
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: tclrot.c,v 1.1 2002-01-22 00:34:48 fillods Exp $
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
#include <hamlib/rotator.h>
#include <misc.h>

#include "tclrot.h"


#define ARG_IN1  0x01
#define ARG_OUT1 0x02
#define ARG_IN2  0x04
#define ARG_OUT2 0x08
#define ARG_IN3  0x10
#define ARG_OUT3 0x20
#define ARG_IN4  0x40
#define ARG_OUT4 0x80

#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT  (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

struct cmd_table {
	const char *name;
    int (*rot_routine)(ROT*, const struct cmd_table*, 
					Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
	int flags;
	const char *arg1;
	const char *arg2;
	const char *arg3;
};

#define declare_proto_rot(f) static int (f)(ROT *rot, \
			const struct cmd_table *cmd, \
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])

declare_proto_rot(open);
declare_proto_rot(close);
declare_proto_rot(set_conf);
declare_proto_rot(get_conf);
declare_proto_rot(set_pos);
declare_proto_rot(get_pos);
declare_proto_rot(get_info);

/*
 */
static struct cmd_table cmd_list[] = {
		{ "open", open },
		{ "close", close },
		{ "set_conf", set_conf, ARG_IN, "Token", "Value" },
		{ "get_conf", get_conf, ARG_IN1|ARG_OUT2, "Token", "Value" },
		{ "set_pos", set_pos, ARG_IN, "Azimuth", "Elevation" },
		{ "get_pos", get_pos, ARG_OUT, "Azimuth", "Elevation" },
		{ "get_info", get_info, ARG_OUT, "Info" },
		{ NULL, NULL },

};

#define MAXNAMSIZ 32
#define MAXNBOPT 100	/* max number of different options */


/*
 * TODO: use Lex and some kind of hash table
 */
static struct cmd_table *parse_cmd(const char *name)
{
		int i;
		for (i=0; i<MAXNBOPT && cmd_list[i].name != NULL; i++)
				if (!strncmp(name, cmd_list[i].name, MAXNAMSIZ))
						return &cmd_list[i];
		return NULL;
}


/*
 * since there's no long long support in tcl API, 
 * freq_t is mapped to double object, which has enough
 * significant digits for frequencies up to tens of GHz.
 */

int
DoRotLib(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *resultPtr;
	int    retcode;
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
		rot_model_t model;
		ROT *tcl_rot;
		char rotcmd_name[16];
		static int rotcmd_cpt=0;

		retcode = Tcl_GetIntFromObj(interp, objv[2], (int*)&model);
		if (retcode != TCL_OK)
				return retcode;
		tcl_rot = rot_init(model);
		if (!tcl_rot)
				return TCL_ERROR;
		sprintf(rotcmd_name, "rot%d", rotcmd_cpt++);
		Tcl_CreateObjCommand(interp, rotcmd_name, DoRot,
	       (ClientData)tcl_rot, (Tcl_CmdDeleteProc *)NULL);
		Tcl_SetStringObj(resultPtr, rotcmd_name, -1);
	} else {
		return TCL_ERROR;
	}
	return TCL_OK;
}

int
DoRot(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[])
{
	int    retcode;
	char *subcmd;
	ROT *tcl_rot = (ROT*)clientData;
	struct cmd_table *cmd_entry;

	if (!tcl_rot)
			return TCL_ERROR;

	/* objc gives the number of "objects" = arguments to
	the command */
	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "var1 var2 ?var3?");
		return TCL_ERROR;
	}
	
	/* Process the arguments
	objv is an array holding the arguments */

  /* Use Tcl_GetIntFromObj to put an integer value
       into a variable from the argument list */

	subcmd = Tcl_GetString(objv[1]);
	cmd_entry = parse_cmd(subcmd);
	if (!cmd_entry) {
			/* unknow subcmd */
		return TCL_ERROR;
	}

	retcode = (*cmd_entry->rot_routine)(tcl_rot, 
					cmd_entry, interp, objc, objv);
				
	if (retcode != RIG_OK ) {
			/* FIXME: set Tcl error, and retcode can be TCL_ERROR! */
		fprintf(stderr, "%s: %s\n", cmd_entry->name, rigerror(retcode));
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * static int (f)(ROT *rot, const struct cmd_table *cmd, 
 *			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
 */

declare_proto_rot(open)
{
		return rot_open(rot);
}

declare_proto_rot(close)
{
		return rot_close(rot);
}

declare_proto_rot(set_conf)
{
		token_t tok;

		tok = rot_token_lookup(rot, Tcl_GetString(objv[2]));

		return rot_set_conf(rot, tok, Tcl_GetString(objv[3]));
}

declare_proto_rot(get_conf)
{
		int status;
		token_t tok;
		Tcl_Obj *resultPtr;
		char buf[255];

		resultPtr = Tcl_GetObjResult(interp);

		tok = rot_token_lookup(rot, Tcl_GetString(objv[2]));

		status = rot_get_conf(rot, tok, buf);
		if (status != RIG_OK)
				return status;

		Tcl_SetStringObj(resultPtr, buf, -1);

		return RIG_OK;
}


declare_proto_rot(set_pos)
{
		int status;
		double da, de;

		status = Tcl_GetDoubleFromObj(interp, objv[2], &da);
		if (status != TCL_OK)
			return status;
		status = Tcl_GetDoubleFromObj(interp, objv[3], &de);
		if (status != TCL_OK)
			return status;

		return rot_set_position(rot, (azimuth_t)da, (elevation_t)de);
}

declare_proto_rot(get_pos)
{
		int status;
		azimuth_t az;
		elevation_t el;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rot_get_position(rot, &az, &el);
		if (status != RIG_OK)
				return status;
		/* FIXME: return also elevation */
		Tcl_SetDoubleObj(resultPtr, (double)az);

		return RIG_OK;
}

declare_proto_rot(get_info)
{
		const char *s;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		s = rot_get_info(rot);
		Tcl_SetStringObj(resultPtr, s ? s : "None", -1);

		return RIG_OK;
}

