## Process this file with automake to produce Makefile.in

aclocaldir = $(datadir)/aclocal
aclocal_DATA = hamlib.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hamlib.pc

EXTRA_DIST = PLAN TODO LICENSE hamlib.m4 hamlib.pc.in README.developer \
	README.betatester README.win32 hamlib.spec

# BINDINGS_LIST subdirs are no longer built
SUBDIRS = macros include lib src $(subdirs) @BACKEND_LIST@ @ROT_BACKEND_LIST@ \
			@BINDINGS@ tests doc

# tcl, perl, and kylix subdirs are no longer distributed
DIST_SUBDIRS = macros include lib $(subdirs) src c++ bindings tests doc \
	icom kenwood aor yaesu dummy pcr alinco uniden tentec kachina jrc \
	rpcrig winradio easycomm fodtrack rpcrot drake rotorez \
	flexradio sartek lowe rft rs tapr kit skanti wj racal tuner \
	gs232a heathkit spid

rpm: Makefile
	make dist
	$(RPMBUILD) -ta $(PACKAGE)-$(VERSION).tar.gz
	rm $(PACKAGE)-$(VERSION).tar.gz

ACLOCAL_AMFLAGS = -I macros
