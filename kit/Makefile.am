AM_CFLAGS += $(LIBUSB_CFLAGS)

# FIXME: compile usrp only if CXX available

KITSRC = elektor304.c drt1.c dwt.c usrp.c elektor507.c dds60.c miniVNA.c \
	si570avrusb.c si570avrusb.h funcube.c funcube.h fifisdr.c hiqsdr.c \
  kit.c kit.h usrp_impl.h rs_hfiq.c

if HAVE_USRP
    # Append to the already defined AM_CXXFLAGS that exists outside the conditional.
    AM_CXXFLAGS += $(USRP_CFLAGS)
    KITSRC += usrp_impl.cc
    libhamlib_kit_la_LINK = $(CXXLINK) $(libhamlib_kit_la_LDFLAGS)
else
    # automake gets confused and invokes the C++ linker via libtool regardless
    # of whether or not HAVE_USRP enables the .cc source.  This override forces
    # automake to invoke the C linker as no C++ is involved:
    libhamlib_kit_la_LINK = $(LINK) $(libhamlib_kit_la_LDFLAGS)
endif

KITROTSRC = pcrotor.c

noinst_LTLIBRARIES = libhamlib-kit.la
libhamlib_kit_la_SOURCES = $(KITSRC) $(KITROTSRC)
libhamlib_kit_la_LIBADD = $(USRP_LIBS) $(LIBUSB_LIBS) $(MATH_LIBS)

EXTRA_DIST = README.funcubedongle Android.mk
