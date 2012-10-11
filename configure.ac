## Process this file with autoconf to create configure. -*- autoconf -*-

dnl Autoconf 2.67 is in Debian Squeeze--is an older version needed
dnl for some other distribution?
## FIXME: Is this too new? ##
AC_PREREQ([2.67])


## ------------------------ ##
## Autoconf initialisation. ##
## ------------------------ ##
dnl Please do not use '-' in the version number, as package managers will fail,
dnl however, the use of '~' should be fine as apt (others?) will treat
dnl it as an earlier version than the actual release.  TNX KA6MAL
dnl PACKAGE_NAME + " " + PACKAGE_VERSION must not exceed 20 chars!
AC_INIT([Hamlib],
        [3.0~git],
        [hamlib-developer@lists.sourceforge.net],
        [hamlib],
        [http://www.hamlib.org])

AC_CONFIG_SRCDIR([include/hamlib/rig.h])
AC_CONFIG_MACRO_DIR([macros])
AC_CONFIG_HEADERS([include/config.h])


## ------------------------ ##
## Automake Initialisation. ##
## ------------------------ ##
dnl Passing AC_PACKAGE_TARNAME, AC_PACKAGE_VERSION to AM_INIT_AUTOMAKE is
dnl obsolete as these values are obtained from the AC_INIT macro.
AM_INIT_AUTOMAKE([-Wall])

dnl Clean compilation output makes compiler warnings more visible
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl TODO: Maintainer mode has arguments against its use.
dnl       Consider its removal at some point.
dnl AM_MAINTAINER_MODE


## ------------------------------ ##
## Hamlib specific configuration. ##
## ------------------------------ ##
dnl New backends must be listed here!  Also the new Makefile path must be
dnl added to AC_CONFIG_FILES near the end of this file.  See README.developer
BACKEND_LIST="icom kenwood aor yaesu dummy pcr alinco uniden tentec kachina jrc drake lowe rft rs kit skanti prm80 tapr flexradio wj racal tuner adat"
ROT_BACKEND_LIST="dummy easycomm fodtrack gs232a heathkit kit rotorez sartek spid ars m2 amsat ts7400 celestron"

dnl See README.release on setting these values
# Values given to -version-info when linking.  See libtool documentation.
# Set them here to keep c++/Makefile and src/Makefile in sync.
ABI_VERSION=3
ABI_REVISION=0
ABI_AGE=1

AC_DEFINE_UNQUOTED([ABI_VERSION], [$ABI_VERSION], [Frontend ABI version])
AC_DEFINE_UNQUOTED([ABI_REVISION], [$ABI_REVISION], [Frontend ABI revision])
AC_DEFINE_UNQUOTED([ABI_AGE], [$ABI_AGE], [Frontend ABI age])

AC_SUBST([ABI_VERSION])
AC_SUBST([ABI_REVISION])
AC_SUBST([ABI_AGE])

dnl Pick up the Hamlib macros.
dnl macros/aclocal-include.m4
dnl AM_ACLOCAL_INCLUDE([macros])

# directory for docs (html)
hamlibdocdir=$datadir/doc/hamlib
AC_SUBST([hamlibdocdir])

# Add Hamlib header include paths
AM_CPPFLAGS="${AM_CPPFLAGS} -I\$(top_srcdir)/include -I\$(top_srcdir)/src -I\$(top_srcdir)/lib"


## --------------- ##
## Program checks  ##
## --------------- ##
dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl Pkgconfig assists when using certain external packages
PKG_PROG_PKG_CONFIG

dnl AC_AIX
dnl AC_ISC_POSIX
dnl AC_MINIX

dnl AC_PROG_CC

dnl AC_DIAGNOSE([obsolete],[AM_PROG_CC_STDC:
dnl     your code should no longer depend upon 'am_cv_prog_cc_stdc', but upon
dnl     'ac_cv_prog_cc_stdc'.  Remove this warning and the assignment when
dnl     you adjust the code.  You can also remove the above call to
dnl     AC_PROG_CC if you already called it elsewhere.])
dnl am_cv_prog_cc_stdc=$ac_cv_prog_cc_stdc

## ----------------------------------- ##
## Miscellaneous configuration macros  ##
## ----------------------------------- ##
dnl The latest Autoconf manual (2.69) states these macros are obsolecent.
AC_PROG_GCC_TRADITIONAL
AC_C_CONST

AC_C_INLINE

dnl Set warnings for compilation
dnl LF_SET_WARNINGS
# macros/ax_cflags_warn_all.m4
AX_CFLAGS_WARN_ALL
AX_CXXFLAGS_WARN_ALL

dnl Automake macro for rigmem compilation
AM_PROG_CC_C_O


## ------------------------------------------ ##
## Various parts of Hamlib need these headers ##
## ------------------------------------------ ##
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([alloca.h argz.h malloc.h memory.h string.h strings.h])
AC_CHECK_HEADERS([stdlib.h values.h net/errno.h])
AC_CHECK_HEADERS([fcntl.h sys/ioctl.h sys/time.h sys/param.h unistd.h getopt.h errno.h])
AC_CHECK_HEADERS([sys/ioccom.h sgtty.h term.h termio.h termios.h])
AC_CHECK_HEADERS([linux/ppdev.h linux/parport.h linux/ioctl.h linux/hidraw.h])
AC_CHECK_HEADERS([dev/ppbus/ppi.h dev/ppbus/ppbconf.h])
AC_CHECK_HEADERS([sys/socket.h netinet/in.h netdb.h arpa/inet.h ws2tcpip.h])


## ------------------------------------ ##
## System capability and library checks ##
## ------------------------------------ ##
dnl Check for Mingw (Win32 Sleep) support
# macros/gr_pwin32.m4
GR_PWIN32

dnl macros/ax_pthread.m4
AX_PTHREAD
AS_IF([test x"$ax_pthread_ok" = "xyes"], [
      CFLAGS="${CFLAGS} ${PTHREAD_CFLAGS}"
      CXXFLAGS="${CFLAGS} ${PTHREAD_CFLAGS}"
])

AC_SYS_POSIX_TERMIOS()


dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_HEADER_TIME
dnl AC_TYPE_SIGNAL
AC_CHECK_TYPES([siginfo_t],[],[],[[#include <signal.h>]])


dnl Checks for libraries.

# The following comes from SC_TCL_LINK_LIBS
# we redefine them here because we want a separate NET_LIBS var

#--------------------------------------------------------------------
# On a few very rare systems, all of the libm.a stuff is
# already in libc.a.  Set compiler flags accordingly.
#--------------------------------------------------------------------
AC_CHECK_FUNC([sin], [MATH_LIBS=""], [MATH_LIBS="-lm"])
AC_SUBST([MATH_LIBS])

dnl Interactive UNIX?  You have got to be kidding!
dnl #--------------------------------------------------------------------
dnl # Interactive UNIX requires -linet instead of -lsocket, plus it
dnl # needs net/errno.h to define the socket-related error codes.
dnl #--------------------------------------------------------------------
dnl AC_CHECK_LIB([inet], [main], [LIBS="$LIBS -linet"], [], [])

#--------------------------------------------------------------------
#   Check for the existence of the -lsocket and -lnsl libraries.
#   The order here is important, so that they end up in the right
#   order in the command line generated by make.  Here are some
#   special considerations:
#   1. Use "connect" and "accept" to check for -lsocket, and
#      "gethostbyname" to check for -lnsl.
#   2. Use each function name only once:  cannot redo a check because
#      autoconf caches the results of the last check and will not redo it.
#   3. Use -lnsl and -lsocket only if they supply procedures that
#      are not already present in the normal libraries.  This is because
#      IRIX 5.2 has libraries, but they aren't needed and they're
#      bogus:  they goof up name resolution if used.
#   4. On some SVR4 systems, can't use -lsocket without -lnsl too.
#      To get around this problem, check for both libraries together
#      if -lsocket doesn't work by itself.
#--------------------------------------------------------------------
hl_checkBoth=0
hl_oldLibs=$LIBS

AC_CHECK_FUNC([connect], [hl_checkSocket=0], [hl_checkSocket=1])

AS_IF([test "$hl_checkSocket" = 1], [
        AC_CHECK_LIB([socket],
                     [main],
                     [NET_LIBS="$NET_LIBS -lsocket"],
                     [hl_checkBoth=1],
                     [])
        ])

AS_IF([test "$hl_checkBoth" = 1], [
        hl2_oldLibs=$LIBS
        LIBS="$LIBS -lsocket -lnsl"
        AC_CHECK_FUNC([accept], [hl_checkNsl=0], [LIBS=$hl2_oldLibs])
        ])

# separate Name Service Library
AC_CHECK_FUNC([gethostbyname],
              [],
              [AC_CHECK_LIB([nsl],
                            [main],
                            [NET_LIBS="$NET_LIBS -lnsl"],
                            [],
                            [])
              ])

# Winsock2 32 bit
AC_CHECK_FUNC([gethostbyname],
              [],
              [AC_CHECK_LIB([ws2_32],
                            [main],
                            [NET_LIBS="$NET_LIBS -lws2_32"],
                            [],
                            [])
              ])

LIBS="$LIBS $NET_LIBS"

dnl macros/hl_getaddrinfo.m4
HL_GETADDRINFO

LIBS=$hl_oldLibs
AC_SUBST([NET_LIBS])
## End Hamlib socket test ##


dnl No source file tests for HAVE_LIBSYSLOG set by the following
dnl AC_CHECK_LIB macro so commenting.  May remove later.
dnl # OS/2 needs this
dnl AC_CHECK_LIB(syslog,syslog)

dnl Checks for library functions.
AC_CHECK_FUNCS([atexit snprintf select memmove memset])
AC_CHECK_FUNCS([strcasecmp strchr strdup strerror strrchr strstr strtol])
AC_CHECK_FUNCS([cfmakeraw setitimer ioctl sigaction])
AC_FUNC_ALLOCA

dnl Does not seem to be needed anymore
dnl AC_FUNC_MALLOC

dnl Obsolecent macro
dnl AC_FUNC_VPRINTF

AC_LIBOBJ([termios])
AC_LIBOBJ([dummy])


## ------------------------ ##
## libtool  Initialisation. ##
## ------------------------ ##

LT_PREREQ([2.2.6b])

dnl Name the subdirectory that contains libltdl sources
LT_CONFIG_LTDL_DIR([libltdl])

dnl N.B. LT_INIT([dlopen], [win32-dll]) does not work for older libtool
dnl so require new enough version above.  Disable static library builds as
dnl the frontend and backend libs are not linked.  May need a recipe outside
dnl of Libtool if these are needed.
LT_INIT([dlopen win32-dll disable-static])

dnl Enable building of the convenience libltdl library
dnl (requires --with-included-ltdl be given to configure for the included
dnl libltdl to be used instead of an installed version), useful on systems
dnl where libltdl-dev is not installed.
LTDL_INIT([convenience])


## -------------------------- ##
## Custom host configurations ##
## -------------------------- ##

AS_CASE(["$host_os"],
    [freebsd*], [
        AM_CPPFLAGS="-I/usr/local/include ${AM_CPPFLAGS}"
        AM_LDFLAGS="${AM_LDFLAGS} -L/usr/local/lib"
        AC_SUBST([AM_LDFLAGS])],

    [darwin* | rhapsody*], [
        # Trick from http://fink.sourceforge.net/doc/porting/
        # TODO: check the compiler actually does support these options
        CFLAGS="${CFLAGS} -no-cpp-precomp"
        CXXFLAGS="${CXXFLAGS} -no-cpp-precomp"

        # Tell the OS X linker to allocate enough space inside the
        # libhamlib.X.dylib shared object for install_name_tool(1) to
        # work. This is useful when including hamlib in an app bundle.
        OSXLDFLAGS="-Wl,-headerpad_max_install_names"
        AC_SUBST([OSXLDFLAGS])],

    [mingw* | pw32* | cygwin*], [
        WINLDFLAGS="-Wl,--output-def,libhamlib.def -Wl,--add-stdcall-alias"
        AC_SUBST([WINLDFLAGS])

        # Enable ld's "auto import" for executables
        WINEXELDFLAGS="-Wl,--enable-auto-import"
        AC_SUBST([WINEXELDFLAGS])
    ])


## ---------------------- ##
## External package tests ##
## ---------------------- ##

dnl Check whether we can actually find ltdl.h
dnl (only needed if not using included libltdl).
AS_IF([test x"${with_included_ltdl}" = "xno"], [
      AC_CHECK_HEADER([ltdl.h],
                      [AC_MSG_RESULT([yes])],
                      [AC_MSG_ERROR([""
"ltdl.h not found.  Please install the libltdl development files package which"
"provides /usr/include/ltdl.h (perhaps called 'libltdl-dev' or 'libltdl-devel')."
                      ])
            ])
      ])


dnl Check if C99 struct initializers are supported
AC_MSG_CHECKING([whether C99 struct/array initializers are supported])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],
                  [[struct{char a;int b;}s[8]={[3]={.b=5}};]])],
                  [AC_MSG_RESULT(yes)],
                  [AC_MSG_ERROR([""
"You need a C99 compliant C compiler that supports struct/array intializers."
"Have you considered GCC lately?."])
                  ])


# libxml2 required rigmem xml support, make it user optional
AC_MSG_CHECKING([whether to build rigmem XML support])
AC_ARG_WITH([xml-support],
            [AS_HELP_STRING([--with-xml-support],
                            [build rigmem with XML support @<:@default=no@:>@])],
            [cf_with_xml_support=$withval],
            [cf_with_xml_support=no])
AC_MSG_RESULT([$cf_with_xml_support])

AS_IF([test x"${cf_with_xml_support}" = "xyes"], [
      PKG_CHECK_MODULES([LIBXML2],
                        [libxml-2.0],
                        [AC_DEFINE([HAVE_XML2],
                                   [1],
                                   [Define if libxml2 is available])],
                        [AC_MSG_WARN([libxml-2.0 pkg-config not found, XML support will be disabled])
                        ])
      ])

AC_SUBST([LIBXML2_LIBS])
AC_SUBST([LIBXML2_CFLAGS])


dnl Check if libgd-dev is installed, so we can enable rigmatrix
AC_ARG_ENABLE([rigmatrix],
              [AS_HELP_STRING([--enable-rigmatrix],
                              [generate rigmatrix tool (requires libgd)])],
              [AC_CHECK_HEADERS([gd.h],
                                [AC_CHECK_LIB([gd],
                                              [gdImageCreate],
                                              [enable_rigmatrix=yes],
                                              [enable_rigmatrix=no],
                                              [-lz])
                                ])],
              [enable_rigmatrix=no])

AC_MSG_CHECKING([whether to build rigmatrix])
AC_MSG_RESULT([$enable_rigmatrix])
AS_IF([test x"${enable_rigmatrix}" = "xno"],
      [RIGMATRIX=],
      [RIGMATRIX="rigmatrix"])

AC_SUBST([RIGMATRIX])


## ----------------- ##
## Language bindings ##
## ----------------- ##

BINDINGS=""
BINDING_ALL=""
BINDING_CHECK=""
BINDING_CLEAN=""
BINDING_DISTCLEAN=""
BINDING_DISTCHECK=""
BINDING_INSTALL_EXEC=""
BINDING_UNINSTALL=""
BINDING_LIST=""
BINDING_LIB_TARGETS=""


dnl Check if cxx-binding not wanted, default is to build it

# C++ binding
# TODO: check whether CXX is functional
AC_CHECK_PROG([cf_with_cxx], ["${CXX}"], [yes], [no])

AC_MSG_CHECKING([whether to build C++ binding and demo])
AC_ARG_WITH([cxx-binding],
            [AS_HELP_STRING([--without-cxx-binding],
                            [do not build C++ binding and demo @<:@default=yes@:>@])],
            [cf_with_cxx_binding=$withval],
            [cf_with_cxx_binding=$cf_with_cxx])
AC_MSG_RESULT([$cf_with_cxx_binding])

AS_IF([test x"${cf_with_cxx_binding}" = "xyes"],
      [BINDINGS="${BINDINGS} c++"])


dnl Check if perl-binding is wanted, default is to not build it

# Perl binding
AC_MSG_CHECKING([whether to build perl binding and demo])
AC_ARG_WITH([perl-binding],
            [AS_HELP_STRING([--with-perl-binding],
                            [build perl binding and demo @<:@default=no@:>@])],
            [cf_with_perl_binding=$withval],
            [cf_with_perl_binding=no])
AC_MSG_RESULT([$cf_with_perl_binding])

dnl SC_PATH_PERLINC from macros/perl.m4
AS_IF([test x"${cf_with_perl_binding}" = "xyes"],[
      SC_PATH_PERLINC
      BINDING_LIST="${BINDING_LIST} perl"
      BINDING_ALL="${BINDING_ALL} all-perl"
      BINDING_CHECK="${BINDING_CHECK} check-perl"
      BINDING_CLEAN="${BINDING_CLEAN} clean-perl"
      BINDING_DISTCLEAN="${BINDING_DISTCLEAN} distclean-perl"
      BINDING_DISTCHECK="${BINDING_DISTCHECK} distcheck-perl"
      BINDING_INSTALL_EXEC="${BINDING_INSTALL_EXEC} install-perl"
      BINDING_UNINSTALL="${BINDING_UNINSTALL} uninstall-perl"
      ])


dnl Kylix binding has been removed because unmaintained. Volunteers welcome.


# Tcl binding
AC_MSG_CHECKING([Whether to build Tcl bindings and demos])
AC_ARG_WITH([tcl-binding],
            [AS_HELP_STRING([--with-tcl-binding],
                            [build Tcl binding and demo @<:@default=no@:>@])],
            [build_tcl=$withval],
            [build_tcl=no])
AC_MSG_RESULT([$build_tcl])

dnl SC_PATH_TCLCONFIG and SC_LOAD_TCLCONFIG from macros/tcl.m4
AS_IF([test x"${build_tcl}" = "xyes"],[
      SC_PATH_TCLCONFIG
      SC_LOAD_TCLCONFIG

      tcl_save_CPPFLAGS=$CPPFLAGS
      CPPFLAGS="$CPPFLAGS $TCL_INCLUDE_SPEC"
      AC_CHECK_HEADERS([tcl.h],
                       [],
                       [AC_MSG_ERROR([Unable to find Tcl headers])])
      CPPFLAGS=$tcl_save_CPPFLAGS

      BINDING_LIST="${BINDING_LIST} tcl"
      BINDING_ALL="${BINDING_ALL} all-tcl"
      BINDING_CHECK="${BINDING_CHECK} check-tcl"
      BINDING_CLEAN="${BINDING_CLEAN} clean-tcl"
      BINDING_DISTCLEAN="${BINDING_DISTCLEAN} distclean-tcl"
      BINDING_INSTALL_EXEC="${BINDING_INSTALL_EXEC} install-tcl"
      BINDING_UNINSTALL="${BINDING_UNINSTALL} uninstall-tcl"
      BINDING_LIB_TARGETS="${BINDING_LIB_TARGETS} \$(tcl_ltlib)"])


AM_CONDITIONAL([ENABLE_TCL], [test x"${build_tcl}" = "xyes"])
AC_SUBST([TCL_LIB_SPEC])
AC_SUBST([TCL_INCLUDE_SPEC])
AC_SUBST([TCL_SHLIB_SUFFIX])


dnl Not sure where this goes...
# TODO: require "${ac_cv_header_sys_socket_h}" = "no"


dnl Check for python availability, so we can enable HamlibPy
# Python bindings
AC_MSG_CHECKING([whether to build python binding and demo])
AC_ARG_WITH([python-binding],
            [AS_HELP_STRING([--with-python-binding],
                            [build python binding and demo @<:@default=no@:>@])],
            [cf_with_python_binding=$withval],
            [cf_with_python_binding=no])
AC_MSG_RESULT([$cf_with_python_binding])

dnl AX_PYTHON_DEVEL from macros/ax_python_devel.m4
AS_IF([test x"${cf_with_python_binding}" = "xyes"],[
      AM_PATH_PYTHON([2.1],, [:])
      AX_PYTHON_DEVEL

      BINDING_LIST="${BINDING_LIST} python"
      BINDING_ALL="${BINDING_ALL} all-py"
      BINDING_CHECK="${BINDING_CHECK} check-py"
      BINDING_CLEAN="${BINDING_CLEAN} clean-py"
      BINDING_DISTCLEAN="${BINDING_DISTCLEAN} distclean-py"
      BINDING_INSTALL_EXEC="${BINDING_INSTALL_EXEC} install-py"
      BINDING_UNINSTALL="${BINDING_UNINSTALL} uninstall-py"
      BINDING_LIB_TARGETS="${BINDING_LIB_TARGETS} \$(python_ltlib)"])

AM_CONDITIONAL([ENABLE_PYTHON], [test x"${cf_with_python_binding}" = "xyes"])


## ----------------- ##
## Optional backends ##
## ----------------- ##
dnl Backend list

# Winradio only under Linux (until someone ports it on other os)
AC_MSG_CHECKING([whether to build winradio backend])
AC_ARG_ENABLE([winradio],
              [AS_HELP_STRING([--disable-winradio],
                              [do not build winradio backend @<:@default=yes@:>@])],
              [cf_with_winradio="no"],
              [cf_with_winradio="yes"])
AC_MSG_RESULT([$cf_with_winradio])

AS_IF([test x"${cf_with_winradio}" = "xyes"],
      [BACKEND_LIST="$BACKEND_LIST winradio"])


# stuff that requires C++ support
AS_IF([test x"${cf_with_cxx}" = "xyes"],[

      AC_MSG_CHECKING([whether to build USRP backend])
      AC_ARG_ENABLE([usrp],
                    [AS_HELP_STRING([--enable-usrp],
                                    [build USRP backend @<:@default=no@:>@])],
                    [cf_with_usrp=$enableval],
                    [cf_with_usrp="no"])
      AC_MSG_RESULT([$cf_with_usrp])

      AS_IF([test x"${cf_with_usrp}" = "xyes"],[
            PKG_CHECK_MODULES([USRP],
                              [usrp >= 0.8],
                              [AC_DEFINE([HAVE_USRP],[1],[Define if usrp is available])
                              ])
            ])
      ])
AM_CONDITIONAL([ENABLE_CXX], [test x"${cf_with_cxx}" = "xyes"])
AM_CONDITIONAL([HAVE_USRP], [test x"${cf_with_usrp}" = "xyes"])


PKG_CHECK_MODULES([LIBUSB], [libusb >= 0.1], [],
                  [AC_MSG_WARN([libusb pkg-config not found, USB backends will be disabled])])

CFLAGS="${CFLAGS} ${LIBUSB_CFLAGS}"
CXXFLAGS="${CXXFLAGS} ${LIBUSB_CFLAGS}"

AC_CHECK_HEADERS([usb.h])
AC_MSG_CHECKING([whether to build USB backends])

AS_IF([test x"${LIBUSB_LIBS}" = x],
      [cf_with_libusb="no"],
      [cf_with_libusb="yes"],
      [AC_DEFINE([HAVE_LIBUSB],[1],[Define if libusb is available])
      ])
AC_MSG_RESULT([$cf_with_libusb])


## -------------------------------- ##
## Prepare rig backend dependencies ##
## -------------------------------- ##

# otherwise parallel 'make -jn' will fail

for be in ${BACKEND_LIST} ; do
        BACKENDEPS="${BACKENDEPS} \$(top_builddir)/${be}/hamlib-${be}.la"
done

# dlopen force or preopen self for static version ?
BACKENDLNK="-dlopen force"
for be in ${BACKEND_LIST} ; do
        BACKENDLNK="${BACKENDLNK} -dlopen \$(top_builddir)/${be}/hamlib-${be}.la"
done
AC_SUBST([BACKEND_LIST])
AC_SUBST([BACKENDLNK])
AC_SUBST([BACKENDEPS])


## ---------------------------------- ##
## Prepare rotor backend dependencies ##
## ---------------------------------- ##

# otherwise parallel 'make -jn' will fail

for be in ${ROT_BACKEND_LIST} ; do
        ROT_BACKENDEPS="${ROT_BACKENDEPS} \$(top_builddir)/${be}/hamlib-${be}.la"
done

# dlopen force or preopen self for static version ?
ROT_BACKENDLNK="-dlopen force"
for be in ${ROT_BACKEND_LIST} ; do
        ROT_BACKENDLNK="${ROT_BACKENDLNK} -dlopen \$(top_builddir)/${be}/hamlib-${be}.la"
done
AC_SUBST([ROT_BACKEND_LIST])
AC_SUBST([ROT_BACKENDLNK])
AC_SUBST([ROT_BACKENDEPS])


AC_CHECK_PROG([cf_with_bindings], [swig], [yes], [no], [$PATH])
AS_IF([test x"${cf_with_bindings}" = "xyes"], [
      cf_with_bindings=no

      SWIG_PROG(1.3.22)
      AS_IF([test "${SWIG}" != "false" -a "x${BINDING_ALL}" != "x"], [
            BINDINGS="${BINDINGS} bindings"
            cf_with_bindings=yes])
      ])

AC_MSG_CHECKING([whether to build bindings])
AC_MSG_RESULT([$cf_with_bindings])
AC_SUBST([BINDINGS])
AC_SUBST([BINDING_ALL])
AC_SUBST([BINDING_CHECK])
AC_SUBST([BINDING_CLEAN])
AC_SUBST([BINDING_DISTCLEAN])
AC_SUBST([BINDING_DISTCHECK])
AC_SUBST([BINDING_INSTALL_EXEC])
AC_SUBST([BINDING_UNINSTALL])
AC_SUBST([BINDING_LIST])
AC_SUBST([BINDING_LIB_TARGETS])

AC_SUBST([AM_CPPFLAGS])

AC_CONFIG_FILES([Makefile
macros/Makefile
include/Makefile
lib/Makefile
dummy/Makefile
yaesu/Makefile
icom/Makefile
aor/Makefile
kenwood/Makefile
winradio/Makefile
pcr/Makefile
alinco/Makefile
uniden/Makefile
tentec/Makefile
kachina/Makefile
jrc/Makefile
drake/Makefile
lowe/Makefile
rft/Makefile
rs/Makefile
kit/Makefile
tapr/Makefile
skanti/Makefile
prm80/Makefile
wj/Makefile
racal/Makefile
tuner/Makefile
ars/Makefile
easycomm/Makefile
fodtrack/Makefile
gs232a/Makefile
heathkit/Makefile
spid/Makefile
sartek/Makefile
src/Makefile
c++/Makefile
bindings/Makefile
bindings/hamlibvb.bas
tests/Makefile
doc/Makefile
doc/hamlib.cfg
rotorez/Makefile
flexradio/Makefile
m2/Makefile
amsat/Makefile
adat/Makefile
ts7400/Makefile
celestron/Makefile
scripts/Makefile
hamlib.pc])

AC_OUTPUT

echo \
"----------------------------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION} configuration:

 Prefix         ${prefix}
 Compiler       ${CC} ${CFLAGS} ${CPPFLAGS}


 Package features:
        With C++ binding            ${cf_with_cxx_binding}
        With Perl binding           ${cf_with_perl_binding}
        With Python binding         ${cf_with_python_binding}
        With TCL binding            ${build_tcl}
        With rigmem XML support     ${cf_with_xml_support}
        With included ltdl          ${with_included_ltdl}

        Enable Rig Matrix           ${enable_rigmatrix}
        Enable WinRadio             ${cf_with_winradio}
        Enable USRP                 ${cf_with_usrp}
        Enable USB backends         ${cf_with_libusb}
        Enable shared libs          ${enable_shared}
        Enable static libs          ${enable_static}

-----------------------------------------------------------------------"
