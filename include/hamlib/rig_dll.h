/* $Id: rig_dll.h,v 1.4 2001-12-16 11:14:46 fillods Exp $ */


#if defined(__CYGWIN__)
#  if defined(HAMLIB_DLL)
#    if defined(HAMLIB_STATIC)
#      undef HAMLIB_STATIC
#    endif
#    if defined(ALL_STATIC)
#      undef ALL_STATIC
#    endif
#  endif
#  undef HAMLIB_IMPEXP
#  undef BACKEND_IMPEXP
#  undef HAMLIB_API
#  undef HAMLIB_EXPORT(type)
#  undef HAMLIB_EXPORT_VAR(type)
#  if defined(HAMLIB_DLL) && defined(IN_HAMLIB)
/* building a DLL */
#    define HAMLIB_IMPEXP __declspec(dllexport)
#    define BACKEND_IMPEXP __declspec(dllexport)
#  elif defined(HAMLIB_STATIC) || defined(ALL_STATIC)
/* building or linking to a static library */
#    define HAMLIB_IMPEXP
#    define BACKEND_IMPEXP
#  else
/* linking to the DLL */
#    define HAMLIB_IMPEXP  __declspec(dllimport)
#    define BACKEND_IMPEXP __declspec(dllexport)
#  endif
#  define HAMLIB_API __cdecl
#  define HAMLIB_EXPORT(type) HAMLIB_IMPEXP type HAMLIB_API
#  define HAMLIB_EXPORT_VAR(type) HAMLIB_IMPEXP type
#  define BACKEND_EXPORT(type) BACKEND_IMPEXP type HAMLIB_API
#  define BACKEND_EXPORT_VAR(type) BACKEND_IMPEXP type
#endif

/* Take care of non-cygwin platforms */
#if !defined(HAMLIB_IMPEXP)
#  define HAMLIB_IMPEXP
#endif
#if !defined(BACKEND_IMPEXP)
#  define BACKEND_IMPEXP
#endif
#if !defined(HAMLIB_API)
#  define HAMLIB_API
#endif
#if !defined(HAMLIB_EXPORT)
#  define HAMLIB_EXPORT(type) HAMLIB_IMPEXP type HAMLIB_API
#endif
#if !defined(HAMLIB_EXPORT_VAR)
#  define HAMLIB_EXPORT_VAR(type) HAMLIB_IMPEXP type
#endif
#if !defined(BACKEND_EXPORT)
#  define BACKEND_EXPORT(type) BACKEND_IMPEXP type HAMLIB_API
#endif
#if !defined(BACKEND_EXPORT_VAR)
#  define BACKEND_EXPORT_VAR(type) BACKEND_IMPEXP type
#endif
