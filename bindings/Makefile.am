
# Hamlib bindings using SWIG
# more information on swig at http://www.swig.org
#

AM_CPPFLAGS = @AM_CPPFLAGS@ -I$(top_srcdir)/include -I$(top_srcdir)/bindings  \
	$(TCL_INCLUDE_SPEC) $(PYTHON_CPPFLAGS)

AM_CFLAGS = @AM_CPPFLAGS@ -fno-strict-aliasing
AM_CXXFLAGS = -O2

SWGFILES = hamlib.swg ignore.swg rig.swg rotator.swg amplifier.swg

SWGDEP = \
	$(top_srcdir)/include/hamlib/rig.h \
	$(top_srcdir)/include/hamlib/riglist.h \
	$(top_srcdir)/include/hamlib/rotator.h \
	$(top_srcdir)/include/hamlib/rotlist.h \
	$(top_srcdir)/include/hamlib/amplifier.h \
	$(top_srcdir)/include/hamlib/amplist.h \
	$(SWGFILES)

EXTRA_DIST = $(SWGFILES) \
	Makefile.PL perltest.pl tcltest.tcl.in pytest.py py3test.py \
	luatest.lua README.python

exampledir = $(docdir)/examples
example_DATA =

BUILT_SOURCES =
MOSTLYCLEANFILES =


if ENABLE_PERL
##########################################
# Perl binding

BUILT_SOURCES += hamlibperl_wrap.c
MOSTLYCLEANFILES += hamlibperl_wrap.c Hamlib.pm Hamlib.bs Hamlib-pl.mk.old
example_DATA += perltest.pl

hamlibperl_wrap.c: hamlib.swg $(SWIGDEP)
	$(AM_V_GEN)$(SWIG) -perl5 -shadow $(AM_CPPFLAGS) -I$(top_srcdir)/bindings \
		-o $@ $$(test -f hamlib.swg || echo '$(srcdir)/')hamlib.swg

Hamlib-pl.mk: $(srcdir)/Makefile.PL
	$(AM_V_at)test -f Makefile.PL || $(LN_S) $(srcdir)/Makefile.PL Makefile.PL
	$(AM_V_at)perl $$(test -f Makefile.PL || echo '$(srcdir)/')Makefile.PL \
			MAKEFILE="Hamlib-pl.mk" \
			PREFIX="$(prefix)" \
			INC="$(AM_CPPFLAGS)"  \
			CC="$(CC)"  \
			OBJECT="hamlibperl_wrap.o" \
			VERSION="$(PACKAGE_VERSION)" \
			LIBS="-L$(top_builddir)/src/.libs -lhamlib"
	$(AM_V_at)sed -e 's/^LD_RUN_PATH.*/LD_RUN_PATH =/' Hamlib-pl.mk > Hamlib-pl-norpath.mk
	$(AM_V_at)mv Hamlib-pl-norpath.mk Hamlib-pl.mk

all-perl: Hamlib-pl.mk hamlibperl_wrap.c
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk all

check-perl: all-perl
	$(AM_V_at)LD_LIBRARY_PATH=$(top_builddir)/src/.libs perl $(top_srcdir)/bindings/perltest.pl

clean-perl: Hamlib-pl.mk
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk clean

## 'distclean' target is a NOOP in Hamlib-pl.mk
distclean-perl: ## Hamlib-pl.mk
##	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk distclean

distcheck-perl: Hamlib-pl.mk
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk distcheck

install-perl: Hamlib-pl.mk
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk install_site

## ExtUtils::MakeMaker says uninstall is deprecated and won't remove the files
## anyway, so we'll have to figure out another way to remove the installed files.
uninstall-perl: ## Hamlib-pl.mk
##	$(MAKE) $(AM_MAKEFLAGS) -f Hamlib-pl.mk uninstall

endif
# Perl


if ENABLE_PYTHON
##########################################
# Python binding

pyexec_ltlib = _Hamlib.la

MOSTLYCLEANFILES += hamlibpy_wrap.c Hamlib.py *.pyc
BUILT_SOURCES += hamlibpy_wrap.c Hamlib.py

# Set by AM_CONDITIONAL in configure.ac
if PYVER_3
pytest = py3test.py
else !PYVER_3
pytest = pytest.py
endif
# PYVER_3

example_DATA += $(pytest)

nodist_pyexec_PYTHON = Hamlib.py
nodist__Hamlib_la_SOURCES = hamlibpy_wrap.c

_Hamlib_la_LDFLAGS = -no-undefined -module -avoid-version
_Hamlib_la_LIBADD = $(top_builddir)/src/libhamlib.la $(PYTHON_LIBS)
_Hamlib_ladir = $(pyexecdir)
_Hamlib_la_LTLIBRARIES = $(pyexec_ltlib)

all-py: $(pyexec_ltlib)

check-py: all-py
	$(AM_V_at)PYTHONPATH=$(builddir):$(builddir)/.libs \
		$(PYTHON) $(srcdir)/$(pytest) \
		|| echo "Python test failed" 1>&2

Hamlib.py: hamlibpy_wrap.c

