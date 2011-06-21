## Process this file with automake to produce Makefile.in

aclocaldir = $(datadir)/aclocal
aclocal_DATA = hamlib.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hamlib.pc

EXTRA_DIST = PLAN TODO LICENSE hamlib.m4 hamlib.pc.in README.developer \
	README.betatester README.win32 hamlib.spec

# BINDINGS_LIST subdirs are no longer built
SUBDIRS = macros include lib $(subdirs) src @BACKEND_LIST@ @ROT_BACKEND_LIST@ \
			@BINDINGS@ tests doc

# tcl, perl, and kylix subdirs are no longer distributed
DIST_SUBDIRS = macros include lib $(subdirs) src c++ bindings tests doc \
	icom kenwood aor yaesu dummy pcr alinco uniden tentec kachina jrc \
	rpcrig winradio easycomm fodtrack rpcrot drake rotorez \
	flexradio sartek lowe rft rs tapr kit skanti prm80 wj racal tuner \
	gs232a heathkit spid ars m2 amsat scripts

rpm: Makefile dist
	$(RPMBUILD) -ta $(PACKAGE)-$(VERSION).tar.gz
	rm $(PACKAGE)-$(VERSION).tar.gz

ACLOCAL_AMFLAGS = -I macros

# Hack for generating the win32 binaries

.PHONY: dist-win32

distdirw32="$(distdir)-win32"
# FIXME: get libusb-win32 path from configure
libusb_win32_bin_path=$(top_builddir)/../libusb-win32-bin-1.2.4.0
msvc_toolkit_path=~/.wine/drive_c/Program\ Files/Microsoft\ Visual\ C++\ Toolkit\ 2003
zipdir=hamlib-win32-$(VERSION)

# aka unix2dos
TODOS=todos

dist-win32:
	@# Import internal ./libltdl and build it for mingw32
	mkdir -p $(distdirw32)/_build $(distdirw32)/_inst
	@# Configure and build hamlib for mingw32, with libusb-win32
	@# FIXME: Need to forge a file libusb.pc
	$(am__cd) $(distdirw32)/_build \
	&& ( test -d libltdl || ( libtoolize --ltdl && \
		$(am__cd) libltdl && ./configure --disable-static --host=i586-mingw32msvc && make ) ) \
	&& ../../$(top_srcdir)/configure --disable-static \
			--host=i586-mingw32msvc \
			--prefix=`pwd`/../_inst \
			--without-rpc-backends \
			--without-cxx-binding \
			PKG_CONFIG_LIBDIR=../../$(libusb_win32_bin_path)/lib/pkgconfig \
	&& make $(AM_MAKEFLAGS) install
	mkdir -p $(zipdir)/bin $(zipdir)/lib/msvc $(zipdir)/lib/gcc $(zipdir)/include
	cp -a $(distdirw32)/_build/src/libhamlib.def $(zipdir)/lib/msvc/libhamlib-@ABI_VERSION@.def
	cp -a $(distdirw32)/_inst/include/hamlib $(zipdir)/include/.
	$(TODOS) $(zipdir)/lib/msvc/libhamlib-@ABI_VERSION@.def $(zipdir)/include/hamlib/*.h
	@# C++ binding is pointless on win32 because of its ABI
	rm -f $(zipdir)/include/hamlib/{rig,rot}class.h
	for f in README README.betatester LICENSE COPYING.LIB README_VB ; do \
		cp -a $(top_srcdir)/$${f} $(zipdir)/$${f}.txt && $(TODOS) $(zipdir)/$${f}.txt ; done
	cp -a $(distdirw32)/_inst/bin/{libhamlib-@ABI_VERSION@.dll,rigctld.exe,rigctl.exe,rigmem.exe,rigsmtr.exe,rigswr.exe,rotctld.exe,rotctl.exe} $(zipdir)/bin/.
	cp -a $(distdirw32)/_inst/lib/hamlib/hamlib-*.dll $(zipdir)/bin/.
	cp -a $(distdirw32)/_inst/lib/libhamlib.dll.a $(zipdir)/lib/gcc
	@# NB: Do no strip libusb0.dll
	i586-mingw32msvc-strip $(zipdir)/bin/*.exe $(zipdir)/bin/*hamlib-*.dll
	cp -a ${libusb_win32_bin_path}/bin/x86/libusb0_x86.dll $(zipdir)/bin/libusb0.dll
	@# Need VC++ free toolkit installed somewhere (TODO: or dlltool is enough?)
	$(am__cd) $(zipdir)/lib/msvc/ && \
		wine $(msvc_toolkit_path)/bin/link.exe /lib /machine:i386 /def:libhamlib-@ABI_VERSION@.def || true
	zip -r hamlib-win32-$(VERSION).zip $(zipdir)
	rm -rf $(distdirw32)

