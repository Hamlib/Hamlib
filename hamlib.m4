dnl Configure Paths for Hamlib
dnl Cloned from Alsa project http://www.alsa-project.org
dnl AM_PATH_HAMLIB([MINIMUM-VERSION [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for libhamlib, and define HAMLIB_LTDL,
dnl HAMLIB_CFLAGS and HAMLIB_LIBS as appropriate.
dnl enables arguments --with-hamlib-prefix=
dnl                   --with-hamlib-inc-prefix=
dnl                   --disable-hamlibtest  (this has no effect, as yet)
dnl
dnl For backwards compatibility, if ACTION_IF_NOT_FOUND is not specified,
dnl and the hamlib libraries are not found, a fatal AC_MSG_ERROR() will result.
dnl
AC_DEFUN([AM_PATH_HAMLIB],
[dnl Save the original CFLAGS, LDFLAGS, and LIBS
hamlib_save_CFLAGS="$CFLAGS"
hamlib_save_LDFLAGS="$LDFLAGS"
hamlib_save_LIBS="$LIBS"
hamlib_found=yes

dnl
dnl Get the cflags and libraries for hamlib
dnl
AC_ARG_WITH(hamlib-prefix,
[  --with-hamlib-prefix=PFX  Prefix where Hamlib library is installed(optional)],
[hamlib_prefix="$withval"], [hamlib_prefix=""])

AC_ARG_WITH(hamlib-inc-prefix,
[  --with-hamlib-inc-prefix=PFX  Prefix where include libraries are (optional)],
[hamlib_inc_prefix="$withval"], [hamlib_inc_prefix=""])

dnl FIXME: this is not yet implemented
AC_ARG_ENABLE(hamlibtest,
[  --disable-hamlibtest      Do not try to compile and run a test Hamlib program],
[enable_hamlibtest=no],
[enable_hamlibtest=yes])

dnl Add any special include directories
AC_MSG_CHECKING(for HAMLIB CFLAGS)
if test "$hamlib_inc_prefix" != "" ; then
	HAMLIB_CFLAGS="$HAMLIB_CFLAGS -I$hamlib_inc_prefix"
	CFLAGS="$CFLAGS -I$hamlib_inc_prefix $LIBUSB_CFLAGS"
fi
AC_MSG_RESULT($HAMLIB_CFLAGS)

dnl add any special lib dirs
AC_MSG_CHECKING(for HAMLIB LDFLAGS)
if test "$hamlib_prefix" != "" ; then
	HAMLIB_LIBS="$HAMLIB_LIBS -L$hamlib_prefix -Wl,--rpath -Wl,$hamlib_prefix"
	LDFLAGS="$LDFLAGS $HAMLIB_LIBS $LIBUSB_LDFLAGS"
fi

dnl add the hamlib library
HAMLIB_LIBS="$HAMLIB_LIBS -lhamlib -lm -ldl"
LIBS=$(echo $LIBS | sed 's/-lm//')
LIBS=$(echo $LIBS | sed 's/-ldl//')
LIBS=$(echo $LIBS | sed 's/  //')
#LIBS="$HAMLIB_LIBS $LIBS"
AC_MSG_RESULT($HAMLIB_LIBS)

dnl add any special LTDL file
AC_MSG_CHECKING(for HAMLIB LTDL)
if test "$hamlib_prefix" != "" ; then
	HAMLIB_LTDL="$HAMLIB_LTDL $hamlib_prefix/libhamlib.la"
fi
AC_MSG_RESULT($HAMLIB_LTDL)

dnl Check for a working version of libhamlib that is of the right version.
dnl FIXME: not implemented yet!

dnl Now that we know that we have the right version, let's see if we have the library and not just the headers.
#AC_CHECK_LIB([hamlib], [rig_init],,
#	[ifelse([$3], , [AC_MSG_ERROR(No linkable libhamlib was found.)])
#	 hamlib_found=no]
#)

if test "x$hamlib_found" = "xyes" ; then
   ifelse([$2], , :, [$2])
   LIBS=$(echo $LIBS | sed 's/-lhamlib//g')
   LIBS=$(echo $LIBS | sed 's/  //')
   LIBS="-lhamlib $LIBS"
fi
if test "x$hamlib_found" = "xno" ; then
   ifelse([$3], , :, [$3])
   CFLAGS="$hamlib_save_CFLAGS"
   LDFLAGS="$hamlib_save_LDFLAGS"
   LIBS="$hamlib_save_LIBS"
   HAMLIB_CFLAGS=""
   HAMLIB_LIBS=""
fi

dnl That should be it.  Now just export out symbols:
AC_SUBST(HAMLIB_CFLAGS)
AC_SUBST(HAMLIB_LIBS)
AC_SUBST(HAMLIB_LTDL)
])
