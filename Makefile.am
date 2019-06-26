## Process this file with automake to produce Makefile.in

aclocaldir = $(datadir)/aclocal
aclocal_DATA = hamlib.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hamlib.pc

EXTRA_DIST = PLAN TODO LICENSE hamlib.m4 hamlib.pc.in README.developer \
	README.betatester README.win32 Android.mk

doc_DATA = ChangeLog COPYING COPYING.LIB LICENSE \
	README README.betatester README.developer

SUBDIRS = macros include lib \
	$(BACKEND_LIST) \
	$(ROT_BACKEND_LIST) \
	$(AMP_BACKEND_LIST) \
	src \
	$(BINDINGS) \
	tests doc

## Static list of distributed directories.  $(BACKEND_LIST) is dynamically
## assigned by 'configure' so list rig backends statically.
DIST_SUBDIRS = macros include lib src c++ bindings tests doc android scripts\
	adat alinco aor barrett drake dorji dummy elad flexradio icom icmarine jrc\
	kachina kenwood kit lowe pcr prm80 racal rft rs skanti tapr tentec\
	tuner uniden wj yaesu winradio\
	$(ROT_BACKEND_LIST) $(AMP_BACKEND_LIST)

# Install any third party macros into our tree for distribution
ACLOCAL_AMFLAGS = -I macros --install
