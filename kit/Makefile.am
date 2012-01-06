AM_CFLAGS = @LIBUSB_CFLAGS@

# FIXME: compile usrp only if CXX available
KITSRCLIST = elektor304.c drt1.c dwt.c usrp.c elektor507.c \
	     dds60.c miniVNA.c si570avrusb.c funcube.c fifisdr.c hiqsdr.c

if HAVE_USRP
    AM_CXXFLAGS = @USRP_CFLAGS@
    KITSRCLIST += usrp_impl.cc
    hamlib_kit_la_LINK = $(CXXLINK) $(hamlib_kit_la_LDFLAGS)
else
    # automake gets confused and invokes the C++ linker via libtool regardless
    # of whether or not HAVE_USRP enables the .cc source.  This override forces
    # automake to invoke the C linker as no C++ is involved:
    hamlib_kit_la_LINK = $(LINK) $(hamlib_kit_la_LDFLAGS)
endif

KITROTSRCLIST = pcrotor.c

pkglib_LTLIBRARIES = hamlib-kit.la
hamlib_kit_la_SOURCES = $(KITSRCLIST) $(KITROTSRCLIST) kit.c
hamlib_kit_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_kit_la_LIBADD = $(top_builddir)/lib/libmisc.la \
		       $(USRP_LIBS) \
		       $(LIBUSB_LIBS) \
		       @MATH_LIBS@ \
		       $(top_builddir)/src/libhamlib.la

noinst_HEADERS = kit.h usrp_impl.h si570avrusb.h funcube.h

EXTRA_DIST = README.funcubedongle
