/*
 *  Hamlib tcl/tk bindings - rig header
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: tclrig.h,v 1.1 2002-01-22 00:34:48 fillods Exp $
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

#ifndef _TCLRIG_H
#define _TCLRIG_H

#include <tcl.h>

extern int DoRig(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[]);
extern int DoRigLib(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[]);

#endif /* _TCLRIG_H */
