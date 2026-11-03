## Process this file with automake to produce Makefile.in

aclocaldir = $(datadir)/aclocal
aclocal_DATA = hamlib.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hamlib.pc

EXTRA_DIST = \
	README.betatester \
	LICENSE \
	PLAN \
	README.developer \
	README.freqranges \
	README.macos \
	README.md \
	README.multicast \
	HAMLIB_STREAMING.md \
	HAMLIB_STREAMING_BACKEND_GUIDE.md \
	ReleaseNotes_4.7.md \
	ReleaseNotes_5.0.md \
	hamlib.m4 \
	hamlib.pc.in

doc_DATA = ChangeLog COPYING COPYING.LIB LICENSE \
	README.md README.betatester README.developer

SUBDIRS = macros include lib security . src $(BINDINGS) tests test doc

## Static list of distributed directories.
DIST_SUBDIRS = \
	bindings \
	c++ \
	doc \
	include \
	lib \
	macros \
	scripts \
	security \
	simulators \
	src \
	test \
	tests

# Install any third party macros into our tree for distribution
ACLOCAL_AMFLAGS = -I macros --install 

AM_CFLAGS = @AM_CFLAGS@

noinst_LTLIBRARIES =

include $(srcdir)/rigs/adat/local.mk
include $(srcdir)/rigs/alinco/local.mk
include $(srcdir)/rigs/anytone/local.mk
include $(srcdir)/rigs/aor/local.mk
include $(srcdir)/rigs/ats-mini/local.mk
include $(srcdir)/rigs/barrett/local.mk
include $(srcdir)/rigs/codan/local.mk
include $(srcdir)/rigs/commradio/local.mk
include $(srcdir)/rigs/dorji/local.mk
include $(srcdir)/rigs/drake/local.mk
include $(srcdir)/rigs/dummy/local.mk
include $(srcdir)/rigs/elad/local.mk
include $(srcdir)/rigs/flexradio/local.mk
include $(srcdir)/rigs/gomspace/local.mk
include $(srcdir)/rigs/guohetec/local.mk
include $(srcdir)/rigs/harris/local.mk
include $(srcdir)/rigs/icmarine/local.mk
include $(srcdir)/rigs/icom/local.mk
include $(srcdir)/rigs/jrc/local.mk
include $(srcdir)/rigs/kachina/local.mk
include $(srcdir)/rigs/kenwood/local.mk
include $(srcdir)/rigs/kit/local.mk
include $(srcdir)/rigs/lowe/local.mk
include $(srcdir)/rigs/mds/local.mk
include $(srcdir)/rigs/motorola/local.mk
include $(srcdir)/rigs/pcr/local.mk
include $(srcdir)/rigs/prm80/local.mk
include $(srcdir)/rigs/racal/local.mk
include $(srcdir)/rigs/rft/local.mk
include $(srcdir)/rigs/rs/local.mk
include $(srcdir)/rigs/simplecat/local.mk
include $(srcdir)/rigs/skanti/local.mk
include $(srcdir)/rigs/tapr/local.mk
include $(srcdir)/rigs/tci/local.mk
include $(srcdir)/rigs/tentec/local.mk
include $(srcdir)/rigs/tuner/local.mk
include $(srcdir)/rigs/uniden/local.mk
include $(srcdir)/rigs/winradio/local.mk
include $(srcdir)/rigs/wj/local.mk
include $(srcdir)/rigs/yaesu/local.mk
include $(srcdir)/rotators/amsat/local.mk
include $(srcdir)/rotators/androidsensor/local.mk
include $(srcdir)/rotators/apex/local.mk
include $(srcdir)/rotators/ars/local.mk
include $(srcdir)/rotators/celestron/local.mk
include $(srcdir)/rotators/cnctrk/local.mk
include $(srcdir)/rotators/easycomm/local.mk
include $(srcdir)/rotators/ether6/local.mk
include $(srcdir)/rotators/flir/local.mk
include $(srcdir)/rotators/fodtrack/local.mk
include $(srcdir)/rotators/grbltrk/local.mk
include $(srcdir)/rotators/gs232a/local.mk
include $(srcdir)/rotators/heathkit/local.mk
include $(srcdir)/rotators/indi/local.mk
include $(srcdir)/rotators/ioptron/local.mk
include $(srcdir)/rotators/m2/local.mk
include $(srcdir)/rotators/meade/local.mk
include $(srcdir)/rotators/prosistel/local.mk
include $(srcdir)/rotators/radant/local.mk
include $(srcdir)/rotators/rotorez/local.mk
include $(srcdir)/rotators/saebrtrack/local.mk
include $(srcdir)/rotators/sartek/local.mk
include $(srcdir)/rotators/satel/local.mk
include $(srcdir)/rotators/skywatcher/local.mk
include $(srcdir)/rotators/spid/local.mk
include $(srcdir)/rotators/ts7400/local.mk
include $(srcdir)/amplifiers/elecraft/local.mk
include $(srcdir)/amplifiers/expert/local.mk
include $(srcdir)/amplifiers/gemini/local.mk

.PHONY: backend-libraries
backend-libraries: $(noinst_LTLIBRARIES)
