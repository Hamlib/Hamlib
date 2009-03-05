/* $Id: rig_dll.h,v 1.11 2007-05-21 21:41:59 fillods Exp $ */

/*
 * Provide definitions to compile in Windows
 * using C-friendly options, e.g.
 *
 * HAMLIB_API -> __cdecl
 * HAMLIB_EXPORT, HAMLIB_EXPORT_VAR -> __declspec(dllexport)
 * BACKEND_EXPORT, BACKEND_EXPORT_VAR -> __declspec(dllexport)
 *
 * No effect in non-Windows environments.
 */

#if defined(_WIN32) && !defined(__CYGWIN__)
#  undef HAMLIB_IMPEXP
#  undef BACKEND_IMPEXP
#  undef HAMLIB_API
#  undef HAMLIB_EXPORT
#  undef HAMLIB_EXPORT_VAR
#  undef BACKEND_EXPORT
#  undef BACKEND_EXPORT_VAR
#  undef HAMLIB_DLL_IMPORT
#  undef HAMLIB_DLL_EXPORT

#  if defined (__BORLANDC__)
#  define HAMLIB_DLL_IMPORT __import
#  define HAMLIB_DLL_EXPORT __export
#  else
#  define HAMLIB_DLL_IMPORT __declspec(dllimport)
#  define HAMLIB_DLL_EXPORT __declspec(dllexport)
#  endif

#  ifdef DLL_EXPORT
     /* HAMLIB_API may be set to __stdcall for VB, .. */
#    define HAMLIB_API __cdecl
#    ifdef IN_HAMLIB
#      define BACKEND_IMPEXP HAMLIB_DLL_EXPORT
#      define HAMLIB_IMPEXP HAMLIB_DLL_EXPORT
#    else
#      define BACKEND_IMPEXP HAMLIB_DLL_EXPORT
#      define HAMLIB_IMPEXP HAMLIB_DLL_IMPORT
#    endif
#  else
       /* static build, only export the backend entry points for lt_dlsym */
#      define BACKEND_IMPEXP HAMLIB_DLL_EXPORT
#  endif
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


