
%module hamlib

%{
/*
 *  Hamlib bindings - swig interface file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: hamlib.swg,v 1.3 2002-10-07 21:44:51 fillods Exp $
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

#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include "misc.h"

#include <limits.h>
%}

/*
 * symbols that won't be wrapped
 */
%include "ignore.swg"

%include <hamlib/rig_dll.h>

%include <hamlib/riglist.h>
%include <hamlib/rig.h>

%include <hamlib/rotlist.h>
%include <hamlib/rotator.h>

/* needed because rig.swg and rotator.swg macros require identifiers like arg (no spaces) */
%header %{
	typedef char * char_string;
	typedef const char * const_char_string;
	typedef channel_t * channel_t_p;
	typedef channel_t * const_channel_t_p;
%}

%include typemaps.i
%include exception.i

/*
 * The Rig "class"
 */
%include "rig.swg"

/*
 * The Rot "class"
 */
%include "rotator.swg"

/*
 * Put binding specific code in separate files
 *
  %include "perl.i"
  %include "tcl.i"
  %include "python.i"
  %include "whatever.i"
 */


