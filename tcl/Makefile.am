INCLUDES = @INCLUDES@ -I/usr/include/tcl8.2

TCLSRCLIST = hamlibtcl.c tclrig.c tclrot.c
LDFLAGS = -no-undefined -module -avoid-version
if HAMLIBTCL
lib_LTLIBRARIES = libhamlibtcl.la
#lib_LTLIBRARIES = @LTHAMLIBTCL@
libhamlibtcl_la_SOURCES = $(TCLSRCLIST)
libhamlibtcl_la_LIBADD = ../src/libhamlib.la

#libhamlibtcl_la_LDFLAGS = -no-undefined -module -avoid-version
#EXTRA_LTLIBRARIES = libhamlibtcl.la

else
BUILT_SOURCES = $(TCLSRCLIST)
endif

noinst_HEADERS = tclrig.h tclrot.h

noinst_SCRIPTS = tcltest.tcl
EXTRA_DIST = tcltest.tcl
