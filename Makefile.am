
#AUTOMAKE_OPTIONS = 1.6

aclocaldir = $(datadir)/aclocal
aclocal_DATA = hamlib.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hamlib.pc

EXTRA_DIST = PLAN TODO LICENSE hamlib.m4 hamlib.pc.in README.developer README.betatester

# BINDINGS_LIST subdirs are no more built
SUBDIRS = macros include lib libltdl src @BACKEND_LIST@ @ROT_BACKEND_LIST@ \
			@BINDINGS@ tests doc

# tcl and perl subdir are no more distributed
DIST_SUBDIRS = macros include lib libltdl src c++ kylix bindings tests doc \
	icom kenwood aor yaesu dummy pcr alinco uniden tentec kachina jrc \
	rpcrig winradio easycomm rpcrot gnuradio drake

