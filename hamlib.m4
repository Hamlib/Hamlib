dnl
dnl AC_LIB_HAMLIB_FLAGS
dnl This just unconditionally sets the options.  It should offer an option for
dnl explicitly giving the path to hamlib on the configure command line.
dnl REM: this script file was cloned from libraw1394.m4. Thanks guys!
dnl
AC_DEFUN(AC_LIB_HAMLIB_FLAGS, [
HAMLIB_CPPFLAGS=""
HAMLIB_CFLAGS=""
HAMLIB_LIBS="-lhamlib"

AC_SUBST(HAMLIB_CPPFLAGS)
AC_SUBST(HAMLIB_CFLAGS)
AC_SUBST(HAMLIB_LIBS)
])

dnl
dnl AC_LIB_HAMLIB_HEADERS([ACTION_IF_FOUND[,ACTION_IF_NOT_FOUND]])
dnl
AC_DEFUN(AC_LIB_HAMLIB_HEADERS, [
AC_REQUIRE([AC_LIB_HAMLIB_FLAGS])

ac_hamlib_save_cppflags=$CPPFLAGS
CPPFLAGS="$HAMLIB_CPPFLAGS $CPPFLAGS"

ac_hamlib_headers=no
AC_CHECK_HEADER(hamlib/rig.h, ac_hamlib_headers=yes)

CPPFLAGS=$ac_hamlib_save_cppflags

if test $ac_hamlib_headers = yes ; then
	ifelse([$1], , :, $1)
else
	ifelse([$2], , :, $2)
fi
])


dnl
dnl AC_LIB_HAMLIB_LIBVERSION(MINIMUMVERSION[,ACTION_IF_FOUND[,ACTION_IF_NOT_FOUND]])
dnl
AC_DEFUN(AC_LIB_HAMLIB_LIBVERSION, [
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_LIB_HAMLIB_FLAGS])

ac_hamlib_save_cppflags=$CPPFLAGS
ac_hamlib_save_cflags=$CFLAGS
ac_hamlib_save_libs=$LIBS
CPPFLAGS="$HAMLIB_CPPFLAGS $CPPFLAGS"
CFLAGS="$HAMLIB_CFLAGS $CFLAGS"
LIBS="$HAMLIB_LIBS $LIBS"

ac_hamlib_versiontest_success=no
ac_hamlib_ver_symbol=`echo __hamlib_version_$1 | sed 's/\./_/g'`

AC_TRY_LINK([], [{
	extern char $ac_hamlib_ver_symbol;
	$ac_hamlib_ver_symbol++;
}], ac_hamlib_versiontest_success=yes)

CPPFLAGS=$ac_hamlib_save_cppflags
CFLAGS=$ac_hamlib_save_cflags
LIBS=$ac_hamlib_save_libs

if test $ac_hamlib_versiontest_success = yes; then
	ifelse([$2], , :, $2)
else
	ifelse([$3], , :, $3)
fi
])


dnl
dnl AC_LIB_HAMLIB_RUNTEST(MINIMUMVERSION[,ACTION_IF_FOUND
dnl                        [,ACTION_IF_NOT_FOUND[,ACTION_IF_CROSS_COMPILING]]])
AC_DEFUN(AC_LIB_HAMLIB_RUNTEST, [
ac_hamlib_save_cppflags=$CPPFLAGS
ac_hamlib_save_cflags=$CFLAGS
ac_hamlib_save_libs=$LIBS
CPPFLAGS="$HAMLIB_CPPFLAGS $CPPFLAGS"
CFLAGS="$HAMLIB_CFLAGS $CFLAGS"
LIBS="$HAMLIB_LIBS $LIBS"

dnl This program compares two version strings and returns with code 0 if
dnl req_ver <= lib_ver, returns 1 otherwise.
dnl 
dnl "1.23" < "1.23.1"   (missing fields assumed zero)
dnl "1.23pre" <> "1.23" (undefined, do not use text as version)
dnl "1.21" > "1.3"      (no implicit delimiters)
AC_TRY_RUN([
#include <stdlib.h>
#include <hamlib/rig.h>

int main()
{
        char *req_ver, *lib_ver;
        unsigned int req_i, lib_i;

        req_ver = "$1";
        lib_ver = hamlib_version;

        while (1) {
                req_i = strtoul(req_ver, &req_ver, 10);
                lib_i = strtoul(lib_ver, &lib_ver, 10);

                if (req_i > lib_i) exit(1);
                if (req_i < lib_i) exit(0);

                if (*req_ver != '.' || *lib_ver != '.') exit(0);

                req_ver++;
                lib_ver++;
        }
}
], ac_hamlib_run=yes, ac_hamlib_run=no, ac_hamlib_run=cross)


CPPFLAGS=$ac_hamlib_save_cppflags
CFLAGS=$ac_hamlib_save_cflags
LIBS=$ac_hamlib_save_libs

if test $ac_hamlib_run = yes; then
	ifelse([$2], , :, $2)
elif test $ac_hamlib_run = no; then
	ifelse([$3], , :, $3)
else
	ifelse([$4], ,
               AC_MSG_ERROR([no default for cross compiling in hamlib runtest macro]),
               [$4])
fi
])

dnl
dnl AC_LIB_HAMLIB(MINIMUMVERSION[,ACTION_IF_FOUND[,ACTION_IF_NOT_FOUND]])
dnl
dnl Versions before 0.9 can't be checked, so this will always fail if the
dnl installed hamlib is older than 0.9 as if the library weren't found.
dnl
AC_DEFUN(AC_LIB_HAMLIB, [

AC_LIB_HAMLIB_FLAGS
AC_LIB_HAMLIB_HEADERS(ac_hamlib_found=yes, ac_hamlib_found=no)

if test $ac_hamlib_found = yes ; then

AC_MSG_CHECKING(for hamlib version >= [$1])
AC_LIB_HAMLIB_RUNTEST([$1], , ac_hamlib_found=no,
                       AC_LIB_HAMLIB_LIBVERSION([$1], , ac_hamlib_found=no))

if test $ac_hamlib_found = yes ; then
	AC_MSG_RESULT(yes)
	$2
else
	AC_MSG_RESULT(no)
	$3
fi

fi

])
