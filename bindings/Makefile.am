
# hamlibtcl, hamlibperl, etc. are temproray modules names
# They will be generated in separate subdirs in the future

# more information on swig at http://www.swig.org

#lib_LTLIBRARIES = @BINDING_LA@
#EXTRA_LTLIBRARIES = hamlibperl.la hamlibtcl.la
lib_LTLIBRARIES = hamlibperl.la hamlibtcl.la

INCLUDES = @INCLUDES@ -Dbool=char -I@PERL_INC_DIR@ -I@TCL_SRC_DIR@/generic -I$(top_srcdir)/bindings

# Perl binding
#hamlibperl_la_INCLUDE = @INCLUDES@ -Dbool=char -I@PERL_INC_DIR@

nodist_hamlibperl_la_SOURCES = hamlibperl_wrap.c
hamlibperl_la_LDFLAGS = -no-undefined -module -avoid-version
hamlibperl_la_LIBADD = ../src/libhamlib.la

BUILT_SOURCES  = hamlibperl_wrap.c hamlibtcl_wrap.c
DISTCLEANFILES = hamlibperl_wrap.c hamlibtcl_wrap.c

hamlibperl_wrap.c: hamlib.swg $(srcdir)/../include/hamlib/rig.h
	swig -perl5 -shadow @INCLUDES@ -I$(top_srcdir)/bindings -o $@ `test -f hamlib.swg || echo '$(srcdir)/'`hamlib.swg


# Tcl binding
#hamlibtcl_la_INCLUDE = @INCLUDES@ -I@TCL_SRC_DIR@/generic

nodist_hamlibtcl_la_SOURCES = hamlibtcl_wrap.c
hamlibtcl_la_LDFLAGS = -no-undefined -module -avoid-version @TCL_LIB_SPEC@
hamlibtcl_la_LIBADD = ../src/libhamlib.la

hamlibtcl_wrap.c: hamlib.swg $(srcdir)/../include/hamlib/rig.h
	swig -tcl @INCLUDES@ -I$(top_srcdir)/bindings -o $@ `test -f hamlib.swg || echo '$(srcdir)/'`hamlib.swg

rig.swg: ../include/hamlib/rig.h ../include/hamlib/riglist.h
rotator.swg: ../include/hamlib/rotator.h ../include/hamlib/rotlist.h

hamlib.swg: ignore.swg rig.swg rotator.swg

EXTRA_DIST = hamlib.swg ignore.swg rig.swg rotator.swg

CLEANFILES = hamlibperl_wrap.c hamlib.pm hamlibtcl_wrap.c

