/* $Id: rig_dll.h,v 1.3 2001-06-15 07:16:16 f4cfe Exp $ */


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
#  undef HAMLIB_API
#  undef HAMLIB_EXPORT(type)
#  undef HAMLIB_EXPORT_VAR(type)
#  if defined(HAMLIB_DLL)
/* building a DLL */
#    define HAMLIB_IMPEXP __declspec(dllexport)
#  elif defined(HAMLIB_STATIC) || defined(ALL_STATIC)
/* building or linking to a static library */
#    define HAMLIB_IMPEXP
#  else
/* linking to the DLL */
#    define HAMLIB_IMPEXP __declspec(dllimport)
#  endif
#  define HAMLIB_API __cdecl
#  define HAMLIB_EXPORT(type) HAMLIB_IMPEXP type HAMLIB_API
#  define HAMLIB_EXPORT_VAR(type) HAMLIB_IMPEXP type
#endif

/* Take care of non-cygwin platforms */
#if !defined(HAMLIB_IMPEXP)
#  define HAMLIB_IMPEXP
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
