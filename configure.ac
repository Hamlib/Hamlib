## Process this file with autoconf to create configure. -*- autoconf -*-

dnl Autoconf 2.67 is in Debian Squeeze--is an older version needed
dnl for some other distribution?
## FIXME: Is this too new? ##
AC_PREREQ([2.69])


## ------------------------ ##
## Autoconf initialisation. ##
## ------------------------ ##

dnl Please do not use '-' in the version number, as package managers will fail,
dnl however, the use of '~' should be fine as apt (others?) will treat
dnl it as an earlier version than the actual release.  TNX KA6MAL
dnl PACKAGE_NAME + " " + PACKAGE_VERSION must not exceed 20 chars!
AC_INIT([Hamlib],[4.5.4],[hamlib-developer@lists.sourceforge.net],[hamlib],[http://www.hamlib.org])

AC_CONFIG_SRCDIR([include/hamlib/rig.h])
AC_CONFIG_MACRO_DIR([macros])
AC_CONFIG_HEADERS([include/hamlib/config.h])

dnl Place build system provided programs in this directory.
AC_CONFIG_AUX_DIR([build-aux])

AC_USE_SYSTEM_EXTENSIONS


## ------------------------ ##
## Automake Initialisation. ##
## ------------------------ ##

dnl Passing AC_PACKAGE_TARNAME, AC_PACKAGE_VERSION to AM_INIT_AUTOMAKE is
dnl obsolete as these values are obtained from the AC_INIT macro.
AM_INIT_AUTOMAKE([-Wall subdir-objects])

dnl Clean compilation output makes compiler warnings more visible
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


## ------------------------------ ##
## Hamlib specific configuration. ##
## ------------------------------ ##

dnl New backends must be listed here!  Also the new Makefile path must be
dnl added to AC_CONFIG_FILES near the end of this file.  See README.developer
dnl Beware of duplication should a backend directory include both rig and
dnl rotor definitions, e.g. "dummy".  Optional backends will not be listed
dnl here but will be added later, e.g. "winradio".
RIG_BACKEND_LIST="rigs/adat rigs/alinco rigs/aor rigs/barrett rigs/codan rigs/dorji rigs/drake rigs/dummy rigs/elad rigs/flexradio rigs/icom rigs/icmarine rigs/jrc rigs/kachina rigs/kenwood rigs/kit rigs/lowe rigs/pcr rigs/prm80 rigs/racal rigs/rft rigs/rs rigs/skanti rigs/tapr rigs/tentec rigs/tuner rigs/uniden rigs/winradio rigs/wj rigs/yaesu rigs/gomspace"
ROT_BACKEND_LIST="rotators/amsat rotators/ars rotators/celestron rotators/cnctrk rotators/grbltrk rotators/easycomm rotators/ether6 rotators/fodtrack rotators/gs232a rotators/heathkit rotators/m2 rotators/meade rotators/rotorez rotators/sartek rotators/spid rotators/ts7400 rotators/prosistel rotators/ioptron rotators/satel rotators/radant"
# Amplifiers are all in the amplifiers directory
AMP_BACKEND_LIST="amplifiers/elecraft amplifiers/gemini"

dnl See README.release on setting these values
# Values given to -version-info when linking.  See libtool documentation.
# Set them here to keep c++/Makefile and src/Makefile in sync.
ABI_VERSION=4
ABI_REVISION=6
ABI_AGE=0

AC_DEFINE_UNQUOTED([ABI_VERSION], [$ABI_VERSION], [Frontend ABI version])
AC_DEFINE_UNQUOTED([ABI_REVISION], [$ABI_REVISION], [Frontend ABI revision])
AC_DEFINE_UNQUOTED([ABI_AGE], [$ABI_AGE], [Frontend ABI age])

AC_SUBST([ABI_VERSION])
AC_SUBST([ABI_REVISION])
AC_SUBST([ABI_AGE])

# Add Hamlib define needed to link DLLs
AM_CPPFLAGS="${AM_CPPFLAGS} -DIN_HAMLIB"

# Add Hamlib header include paths
AM_CPPFLAGS="${AM_CPPFLAGS} -I\$(top_srcdir)/include -I\$(top_srcdir)/src -I\$(top_srcdir)/lib -I\$(top_builddir)/include"


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

# TODO: check whether CXX is functional
AC_CHECK_PROG([cf_with_cxx], ["${CXX}"], [yes], [no])

AM_CONDITIONAL([ENABLE_CXX], [test x"${cf_with_cxx}" = "xyes"])

dnl Broke on older Automake, so test for its existence.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])


