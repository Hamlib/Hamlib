rigs_kit_sources = rigs/kit/elektor304.c rigs/kit/drt1.c rigs/kit/dwt.c \
    rigs/kit/usrp.c rigs/kit/elektor507.c rigs/kit/dds60.c \
    rigs/kit/miniVNA.c rigs/kit/si570avrusb.c rigs/kit/si570avrusb.h \
    rigs/kit/funcube.c rigs/kit/funcube.h rigs/kit/fifisdr.c \
    rigs/kit/hiqsdr.c rigs/kit/kit.c rigs/kit/kit.h rigs/kit/usrp_impl.h \
    rigs/kit/rs_hfiq.c

rigs_kit_rot_sources = rigs/kit/pcrotor.c

noinst_LTLIBRARIES += rigs/kit/libhamlib-kit.la
rigs_kit_libhamlib_kit_la_SOURCES = $(rigs_kit_sources) $(rigs_kit_rot_sources)
rigs_kit_libhamlib_kit_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/kit -I$(top_srcdir)/rigs/kit $(AM_CPPFLAGS)
rigs_kit_libhamlib_kit_la_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
rigs_kit_libhamlib_kit_la_LIBADD = $(USRP_LIBS) $(LIBUSB_LIBS) $(MATH_LIBS)

if HAVE_USRP
rigs_kit_sources += rigs/kit/usrp_impl.cc
rigs_kit_libhamlib_kit_la_CXXFLAGS = $(AM_CXXFLAGS) $(USRP_CFLAGS)
rigs_kit_libhamlib_kit_la_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX \
    $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link $(CXXLD) \
    $(rigs_kit_libhamlib_kit_la_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
    $(LDFLAGS) -o $@ $(rigs_kit_libhamlib_kit_la_LDFLAGS)
else
rigs_kit_libhamlib_kit_la_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
    $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link $(CCLD) \
    $(rigs_kit_libhamlib_kit_la_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) \
    $(LDFLAGS) -o $@ $(rigs_kit_libhamlib_kit_la_LDFLAGS)
endif

EXTRA_DIST += rigs/kit/README.funcubedongle
