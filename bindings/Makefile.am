
# Hamlib bindings using SWIG
# more information on swig at http://www.swig.org
#

#INCLUDES = @INCLUDES@ -Dbool=char -I@PERL_INC_DIR@ @TCL_INCLUDE_SPEC@ -I$(top_srcdir)/bindings
INCLUDES = @INCLUDES@ -I$(top_srcdir)/bindings @TCL_INCLUDE_SPEC@ @PYTHON_CPPFLAGS@

SWGDEP=$(top_srcdir)/include/hamlib/rig.h $(top_srcdir)/include/hamlib/riglist.h \
		$(top_srcdir)/include/hamlib/rotator.h $(top_srcdir)/include/hamlib/rotlist.h

##########################################
# Perl binding

hamlibperl_wrap.c: hamlib.swg $(SWIGDEP)
	$(SWIG) -perl5 -shadow @INCLUDES@ -I$(top_srcdir)/bindings -o $@ \
			`test -f hamlib.swg || echo '$(srcdir)/'`hamlib.swg

Hamlib-pl.mk: $(srcdir)/Makefile.PL
	test -f Makefile.PL || $(LN_S) $(srcdir)/Makefile.PL Makefile.PL
	perl `test -f Makefile.PL || echo '$(srcdir)/'`Makefile.PL \
			MAKEFILE=Hamlib-pl.mk \
			PREFIX="$(prefix)" \
			INC="$(INCLUDES)"  \
			CC="$(CC)"  \
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

uninstall-perl: Hamlib-pl.mk
	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk uninstall


##########################################
# Tcl binding

tcl_ltlib = hamlibtcl.la

tcldir = $(prefix)/lib/tcl

PKG_VER=1.0
DLL=hamlibtcl-$(PKG_VER)@TCL_SHLIB_SUFFIX@

nodist_hamlibtcl_la_SOURCES = hamlibtcl_wrap.c
hamlibtcl_la_LDFLAGS = -no-undefined -module -release $(PKG_VER) -avoid-version @TCL_LIB_SPEC@
hamlibtcl_la_LIBADD = $(top_builddir)/src/libhamlib.la

hamlibtcl_ladir = $(tcldir)
hamlibtcl_la_DATA = pkgIndex.tcl

pkgIndex.tcl: Makefile
	echo 'package ifneeded Hamlib $(PKG_VER) [list load [file join $$dir $(DLL)] Hamlib]' > pkgIndex.tcl

hamlibtcl_wrap.c: hamlib.swg $(SWGDEP)
	$(SWIG) -tcl -pkgversion $(PKG_VER) @INCLUDES@ -I$(top_srcdir)/bindings -o $@ \
		`test -f hamlib.swg || echo '$(srcdir)/'`hamlib.swg

all-tcl: pkgIndex.tcl $(tcl_ltlib)

check-tcl: all-tcl
	TCLLIBPATH=$(builddir)/.libs $(srcdir)/tcltest.tcl || echo "Tcl test failed" 1>&2

install-tcl: 
clean-tcl:
distclean-tcl: clean-tcl
uninstall-tcl: 

##########################################
# Python binding

python_ltlib = _Hamlib.la

pythondir = $(prefix)/lib/python

nodist__Hamlib_la_SOURCES = hamlibpy_wrap.c
_Hamlib_la_LDFLAGS = -no-undefined -module -avoid-version
_Hamlib_la_LIBADD = $(top_builddir)/src/libhamlib.la

_Hamlib_ladir = $(pythondir)

Hamlib.py: hamlibpy_wrap.c
hamlibpy_wrap.c: hamlib.swg $(SWGDEP)
	$(SWIG) -python @INCLUDES@ -I$(top_srcdir)/bindings -o $@ \
		`test -f hamlib.swg || echo '$(srcdir)/'`hamlib.swg

all-py: $(python_ltlib)

check-py: all-py
	$(srcdir)/pytest.py || echo "Python test failed" 1>&2

python_PYTHON = Hamlib.py

install-py:
clean-py:
distclean-py:
uninstall-py: 

##########################################

lib_LTLIBRARIES = $(tcl_ltlib) $(python_ltlib)

all-local: all-perl all-tcl all-py

check-local: check-perl check-tcl check-py
clean-local: clean-perl clean-tcl clean-py
distclean-local: distclean-perl distclean-tcl distclean-py
distcheck-local: distcheck-perl
install-exec-local: install-perl install-tcl install-py
uninstall-local: uninstall-perl uninstall-tcl uninstall-py


rig.swg: $(top_srcdir)/include/hamlib/rig.h $(top_srcdir)/include/hamlib/riglist.h
rotator.swg: $(top_srcdir)/include/hamlib/rotator.h $(top_srcdir)/include/hamlib/rotlist.h

hamlib.swg: ignore.swg rig.swg rotator.swg


EXTRA_DIST = hamlib.swg ignore.swg rig.swg rotator.swg \
			Makefile.PL perltest.pl tcltest.tcl pytest.py

BUILT_SOURCES  = hamlibperl_wrap.c hamlibtcl_wrap.c hamlibpy_wrap.c Hamlib.py

DISTCLEANFILES = Hamlib-pl.mk Hamlib-pl.mk.old

MOSTLYCLEANFILES = hamlibperl_wrap.c Hamlib.pm Hamlib.bs \
		hamlibtcl_wrap.c pkgIndex.tcl \
		hamlibpy_wrap.c Hamlib.py *.pyc

noinst_SCRIPTS = perltest.pl tcltest.tcl pytest.py
