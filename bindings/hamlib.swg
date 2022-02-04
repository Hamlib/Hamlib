
#ifndef SWIGLUA
%module Hamlib
#else
%module Hamliblua
#endif

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

#include <hamlib/config.h>

#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include <hamlib/amplifier.h>
//mdblack98 -- Commented this out -- is anybody using the functions in misc.h?
//If so, then those functions should be moved to rig.h
//#include "misc.h"

#include <limits.h>

%}

/*
 * symbols that won't be wrapped
 */
%include "ignore.swg"

%include typemaps.i
%include stdint.i
%include exception.i
#ifndef SWIGLUA
#ifndef SWIGJAVA
%include cstring.i
#endif
#endif
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

#ifdef SWIGLUA

%typemap(out) int [ANY] {
  int len,i;
  len = $1_dim0;
  lua_createtable (L, len, 0);
  for(i=0; i<len; i++) {
    lua_pushinteger(L, (i+1)); // Lua starts list index with 1
    lua_pushinteger(L, (lua_Integer)$1[i]);
    lua_rawset(L, -3);
  }
  SWIG_arg++;
}

%typemap(varout) int [ANY] {
  int len,i;
  len = $1_dim0;
  lua_createtable (L, len, 0);
  for(i=0; i<len; i++) {
    lua_pushinteger(L, (i+1)); // Lua starts list index with 1
    lua_pushinteger(L, (lua_Integer)$1[i]);
    lua_rawset(L, -3);
  }
  SWIG_arg++;
}

%typemap(in,numinputs=0) char * OUTPUT (char *returnstr) {
    char returnstr[1024];
    $1 = returnstr;
}

%typemap(argout) char * OUTPUT {
    lua_pushstring(L,(char *)$1); SWIG_arg++;
}


#endif

%apply double *OUTPUT { double *distance, double *azimuth };
%apply int *OUTPUT { int *degrees, int *minutes, double *seconds, int *sw };
%apply int *OUTPUT { int *degrees, double *minutes, int *sw };
%apply double *OUTPUT { double *longitude, double *latitude };

#ifndef SWIGJAVA
// If SWIGJAVA ever maps char * we can enable these
%apply char *OUTPUT { char *locator_res };
%apply char *OUTPUT { char *hamlib_version2 };
%apply char *OUTPUT { char *hamlib_copyright2 };
%apply char *OUTPUT { char *returnstr };
#endif

#ifndef SWIGLUA
#ifndef SWIG_CSTRING_UNIMPL
#ifndef SWIGJAVA
/* longlat2locator */
%cstring_bounded_output(char *locator_res, 13)
%cstring_bounded_output(char *hamlib_version2, 4)
%cstring_bounded_output(char *hamlib_copyright2, 4)
#endif
#endif
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
%immutable hamlib_version2;
%immutable hamlib_copyright2;

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
 * The Amp "class"
 */
%include "amplifier.swg"

/*
 * Put binding specific code in separate files
 *
  %include "perl.i"
  %include "tcl.i"
  %include "python.i"
  %include "whatever.i"
 */
