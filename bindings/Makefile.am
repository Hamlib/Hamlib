
# Hamlib bindings using SWIG
# more information on swig at http://www.swig.org
#
# note:
#  - VPATH building does not work yet
#  - Tcl install procedure is not complete yet

SWIG=swig

#INCLUDES = @INCLUDES@ -Dbool=char -I@PERL_INC_DIR@ @TCL_INCLUDE_SPEC@ -I$(top_srcdir)/bindings
INCLUDES = @INCLUDES@ -I$(top_srcdir)/bindings @TCL_INCLUDE_SPEC@

SWGDEP=$(top_srcdir)/include/hamlib/rig.h $(top_srcdir)/include/hamlib/riglist.h \
		$(top_srcdir)/include/hamlib/rotator.h $(top_srcdir)/include/hamlib/rotlist.h

##########################################
# Perl binding

hamlibperl_wrap.c: hamlib.swg $(SWIGDEP)
	$(SWIG) -perl5 -shadow @INCLUDES@ -I$(top_srcdir)/bindings -o $@ \
			`test -f hamlib.swg || echo '$(srcdir)/'`hamlib.swg

# Add  LIB="$(libdir)"  to install in private place
Hamlib-pl.mk: $(srcdir)/Makefile.PL
	test -f Makefile.PL || $(LN_S) $(srcdir)/Makefile.PL Makefile.PL
	perl `test -f Makefile.PL || echo '$(srcdir)/'`Makefile.PL \
			MAKEFILE=Hamlib-pl.mk INST_MAN3DIR="$(mandir)" \
			INC="$(INCLUDES)"  \
			OBJECT="hamlibperl_wrap.o" VERSION="$(PACKAGE_VERSION)" \
			LIBS="-L$(top_builddir)/src/.libs -lhamlib"

all-perl: Hamlib-pl.mk hamlibperl_wrap.c
	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk all

check-perl: all-perl
	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk test

clean-perl: Hamlib-pl.mk
	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk clean

distclean-perl: Hamlib-pl.mk
	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk distclean

distcheck-perl: Hamlib-pl.mk
	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk distcheck

install-perl: Hamlib-pl.mk
	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk install


##########################################
# Tcl binding

lib_LTLIBRARIES = hamlibtcl.la

PKG_VER=1.0
DLL=hamlibtcl-$(PKG_VER)@TCL_SHLIB_SUFFIX@

nodist_hamlibtcl_la_SOURCES = hamlibtcl_wrap.c
hamlibtcl_la_LDFLAGS = -no-undefined -module -release $(PKG_VER) -avoid-version @TCL_LIB_SPEC@
hamlibtcl_la_LIBADD = $(top_builddir)/src/libhamlib.la

pkgIndex.tcl: Makefile
	echo 'package ifneeded Hamlib $(PKG_VER) [list load [file join $$dir $(DLL)] Hamlib]' > pkgIndex.tcl

hamlibtcl_wrap.c: hamlib.swg $(SWGDEP)
	$(SWIG) -tcl -pkgversion $(PKG_VER) @INCLUDES@ -I$(top_srcdir)/bindings -o $@ \
		`test -f hamlib.swg || echo '$(srcdir)/'`hamlib.swg

all-tcl: pkgIndex.tcl #$(DLL)


PACKAGEDIR      = $(TCL_EXEC_PREFIX)/lib/$(PACKAGE)
libdir = $(PACKAGEDIR)

install-tcl: all-tcl
#	if test ! -d $(PACKAGEDIR); then mkdir $(PACKAGEDIR); fi
#	$(INSTALL_PROGRAM) $(DLL) $(PACKAGEDIR)/$(DLL)
#	$(INSTALL_DATA) pkgIndex.tcl $(PACKAGEDIR)/pkgIndex.tcl
#	$(INSTALL_DATA) $(MANN) $(MAN_INSTALL_DIR)/

check-tcl: all-tcl
	TCLLIBPATH=$(builddir)/.libs $(srcdir)/tcltest.tcl || echo "Tcl test failed" 1>&2

clean-tcl:
	rm -f pkgIndex.tcl $(DLL)
distclean-tcl: clean-tcl

##########################################

all-local: all-perl all-tcl

check-local: check-perl check-tcl
clean-local: clean-perl clean-tcl
distclean-local: distclean-perl distclean-tcl
distcheck-local: distcheck-perl
install-exec-local: install-perl install-tcl



rig.swg: $(top_srcdir)/include/hamlib/rig.h $(top_srcdir)/include/hamlib/riglist.h
rotator.swg: $(top_srcdir)/include/hamlib/rotator.h $(top_srcdir)/include/hamlib/rotlist.h

hamlib.swg: ignore.swg rig.swg rotator.swg


EXTRA_DIST = hamlib.swg ignore.swg rig.swg rotator.swg \
			Makefile.PL perltest.pl tcltest.tcl

BUILT_SOURCES  = hamlibperl_wrap.c hamlibtcl_wrap.c

DISTCLEANFILES = Hamlib-pl.mk Hamlib-pl.mk.old

CLEANFILES = hamlibperl_wrap.c Hamlib.pm Hamlib.bs \
		hamlibtcl_wrap.c pkgIndex.tcl

noinst_SCRIPTS = perltest.pl tcltest.tcl
