AM_CFLAGS = @LIBUSB_CFLAGS@
AM_CXXFLAGS = @USRP_CFLAGS@

# FIXME: compile usrp only if CXX available
KITSRCLIST = elektor304.c drt1.c dwt.c usrp.c usrp_impl.cc elektor507.c \
	     dds60.c miniVNA.c

KITROTSRCLIST = pcrotor.c

lib_LTLIBRARIES = hamlib-kit.la
hamlib_kit_la_SOURCES = $(KITSRCLIST) $(KITROTSRCLIST) kit.c
hamlib_kit_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_kit_la_LIBADD = $(top_builddir)/lib/libmisc.la \
		       $(USRP_LIBS) \
		       $(LIBUSB_LIBS) \
		       @MATH_LIBS@ \
		       $(top_builddir)/src/libhamlib.la

noinst_HEADERS = kit.h usrp_impl.h

