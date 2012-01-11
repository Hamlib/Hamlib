## Process this file with automake to produce Makefile.in

aclocaldir = $(datadir)/aclocal
aclocal_DATA = hamlib.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hamlib.pc

EXTRA_DIST = PLAN TODO LICENSE hamlib.m4 hamlib.pc.in README.developer \
	README.betatester README.win32

# BINDINGS_LIST subdirs are no longer built
SUBDIRS = macros include lib $(subdirs) src @BACKEND_LIST@ @ROT_BACKEND_LIST@ \
			@BINDINGS@ tests doc

# perl and kylix subdirs are no longer distributed
DIST_SUBDIRS = macros include lib $(subdirs) src c++ bindings tests doc \
	icom kenwood aor yaesu dummy pcr alinco uniden tentec kachina jrc \
	rpcrig winradio adat easycomm fodtrack rpcrot drake rotorez \
	flexradio sartek lowe rft rs tapr kit skanti prm80 wj racal tuner \
	gs232a heathkit spid ars m2 amsat scripts ts7400 celestron

ACLOCAL_AMFLAGS = -I macros

