dnl Process this file with autoconf to produce a configure script.

AC_INIT(hamlib, 1.1.3, hamlib-developer@lists.sourceforge.net)
AM_INIT_AUTOMAKE(hamlib, 1.1.3)
AC_CONFIG_SRCDIR([include/hamlib/rig.h])
AM_CONFIG_HEADER(include/config.h)

# Minimum Autoconf version required.
AC_PREREQ(2.50)

dnl directory for docs (html)
hamlibdocdir=$datadir/doc/hamlib
AC_SUBST(hamlibdocdir)

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_AIX
AC_ISC_POSIX
AC_MINIX
AM_PROG_CC_STDC
AC_PROG_GCC_TRADITIONAL

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([alloca.h argz.h malloc.h memory.h string.h strings.h])
AC_CHECK_HEADERS([stdlib.h values.h rpc/rpc.h])
AC_CHECK_HEADERS([fcntl.h sys/ioctl.h sys/time.h unistd.h getopt.h errno.h])
AC_CHECK_HEADERS([sys/ioccom.h linux/ppdev.h sgtty.h term.h termio.h termios.h])
AC_CHECK_HEADERS([windows.h winioctl.h winbase.h])

AM_SYS_POSIX_TERMIOS

AC_MSG_CHECKING(if you want to build C++ binding and demo)
AC_ARG_WITH(cxx-binding,
        [  --without-cxx-binding   do not build C++ binding and demo],
        [cf_with_cxx_binding=$withval],
        [cf_with_cxx_binding=$cf_with_cxx])
AC_MSG_RESULT($cf_with_cxx_binding)


INCLUDES="${INCLUDES} -I\$(top_srcdir)/include -I\$(top_srcdir)/src -I\$(top_srcdir)/lib"
CPPFLAGS="${CPPFLAGS} -D_GNU_SOURCE"
if test "${ac_cv_c_compiler_gnu}" = "yes"; then
  CFLAGS="${CFLAGS} -Wall"
fi
if test "${ac_cv_cxx_compiler_gnu}" = "yes"; then
  CXXFLAGS="${CXXFLAGS} -Wall"
fi

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_TYPE_SIGNAL
AC_CHECK_TYPES([siginfo_t],[],[],[#include <signal.h>])

dnl Checks for libraries.

dnl AC_CHECK_LIB(intl,gettext)
dnl AC_CHECK_LIB(nsl,gethostbyaddr)
dnl AC_CHECK_LIB(socket,socket)
AC_CHECK_LIB(syslog,syslog)             # OS/2 needs this

dnl Checks for library functions.
AC_CHECK_FUNCS([atexit snprintf select gettimeofday memmove memset])
AC_CHECK_FUNCS([strcasecmp strchr strdup strerror strrchr strstr strtol])
AC_CHECK_FUNCS(getopt_long,,LIBOBJS="$LIBOBJS getopt.o getopt1.o")
AC_CHECK_FUNCS(usleep,,LIBOBJS="$LIBOBJS usleep.o")
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_VPRINTF

dnl initialize libtool
dnl Enable building of the convenience library
dnl and set LIBLTDL accordingly
AC_LIBLTDL_CONVENIENCE
dnl Substitute INCLTDL and LIBLTDL in the Makefiles
AC_SUBST(INCLTDL)
AC_SUBST(LIBLTDL)
dnl Check for dlopen support
AC_LIBTOOL_WIN32_DLL
AC_LIBTOOL_DLOPEN
dnl Configure libtool
AM_PROG_LIBTOOL
dnl Configure libltdl
AC_CONFIG_SUBDIRS(libltdl)

dnl Check if libgd-dev is installed, so we can enable rigmatrix

AC_ARG_ENABLE([rigmatrix],
[  --enable-rigmatrix	Generate rigmatrix tool (requires libgd)],
[case "${enableval}" in
	yes) wantrigmatrix=true ;;
	no)  wantrigmatrix=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-rigmatrix) ;;
	esac],[wantrigmatrix=false])

AM_CONDITIONAL(WANTRIGMATRIX, test x$wantrigmatrix = xtrue)

if test "x${wantrigmatrix}" != "xfalse"; then
AC_CHECK_HEADERS([gd.h],
  [AC_CHECK_LIB([gd],[gdImageCreate],[enable_rigmatrix=yes], [enable_rigmatrix=no],[-lz])])
if test "${enable_rigmatrix}" = "no"; then
  echo "disabling rigmatrix generation"
  RIGMATRIX=
else
  echo "enabling rigmatrix generation"
  RIGMATRIX=rigmatrix
fi
else
  echo "disabling rigmatrix generation"
  RIGMATRIX=
fi
AC_SUBST(RIGMATRIX)

dnl Check if tcl includes are there, so we can enable HamlibTCL
dnl FIXME!

AC_CHECK_HEADERS(tcl8.2/tcl.h)
if test "${ac_cv_header_tcl8_2_tcl_h}" = "no" ; then
  echo "disabling HamlibTCL support"
  LIBHAMLIBTCL=
  LTHAMLIBTCL=
else
  echo "enabling HamlibTCL support"
  LIBHAMLIBTCL=libhamlibtcl.a
  LTHAMLIBTCL=libhamlibtcl.la
fi
AM_CONDITIONAL(HAMLIBTCL, test x$LTHAMLIBTCL != x)
AC_SUBST(LIBHAMLIBTCL)
AC_SUBST(LTHAMLIBTCL)



AC_SUBST(INCLUDES)

if test "${ac_cv_header_sys_socket_h}" = "no"; then
  echo "disabling NET port"
  NET=
else
  echo "enabling NET port"
  NET=net
fi
AC_SUBST(NET)

if test "${enable_shared}" = "no"; then
  enable_preload=yes
fi
if test "${enable_dynamic}" = "no" || test "${enable_preload}" = "yes"; then
  echo "preloading backends into DLL"
  DLL_PRELOAD="\$(PRELOADABLE_BACKENDS)"
else
  DLL_PRELOAD=""
fi
AC_SUBST(DLL_PRELOAD)
AC_SUBST(LIBOBJS)

case "$host_os" in
 linux-gnu*)
 # Winradio only under Linux (until someone port it on other os)
 BACKEND_LIST="icom kenwood aor yaesu dummy pcr alinco winradio uniden tentec kachina jrc"
 ;;
 *) 
 BACKEND_LIST="icom kenwood aor yaesu dummy pcr alinco uniden tentec kachina jrc"
esac

if test "${ac_cv_header_rpc_rpc_h}" = "yes" ; then
 BACKEND_LIST="$BACKEND_LIST rpcrig"
fi

for be in ${BACKEND_LIST} ; do
	BACKENDLNK="${BACKENDLNK} -dlopen ../${be}/libhamlib-${be}.la"
	BACKENDEPS="${BACKENDEPS} ../${be}/libhamlib-${be}.la"
done
AC_SUBST(BACKEND_LIST)
AC_SUBST(BACKENDLNK)
AC_SUBST(BACKENDEPS)

AC_CONFIG_FILES([Makefile
include/Makefile
include/hamlib/Makefile
lib/Makefile
dummy/Makefile
yaesu/Makefile
icom/Makefile
aor/Makefile
kenwood/Makefile
winradio/Makefile
winradio/linradio/Makefile
pcr/Makefile
alinco/Makefile
uniden/Makefile
tentec/Makefile
kachina/Makefile
jrc/Makefile
rpcrig/Makefile
src/Makefile
c++/Makefile
tcl/Makefile
tests/Makefile
doc/Makefile
hamlib.spec])
AC_OUTPUT

