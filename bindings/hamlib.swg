
%module Hamlib

%{
/*
 *  Hamlib bindings - swig interface file
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *	$Id: hamlib.swg,v 1.6 2008-05-04 21:38:25 fillods Exp $
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

%include typemaps.i
%include exception.i
%include cstring.i

%apply double *OUTPUT { double *distance, double *azimuth };
%apply int *OUTPUT { int *degrees, int *minutes, double *seconds, int *sw };
%apply int *OUTPUT { int *degrees, double *minutes, int *sw };
%apply double *OUTPUT { double *longitude, double *latitude };
%apply char *OUTPUT { char *locator_res };

/* longlat2locator */
%cstring_bounded_output(char *locator_res, 13) 


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


