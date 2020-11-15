AU_ALIAS([VL_LIB_NOVA], [AX_LIB_NOVA])
AC_DEFUN([AX_LIB_NOVA], [
  AC_CACHE_CHECK([for nova library],
                 ax_cv_lib_nova, [
    AC_LANG_PUSH(C++)
    AC_CHECK_HEADER(libnova/libnova.h, ax_cv_lib_nova="-lnova -lstdc++ -lz")
    AC_LANG_POP()

    if test -z "$ax_cv_lib_nova"; then
      ax_cv_lib_nova="no"
    fi
  ])

  if test "$ax_cv_lib_nova" != "no"; then
    ORIG_LIBS="$LIBS"
    LIBS="$LIBS $ax_cv_lib_nova"
    AC_DEFINE(HAVE_LIBNOVA, 1,
              [Define if you have a nova compatible library])
    LIBS="$ORIG_LIBS"
    NOVA_LIBS="$ax_cv_lib_nova"
    AC_SUBST([NOVA_LIBS])
  fi
])