## ----------------------------------- ##
## Miscellaneous configuration macros  ##
## ----------------------------------- ##

AC_C_INLINE

dnl Set warnings for compilation
dnl CFLAGS and CXXFLAGS are reserved for user, so set their AM_* counterparts.
# macros/ax_cflags_warn_all.m4
AX_CFLAGS_WARN_ALL([AM_CFLAGS])
AX_CXXFLAGS_WARN_ALL([AM_CXXFLAGS])

dnl Automake macro for rigmem compilation
AM_PROG_CC_C_O


## ------------------------------------------ ##
## Various parts of Hamlib need these headers ##
## ------------------------------------------ ##

AC_CHECK_HEADERS([errno.h fcntl.h getopt.h limits.h locale.h malloc.h \
netdb.h sgtty.h stddef.h termio.h termios.h values.h \
arpa/inet.h dev/ppbus/ppbconf.hdev/ppbus/ppi.h \
linux/hidraw.h linux/ioctl.h linux/parport.h linux/ppdev.h  netinet/in.h \
sys/ioccom.h sys/ioctl.h sys/param.h sys/socket.h sys/stat.h sys/time.h \
sys/select.h glob.h ])

dnl set host_os variable
AC_CANONICAL_HOST

dnl ws2tcpip.h not provided by cygwin and its test causes a configure warning.
dnl wspiapi.h provides needed freeaddrinfo function on W2k systems.
AS_IF([test "${host_os}" != "cygwin"], [
       AC_CHECK_HEADERS([ws2tcpip.h wspiapi.h])
])


## ------------------------------------ ##
## System capability and library checks ##
## ------------------------------------ ##

dnl Check for Mingw (Win32 Sleep) support
# macros/gr_pwin32.m4
GR_PWIN32

dnl If pthread support is found, this macro defines HAVE_PTHREAD and
dnl AC_SUBST's PTHREAD_LIBS, PTHREAD_CFLAGS, and PTHREAD_CC making those
dnl variables available in Makefiles.
# macros/ax_pthread.m4
AX_PTHREAD

AC_SYS_POSIX_TERMIOS()


dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_CHECK_TYPES([siginfo_t],[],[],[[#include <signal.h>]])
AC_CHECK_TYPES([sig_atomic_t],[],[],[[#include <signal.h>]])


dnl Checks for libraries.

# The following comes from SC_TCL_LINK_LIBS
# we redefine them here because we want a separate NET_LIBS var

#--------------------------------------------------------------------
# On a few very rare systems, all of the libm.a stuff is
# already in libc.a.  Set compiler flags accordingly.
#--------------------------------------------------------------------
AC_CHECK_FUNC([sin], [MATH_LIBS=""], [MATH_LIBS="-lm"])
AC_SUBST([MATH_LIBS])

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

# macros/hl_getaddrinfo.m4
HL_GETADDRINFO

LIBS=$hl_oldLibs
AC_SUBST([NET_LIBS])
## End Hamlib socket test ##


dnl Checks for library functions.
AC_CHECK_FUNCS([cfmakeraw floor getpagesize getpagesize gettimeofday inet_ntoa \
ioctl memchr memmove memset pow rint select setitimer setlocale sigaction signal \
snprintf socket sqrt strchr strdup strerror strncasecmp strrchr strstr strtol \
glob socketpair ])
AC_FUNC_ALLOCA

dnl AC_LIBOBJ replacement functions directory
AC_CONFIG_LIBOBJ_DIR([lib])

dnl Use Hamlib's termios
AC_LIBOBJ([termios])

dnl Assure libmisc is not empty
AC_LIBOBJ([dummy])


## ------------------------ ##
## libtool  Initialisation. ##
## ------------------------ ##

AR_FLAGS="cr"
LT_PREREQ([2.2.6b])

dnl N.B. LT_INIT([win32-dll]) does not work for older libtool
dnl so require new enough version above.
LT_INIT([win32-dll])


## -------------------------- ##
## Custom host configurations ##
## -------------------------- ##

dnl The host_os variable is set by the AC_CANONICAL_HOST macro above.
AS_CASE(["$host_os"],
    [freebsd*], [
	AM_CPPFLAGS="-I/usr/local/include ${AM_CPPFLAGS}"
	AM_LDFLAGS="${AM_LDFLAGS} -L/usr/local/lib"
	AC_SUBST([AM_LDFLAGS])],

    [darwin* | rhapsody*], [
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

dnl Check if C99 struct initializers are supported
AC_MSG_CHECKING([whether C99 struct/array initializers are supported])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],
		  [[struct{char a;int b;}s[8]={[3]={.b=5}};]])],
		  [AC_MSG_RESULT(yes)],
		  [AC_MSG_ERROR([""
"You need a C99 compliant C compiler that supports struct/array initializers."
"Have you considered GCC lately?."])
		  ])

dnl Check for libusb, treat LIBUSB_LIBS and LIBUSB_CFLAGS as precious variables
AC_MSG_CHECKING([whether to build USB dependent backends])
AC_ARG_WITH([libusb],
    [AS_HELP_STRING([--without-libusb],
	[disable USB dependent backends @<:@default=yes@:>@])],
	[cf_with_libusb=no],
	[cf_with_libusb=yes]
    )

AC_MSG_RESULT([$cf_with_libusb])
AM_CONDITIONAL([HAVE_LIBUSB], [test x"${cf_with_libusb}" = "xyes"])

LIBUSB=""
AC_ARG_VAR([LIBUSB_CFLAGS], [C compiler flags for libusb, overriding configure defaults])
AC_ARG_VAR([LIBUSB_LIBS], [linker flags for libusb, overriding configure check (useful for specifying static libusb-1.0.a (see INSTALL))])

AS_IF([test x"${cf_with_libusb}" = "xyes"], [
    AS_IF([test -z $LIBUSB_LIBS],
	[AC_CHECK_LIB([usb-1.0], [libusb_init], [LIBUSB_LIBS="-lusb-1.0"],
	    [AC_MSG_WARN([libusb_init was not found in libusb-1.0--USB backends will be disabled])]
        [cf_with_libusb="no"])
	], [cf_with_libusb="yes"])

    AS_IF([test x"${cf_with_libusb}" = "xyes"],
	[AC_CHECK_HEADERS([libusb.h libusb-1.0/libusb.h])
	 AC_DEFINE([HAVE_LIBUSB],
		   [1],
		   [Define if libusb-1.0 is available])
	LIBUSB="libusb-1.0"])])

# Only used in hamlib.pc.in
AC_SUBST([LIBUSB])


dnl Check if readline support in rigctl/rotctl is wanted
AC_MSG_CHECKING([whether to use readline in rigctl/rotctl])
AC_ARG_WITH([readline],
    [AS_HELP_STRING([--without-readline],
	[disable readline in rigctl/rotctl @<:@default=yes@:>@])],
	[cf_with_readline_support=no],
	[cf_with_readline_support=yes]
    )

AC_MSG_RESULT([$cf_with_readline_support])

AS_IF([test x"$cf_with_readline_support" != "xno"], [
    # macros/ax_lib_readline.m4
    AX_LIB_READLINE
    ])

AS_IF([test x"$ax_cv_lib_readline" = "xno"], [
    AC_MSG_WARN([readline support not found, using internal input handling.])
    cf_with_readline_support=no
    ])

dnl Check if INDI support in rigctl/rotctl is wanted
AC_MSG_CHECKING([whether to use INDI in rigctl/rotctl])
AC_ARG_WITH([indi],
    [AS_HELP_STRING([--without-indi],
	[disable INDI in rigctl/rotctl @<:@default=yes@:>@])],
	[cf_with_indi_support=no],
	[cf_with_indi_support=yes]
    )

AS_IF([test x"$cf_with_indi_support" != "xno"], [
    # INDI support needs a C++ compiler, tested for presence above.
    AS_IF([test x"${cf_with_cxx}" != "xyes"], [
	AC_MSG_WARN([INDI support needs a C++ compiler.])
	cf_with_indi_support=no
    ])
])

AS_IF([test x"$cf_with_indi_support" != "xno"], [
    # macros/ax_lib_nova.m4
    AX_LIB_NOVA

    AS_IF([test x"$ax_cv_lib_nova" = "xno"], [
	AC_MSG_WARN([libnova support not found, required by INDI.])
	cf_with_indi_support=no
	])

    AS_IF([test x"$ax_cv_lib_nova" != "xno"], [
	# macros/ax_lib_indi.m4
	AX_LIB_INDI

	AS_IF([test x"$ax_cv_lib_indi" = "xno"], [
	    AC_MSG_WARN([INDI support not found.])
	    cf_with_indi_support=no
	])

	AS_IF([test x"$cf_with_indi_support" != "xno"], [
	    ROT_BACKEND_LIST="$ROT_BACKEND_LIST rotators/indi"
	])
    ])
])

dnl Check if libgd-dev is installed, so we can enable rigmatrix
AC_MSG_CHECKING([whether to build HTML rig feature matrix])
AC_ARG_ENABLE([html-matrix],
    [AS_HELP_STRING([--disable-html-matrix],
	[do not generate HTML rig feature matrix (requires libgd-dev) @<:@default=check@:>@])],
	[cf_enable_html_matrix=no],
	[cf_enable_html_matrix=check]
    )

AC_MSG_RESULT([$cf_enable_html_matrix])

AS_IF([test x"$cf_enable_html_matrix" != "xno"],
    [AC_CHECK_HEADERS([gd.h],
	[AC_CHECK_LIB([gd],
	    [gdImageCreate],
	    [cf_enable_html_matrix=yes],
	    [cf_enable_html_matrix=no],
	    [-lz])],
	[cf_enable_html_matrix=no]
	)
    ])

AM_CONDITIONAL([HTML_MATRIX], [test x"${cf_enable_html_matrix}" = "xyes"])


## ------------------ ##
## PKG Config support ##
## ------------------ ##

dnl These features rely on the external pkg-config functionality being available.
dnl Test for pkg-config only when these features are enabled.

# libxml2 required rigmem xml support, make it user optional
AC_MSG_CHECKING([whether to build rigmem XML support])
AC_ARG_WITH([xml-support],
	    [AS_HELP_STRING([--with-xml-support],
			    [build rigmem with XML support @<:@default=no@:>@])],
	    [cf_with_xml_support=$withval],
	    [cf_with_xml_support=no])
AC_MSG_RESULT([$cf_with_xml_support])


# USRP needs a C++ compiler, tested for presence above.
AS_IF([test x"${cf_with_cxx}" = "xyes"], [
      AC_MSG_CHECKING([whether to build USRP backend])
      AC_ARG_ENABLE([usrp],
		    [AS_HELP_STRING([--enable-usrp],
				    [build USRP backend @<:@default=no@:>@])],
		    [cf_with_usrp=$enableval],
		    [cf_with_usrp="no"])
      AC_MSG_RESULT([$cf_with_usrp])
      ])

dnl Only check for pkg-config when either of the dependent features are enabled.

AS_IF([test x"${cf_with_xml_support}" = "xyes" || test x"${cf_with_usrp}" = "xyes"], [
dnl Check for pkg-config presence and if not installed define a dummy macro
dnl to disable libxml2 and USRP use.  Code borrowed from:
dnl http://lists.gnu.org/archive/html/automake/2011-03/msg00008.html
dnl
dnl Check for pkg-config program, used for configuring some libraries.
       m4_define_default([PKG_PROG_PKG_CONFIG],
		[AC_MSG_CHECKING([pkg-config])
		 AC_MSG_RESULT([no])])

       PKG_PROG_PKG_CONFIG

dnl If the pkg-config autoconf support isn't installed, define its
dnl autoconf macro to disable any packages depending on it.
       m4_define_default([PKG_CHECK_MODULES],
		[AC_MSG_CHECKING([$1])
		 AC_MSG_RESULT([no])
		 $4])
      ])


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
AC_MSG_CHECKING([whether to build C++ binding])
AC_ARG_WITH([cxx-binding],
	    [AS_HELP_STRING([--without-cxx-binding],
			    [do not build C++ binding @<:@default=yes@:>@])],
	    [cf_with_cxx_binding=$withval],
	    [cf_with_cxx_binding=$cf_with_cxx])
AC_MSG_RESULT([$cf_with_cxx_binding])

AS_IF([test x"${cf_with_cxx_binding}" = "xyes"],
      [BINDINGS="${BINDINGS} c++"])


dnl Check if perl-binding is wanted, default is to not build it

# Perl binding
AC_MSG_CHECKING([whether to build perl binding])
AC_ARG_WITH([perl-binding],
	    [AS_HELP_STRING([--with-perl-binding],
			    [build perl binding @<:@default=no@:>@])],
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

AM_CONDITIONAL([ENABLE_PERL], [test x"${cf_with_perl_binding}" = "xyes"])


dnl Check for python availability, so we can enable HamlibPy
# Python bindings
AC_MSG_CHECKING([whether to build python binding])
AC_ARG_WITH([python-binding],
    [AS_HELP_STRING([--with-python-binding],
	[build python binding @<:@default=no@:>@])],
    [cf_with_python_binding=$withval],
    [cf_with_python_binding=no])
AC_MSG_RESULT([$cf_with_python_binding])

dnl AX_PYTHON_DEVEL from macros/ax_python_devel.m4
AS_IF([test x"${cf_with_python_binding}" = "xyes"],[
    dnl Order matters!	AX_PYTHON_DEVEL must be first for PYTHON_VERSION to be honored.
    AX_PYTHON_DEVEL([">='2.1'"])
    AM_PATH_PYTHON([2.1],, [:])

    BINDING_LIST="${BINDING_LIST} python"
    BINDING_ALL="${BINDING_ALL} all-py"
    BINDING_CHECK="${BINDING_CHECK} check-py"
    BINDING_CLEAN="${BINDING_CLEAN} clean-py"
    BINDING_DISTCLEAN="${BINDING_DISTCLEAN} distclean-py"
    BINDING_INSTALL_EXEC="${BINDING_INSTALL_EXEC} install-py"
    BINDING_UNINSTALL="${BINDING_UNINSTALL} uninstall-py"
    BINDING_LIB_TARGETS="${BINDING_LIB_TARGETS} \$(python_ltlib)"])

AM_CONDITIONAL([ENABLE_PYTHON], [test x"${cf_with_python_binding}" = "xyes"])

dnl Determine whether to install pytest.py or py3test.py to $(docdir)/examples
AM_PYTHON_CHECK_VERSION([${PYTHON}], [3.0], [pyver_3="yes"], [pyver_3="no"])
AM_CONDITIONAL([PYVER_3], [test x"${pyver_3}" = x"yes"])

# Tcl binding
AC_MSG_CHECKING([Whether to build Tcl bindings])
AC_ARG_WITH([tcl-binding],
    [AS_HELP_STRING([--with-tcl-binding],
	[build Tcl binding @<:@default=no@:>@])],
    [build_tcl=$withval],
    [build_tcl=no])
AC_MSG_RESULT([$build_tcl])

dnl tcl.pc or SC_PATH_TCLCONFIG and SC_LOAD_TCLCONFIG from macros/tcl.m4
AS_IF([test x"${build_tcl}" = "xyes"],[
    dnl Search for and load tcl.pc or tclConfig.sh.
    PKG_CHECK_MODULES([TCL], [tcl],
        [],
        [
            AC_MSG_WARN([Unable to find Tcl pkgconfig])
            SC_PATH_TCLCONFIG
            SC_LOAD_TCLCONFIG
        ])

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
dnl These variables are set once tclConfig.sh is loaded.
AC_SUBST([TCL_VERSION])
AC_SUBST([TCL_LIB_SPEC])
AC_SUBST([TCL_INCLUDE_SPEC])
AC_SUBST([TCL_SHLIB_SUFFIX])
dnl These variables are set once tcl.pc is found.
AC_SUBST([TCL_LIBS])
AC_SUBST([TCL_CFLAGS])


dnl Check for lua availability, so we can enable HamlibLua
# Lua bindings
AC_MSG_CHECKING([whether to build lua binding])
AC_ARG_WITH([lua-binding],
    [AS_HELP_STRING([--with-lua-binding],
	[build lua binding @<:@default=no@:>@])],
    [cf_with_lua_binding=$withval],
    [cf_with_lua_binding=no])
AC_MSG_RESULT([$cf_with_lua_binding])

dnl AX_LUA_DEVEL from macros/ax_lua_devel.m4
AS_IF([test x"${cf_with_lua_binding}" = "xyes"],[

    AX_PROG_LUA([5.2], [5.5])
    AX_LUA_HEADERS
    AX_LUA_LIBS

    BINDING_LIST="${BINDING_LIST} lua"
    BINDING_ALL="${BINDING_ALL} all-lua"
    BINDING_CHECK="${BINDING_CHECK} check-lua"
    BINDING_CLEAN="${BINDING_CLEAN} clean-lua"
    BINDING_DISTCLEAN="${BINDING_DISTCLEAN} distclean-lua"
    BINDING_INSTALL_EXEC="${BINDING_INSTALL_EXEC} install-lua"
    BINDING_UNINSTALL="${BINDING_UNINSTALL} uninstall-lua"
    BINDING_LIB_TARGETS="${BINDING_LIB_TARGETS} \$(lua_ltlib)"])

AM_CONDITIONAL([ENABLE_LUA], [test x"${cf_with_lua_binding}" = "xyes"])


dnl Only search for Swig if one or more bindings are enabled.
AS_IF([test "x${BINDING_ALL}" != "x"],
    [# macros/ax_pkg_swig.m4
    AX_PKG_SWIG([3.0.12],
	[BINDINGS="${BINDINGS} bindings"
	cf_with_bindings="yes"],
	[AC_MSG_ERROR([SWIG is required to build bindings...])
	])],
    [cf_with_bindings="no"])

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


## ----------------- ##
## Optional backends ##
## ----------------- ##

dnl Winradio only under Linux (until someone ports it on other os)
AC_MSG_CHECKING([whether to build winradio backend])
AC_ARG_ENABLE([winradio],
	      [AS_HELP_STRING([--disable-winradio],
			      [do not build winradio backend @<:@default=yes@:>@])],
	      [cf_with_winradio="no"],
	      [cf_with_winradio="yes" AC_DEFINE([HAVE_WINRADIO],[1],[Define if winradio backend is built])])
AC_MSG_RESULT([$cf_with_winradio])

dnl Parallel port device disable
AC_MSG_CHECKING([whether to build parallel port devices])
AC_ARG_ENABLE([parallel],
	      [AS_HELP_STRING([--disable-parallel],
			      [do not build parallel devices @<:@default=yes@:>@])],
	      [cf_with_parallel="no"],
	      [cf_with_parallel="yes" AC_DEFINE([HAVE_PARALLEL],[1],[Define if parallel devices are to be built])])
AC_MSG_RESULT([$cf_with_parallel])

DL_LIBS=""

AS_IF([test x"${cf_with_winradio}" = "xyes"],
      [RIGS_BACKEND_LIST="$RIGS_BACKEND_LIST rigs/winradio"
dnl Check for libdl and set DL_LIBS if found--used for linradio WR-G313 backend.
       AC_CHECK_LIB([dl], [dlopen], [DL_LIBS="-ldl"],
		    [AC_MSG_WARN([dlopen was not found in libdl--linradio backend will be disabled])])
      ])

# Still need -ldl if we have it
AS_IF([test x"${cf_with_winradio}" = "xno"],
      [RIGS_BACKEND_LIST="$RIGS_BACKEND_LIST"
dnl Check for libdl and set DL_LIBS if found--used for linradio WR-G313 backend.
       AC_CHECK_LIB([dl], [dlopen], [DL_LIBS="-ldl"],
		    [AC_MSG_WARN([dlopen was not found in libdl--linradio backend will be disabled])])
      ])

dnl Set conditional compilation for G-313.
AS_CASE(["$host_os"],
	[mingw* | pw32* | cygwin ], [
	     AM_CONDITIONAL(G313_LINUX_GNU, false)
	     AM_CONDITIONAL(G313_WINDOWS, true)
	],
dnl As g313-posix.c has a hard dependency on linux/types.h
dnl via linradio/wrg313api.h, disable compilation on other POSIX.
dnl (Certain systems have extra characters following "-gnu" such as
dnl the Raspberry Pi which is "linux-gnueabihf"; presume that "linux-gnu"
dnl is a sufficient test.)
	[ *linux-gnu* ], [
	     AM_CONDITIONAL(G313_LINUX_GNU, true)
	     AM_CONDITIONAL(G313_WINDOWS, false)
	],
	[
	     AM_CONDITIONAL(G313_LINUX_GNU, false)
	     AM_CONDITIONAL(G313_WINDOWS, false)
	     AC_DEFINE([OTHER_POSIX], [1], [Compilation on POSIX other than Linux])
	])

dnl If libdl is present, check for its header file.
AS_IF([test x"${DL_LIBS}" = "x-ldl"],
      [AC_CHECK_HEADERS([dlfcn.h])])

AC_SUBST([DL_LIBS])

dnl check for c++11
AX_CXX_COMPILE_STDCXX([11],[noext],[mandatory])


dnl stuff that requires C++ support
AS_IF([test x"${cf_with_usrp}" = "xyes"],[
       PKG_CHECK_MODULES([USRP],
			 [usrp >= 0.8],
			 [AC_DEFINE([HAVE_USRP],[1],[Define if usrp is available])
			 ])
      ])

AM_CONDITIONAL([HAVE_USRP], [test x"${cf_with_usrp}" = "xyes"])

AM_CONDITIONAL(BUILD_OS_IS_UBUNTU, [test x"$build_os" = ubuntu])
AM_CONDITIONAL(BUILD_OS_IS_FEDORA, [test x"$build_os" = fedora])
echo Build_OS is $build_os



## -------------------------------- ##
## Prepare rig backend dependencies ##
## -------------------------------- ##

dnl otherwise parallel 'make -jn' will fail

## ---------------------------------- ##
## Prepare rig backend dependencies ##
## ---------------------------------- ##

for be in ${RIG_BACKEND_LIST} ; do
	RIGDIR=$(echo $be | awk -F "/" '{print $2}')
	RIG_BACKENDEPS="${RIG_BACKENDEPS} \$(top_builddir)/rigs/${RIGDIR}/libhamlib-${RIGDIR}.la"
done

AC_SUBST([RIG_BACKEND_LIST])
AC_SUBST([RIG_BACKENDEPS])


## ---------------------------------- ##
## Prepare rotor backend dependencies ##
## ---------------------------------- ##

# otherwise parallel 'make -jn' will fail

for be in ${ROT_BACKEND_LIST} ; do
	ROTDIR=$(echo $be | awk -F "/" '{print $2}')
	ROT_BACKENDEPS="${ROT_BACKENDEPS} \$(top_builddir)/rotators/${ROTDIR}/libhamlib-${ROTDIR}.la"
done

AC_SUBST([ROT_BACKEND_LIST])
AC_SUBST([ROT_BACKENDEPS])

## ---------------------------------- ##
## Prepare amplifier backend dependencies ##
## ---------------------------------- ##

# otherwise parallel 'make -jn' will fail

for be in ${AMP_BACKEND_LIST} ; do
	AMPDIR=$(echo $be | awk -F "/" '{print $2}')
	AMP_BACKENDEPS="${AMP_BACKENDEPS} \$(top_builddir)/amplifiers/${AMPDIR}/libhamlib-${AMPDIR}.la"
done

AC_SUBST([AMP_BACKEND_LIST])
AC_SUBST([AMP_BACKENDEPS])



## ------------ ##
## Final output ##
## ------------ ##

dnl Output accumulated flags to the Makefile files.
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])


AC_CONFIG_FILES([Makefile
macros/Makefile
include/Makefile
lib/Makefile
src/Makefile
security/Makefile
c++/Makefile
bindings/Makefile
doc/Makefile
doc/hamlib.cfg
rotators/amsat/Makefile
rotators/ars/Makefile
rotators/celestron/Makefile
rotators/cnctrk/Makefile
rotators/grbltrk/Makefile
rotators/easycomm/Makefile
rotators/ether6/Makefile
rotators/fodtrack/Makefile
rotators/gs232a/Makefile
rotators/heathkit/Makefile
rotators/ioptron/Makefile
rotators/m2/Makefile
rotators/meade/Makefile
rotators/prosistel/Makefile
rotators/rotorez/Makefile
rotators/sartek/Makefile
rotators/spid/Makefile
rotators/ts7400/Makefile
rotators/indi/Makefile
rotators/satel/Makefile
rotators/radant/Makefile
rigs/adat/Makefile
rigs/alinco/Makefile
rigs/aor/Makefile
rigs/barrett/Makefile
rigs/codan/Makefile
rigs/dorji/Makefile
rigs/drake/Makefile
rigs/dummy/Makefile
rigs/elad/Makefile
rigs/flexradio/Makefile
rigs/icmarine/Makefile
rigs/icom/Makefile
rigs/jrc/Makefile
rigs/kachina/Makefile
rigs/kenwood/Makefile
rigs/kit/Makefile
rigs/lowe/Makefile
rigs/pcr/Makefile
rigs/prm80/Makefile
rigs/racal/Makefile
rigs/rft/Makefile
rigs/rs/Makefile
rigs/skanti/Makefile
rigs/tapr/Makefile
rigs/tentec/Makefile
rigs/tuner/Makefile
rigs/uniden/Makefile
rigs/winradio/Makefile
rigs/wj/Makefile
rigs/yaesu/Makefile
rigs/gomspace/Makefile
tests/Makefile
scripts/Makefile
android/Makefile
amplifiers/elecraft/Makefile
amplifiers/gemini/Makefile
simulators/Makefile
hamlib.pc
])

AC_OUTPUT

echo \
"----------------------------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION} configuration:

 Prefix 	${prefix}
 Preprocessor	${CPP} ${CPPFLAGS}
 C Compiler	${CC} ${CFLAGS}
 C++ Compiler	${CXX} ${CXXFLAGS}

 Package features:
    With C++ binding		    ${cf_with_cxx_binding}
    With Perl binding		    ${cf_with_perl_binding}
    With Python binding 	    ${cf_with_python_binding}
    With TCL binding		    ${build_tcl}
    With Lua binding		    ${cf_with_lua_binding}
    With rigmem XML support	    ${cf_with_xml_support}
    With Readline support	    ${cf_with_readline_support}
    With INDI support		    ${cf_with_indi_support}

    Enable HTML rig feature matrix  ${cf_enable_html_matrix}
    Enable WinRadio		    ${cf_with_winradio}
    Enable Parallel		    ${cf_with_parallel}
    Enable USRP 		    ${cf_with_usrp}
    Enable USB backends 	    ${cf_with_libusb}
    Enable shared libs		    ${enable_shared}
    Enable static libs		    ${enable_static}

-----------------------------------------------------------------------"