hamlibpy_wrap.c: hamlib.swg $(SWGDEP)
	$(AM_V_GEN)$(SWIG) -python $(AM_CPPFLAGS) $(PYTHON_CPPFLAGS) -I$(top_srcdir)/bindings \
		-o $@ $$(test -f hamlib.swg || echo '$(srcdir)/')hamlib.swg

install-py:
clean-py:
distclean-py:
uninstall-py:

endif
# Python


if ENABLE_TCL
##########################################
# Tcl binding

BUILT_SOURCES += hamlibtcl_wrap.c
MOSTLYCLEANFILES += hamlibtcl_wrap.c pkgIndex.tcl tcltest.tcl
example_DATA += tcltest.tcl

tcl_ltlib = hamlibtcl.la

tcldir = $(libdir)/tcl$(TCL_VERSION)/Hamlib

PKG_VER = $(ABI_VERSION).$(ABI_REVISION)
DLL = hamlibtcl-$(PKG_VER)$(TCL_SHLIB_SUFFIX)

nodist_hamlibtcl_la_SOURCES = hamlibtcl_wrap.c
hamlibtcl_la_CFLAGS = $(TCL_CFLAGS)
hamlibtcl_la_LDFLAGS = -no-undefined -module -release $(PKG_VER) -avoid-version
hamlibtcl_la_LIBADD = $(top_builddir)/src/libhamlib.la $(TCL_LIB_SPEC) $(TCL_LIBS)

hamlibtcl_ladir = $(tcldir)
hamlibtcl_la_DATA = pkgIndex.tcl

# Install hamlibtcl.la into the $(tcldir)/Hamlib directory
# set by $(hamlibtcl_ladir)
hamlibtcl_la_LTLIBRARIES = $(tcl_ltlib)

# Having the first occurrence of 'hamlib' capitalized seemed to cause confusion
# for the TCL interpreter loading the Hamlib module.
pkgIndex.tcl: Makefile
	$(AM_V_at)echo 'package ifneeded hamlib $(PKG_VER) [list load [file join $$dir $(DLL)] Hamlib]' > pkgIndex.tcl

hamlibtcl_wrap.c: hamlib.swg $(SWGDEP)
	$(AM_V_GEN)$(SWIG) -tcl -pkgversion $(PKG_VER) $(AM_CPPFLAGS) \
		-I$(top_srcdir)/bindings \
		-o $@ $$(test -f hamlib.swg || echo '$(srcdir)/')hamlib.swg

do_subst = sed -e 's,[@]tcldir[@],$(tcldir),g'

tcltest.tcl: tcltest.tcl.in Makefile
	$(AM_V_GEN)$(do_subst) < $(srcdir)/tcltest.tcl.in > tcltest.tcl

all-tcl: pkgIndex.tcl $(tcl_ltlib) tcltest.tcl

check-tcl: all-tcl
	$(AM_V_at)cp $(builddir)/pkgIndex.tcl $(builddir)/.libs
	$(AM_V_at)TCLLIBPATH=$(builddir)/.libs tclsh $(builddir)/tcltest.tcl \
		|| echo "Tcl test failed" 1>&2
	$(AM_V_at)rm -f $(builddir)/.libs/pkgIndex.tcl

install-tcl:
clean-tcl:
distclean-tcl: clean-tcl
uninstall-tcl:

endif
# TCL

if ENABLE_LUA
##########################################
# Lua binding

luaexec_ltlib = Hamliblua.la

MOSTLYCLEANFILES += hamliblua_wrap.c
BUILT_SOURCES += hamliblua_wrap.c
example_DATA += luatest.lua

nodist_luaexec_LUA = Hamlib.lua
nodist_Hamliblua_la_SOURCES = hamliblua_wrap.c

Hamliblua_la_CPPFLAGS = $(LUA_INCLUDE) -I$(top_srcdir)/include -I$(top_srcdir)/src
Hamliblua_la_LDFLAGS = -no-undefined -module -avoid-version
Hamliblua_la_LIBADD = $(top_builddir)/src/libhamlib.la
Hamliblua_ladir = $(luaexecdir)
Hamliblua_la_LTLIBRARIES = $(luaexec_ltlib)

all-lua: $(luaexec_ltlib)

check-lua: all-lua
	$(AM_V_at)LUA_CPATH="$(abs_builddir)/.libs/?.so" $(LUA) $(srcdir)/luatest.lua \
		|| echo "Lua test failed" 1>&2

Hamlib.lua: hamliblua_wrap.c

hamliblua_wrap.c: hamlib.swg $(SWGDEP)
	$(AM_V_GEN)$(SWIG) -lua $(AM_CPPFLAGS) -I$(top_srcdir)/bindings \
		-o $@ $$(test -f hamlib.swg || echo '$(srcdir)/')hamlib.swg

install-lua:
clean-lua:
distclean-lua:
uninstall-lua:

endif
# Lua


all-local: @BINDING_ALL@

check-local: @BINDING_CHECK@
clean-local: @BINDING_CLEAN@
distclean-local: @BINDING_DISTCLEAN@
distcheck-local: @BINDING_DISTCHECK@
install-exec-local: @BINDING_INSTALL_EXEC@
uninstall-local: @BINDING_UNINSTALL@
