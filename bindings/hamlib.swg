
%module Hamlib

%{
/*
 *  Hamlib bindings - swig interface file
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#ifdef SWIGPYTHON
%include python/file.i

%typemap(out) int [ANY] {
  int len,i;
  len = $1_dim0;
  $result = PyList_New(len);
  for (i = 0; i < len; i++) {
    PyList_SetItem($result,i,PyInt_FromLong((long)$1[i]));
  }
}

%typemap(varout) int [ANY] {
  int len,i;
  len = $1_dim0;
  $result = PyList_New(len);
  for (i = 0; i < len; i++) {
    PyList_SetItem($result,i,PyInt_FromLong((long)$1[i]));
  }
}

#endif

#ifdef SWIGPERL

%typemap(out) int [ANY] {
    AV * av = newAV();
    int i = 0,len = 0;
    len = $1_dim0;

    for (i = 0; i < len ; i++) {
        SV* perlval = newSV(0);
        sv_setiv(perlval, (IV)$1[i]);
        av_push(av, perlval);
    }
    $result = newRV_noinc((SV *)av);
    sv_2mortal( $result );
    argvi++;
}

%typemap(argout) int [ANY] {
    AV * av = newAV();
    int i = 0,len = 0;
    len = $1_dim0;

    for (i = 0; i < len ; i++) {
        SV* perlval = newSV(0);
        sv_setiv(perlval, (IV)$1[i]);
        av_push(av, perlval);
    }
    $result = newRV_noinc((SV *)av);
    sv_2mortal( $result );
    argvi++;
}

#endif

#ifdef SWIGTCL

%typemap(out) int [ANY] {
    int i, len, l;
    len = $1_dim0;

    Tcl_Obj * list = Tcl_NewListObj(len, NULL);
    for(i=0; i < len; i++) {
        Tcl_ListObjAppendElement(interp, list, Tcl_NewIntObj($1[i]));
    }
    Tcl_SetObjResult(interp, list);
}

%typemap(varout) int [ANY] {
    int i, len, l;
    len = $1_dim0;

    Tcl_Obj * list = Tcl_NewListObj(len, NULL);
    for(i=0; i < len; i++) {
        Tcl_ListObjAppendElement(interp, list, Tcl_NewIntObj($1[i]));
    }

    $result = list;
}


#endif

%apply double *OUTPUT { double *distance, double *azimuth };
%apply int *OUTPUT { int *degrees, int *minutes, double *seconds, int *sw };
%apply int *OUTPUT { int *degrees, double *minutes, int *sw };
%apply double *OUTPUT { double *longitude, double *latitude };
%apply char *OUTPUT { char *locator_res };

#ifndef SWIG_CSTRING_UNIMPL
/* longlat2locator */
%cstring_bounded_output(char *locator_res, 13)
#endif

%immutable confparams::name;
%immutable confparams::label;
%immutable confparams::tooltip;
%immutable confparams::dflt;
%immutable cs;
%immutable rig_caps::model_name;
%immutable rig_caps::mfg_name;
%immutable rig_caps::version;
%immutable rig_caps::copyright;

%immutable clone_combo_set;
%immutable clone_combo_get;

%immutable rot_caps::model_name;
%immutable rot_caps::mfg_name;
%immutable rot_caps::version;
%immutable rot_caps::copyright;

%include <hamlib/rig_dll.h>

%include <hamlib/riglist.h>
%include <hamlib/rig.h>

%include <hamlib/rotlist.h>
%include <hamlib/rotator.h>

%inline {
typedef const char * const_char_string;
}

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
