INCLUDES = @INCLUDES@ -I@TCL_SRC_DIR@/generic

lib_LTLIBRARIES = libhamlibtcl.la
libhamlibtcl_la_SOURCES = hamlibtcl.c tclrig.c tclrot.c
libhamlibtcl_la_LDFLAGS = -no-undefined -module -version-info 0:0:0 \
							@TCL_LIB_SPEC@
libhamlibtcl_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = tclrig.h tclrot.h

noinst_SCRIPTS = tcltest.tcl
EXTRA_DIST = tcltest.tcl
