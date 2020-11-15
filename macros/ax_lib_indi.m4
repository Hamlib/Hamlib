AU_ALIAS([VL_LIB_INDI], [AX_LIB_INDI])
AC_DEFUN([AX_LIB_INDI], [
  AC_CACHE_CHECK([for INDI library],
               ax_cv_lib_indi, [
    AC_LANG_PUSH(C++)
    AC_CHECK_HEADER(libindi/baseclient.h, ax_cv_lib_indi="-lindiclient -lstdc++ -lz")
    AC_LANG_POP()

    if test -z "$ax_cv_lib_indi"; then
      ax_cv_lib_indi="no"
    fi
  ])

  if test "$ax_cv_lib_indi" != "no"; then
    ORIG_LIBS="$LIBS"
    LIBS="$LIBS $ax_cv_lib_indi"
    AC_DEFINE(HAVE_LIBINDI, 1,
              [Define if you have an INDI compatible library])
    LIBS="$ORIG_LIBS"
    INDI_LIBS="$ax_cv_lib_indi"
    AC_SUBST([INDI_LIBS])
  fi
])
