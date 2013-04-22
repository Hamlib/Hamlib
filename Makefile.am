## Process this file with automake to produce Makefile.in

aclocaldir = $(datadir)/aclocal
aclocal_DATA = hamlib.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hamlib.pc

EXTRA_DIST = PLAN TODO LICENSE hamlib.m4 hamlib.pc.in README.developer \
	README.betatester README.win32

SUBDIRS =
if BUILD_LIBLTDL
SUBDIRS += libltdl
endif
SUBDIRS += macros include lib src \
	@BACKEND_LIST@ \
	@ROT_BACKEND_LIST@ \
	@BINDINGS@ \
	tests doc

DIST_SUBDIRS = libltdl macros include lib src c++ bindings tests doc \
	icom kenwood aor yaesu dummy pcr alinco uniden tentec kachina jrc \
	winradio adat easycomm fodtrack drake rotorez \
	flexradio sartek lowe rft rs tapr kit skanti prm80 wj racal tuner \
	gs232a heathkit spid ars m2 amsat scripts ts7400 celestron

# Install any third party macros into our tree for distribution
ACLOCAL_AMFLAGS = -I macros --install
