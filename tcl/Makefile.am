INCLUDES = @INCLUDES@ -I/usr/include/tcl8.2

LDFLAGS = -no-undefined -module -avoid-version
if HAMLIBTCL
lib_LTLIBRARIES = libhamlibtcl.la
#lib_LTLIBRARIES = @LTHAMLIBTCL@
libhamlibtcl_la_SOURCES = hamlibtcl.c
#libhamlibtcl_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlibtcl_la_LIBADD = ../src/libhamlib.la
#EXTRA_LTLIBRARIES = libhamlibtcl.la

lib_LIBRARIES = libhamlibtcl.a
#lib_LIBRARIES = @LIBHAMLIBTCL@
libhamlibtcl_a_SOURCES = hamlibtcl.c
libhamlibtcl_a_LIBADD = ../src/libhamlib.la
#EXTRA_LIBRARIES = libhamlibtcl.a
else
BUILT_SOURCES = hamlibtcl.c
endif

noinst_SCRIPTS = tcltest.tcl
EXTRA_DIST = tcltest.tcl
