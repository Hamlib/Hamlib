
AM_CFLAGS = @GNURADIO_CFLAGS@ @PTHREAD_CFLAGS@
AM_CXXFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES = hamlib-gnuradio.la
hamlib_gnuradio_la_SOURCES = mc4020.c graudio.c gr.c gnuradio.cc

hamlib_gnuradio_la_LDFLAGS = -no-undefined -module -avoid-version

# requires libgnuradio, libfftw and c++
hamlib_gnuradio_la_LIBADD = $(top_builddir)/src/libhamlib.la @GNURADIO_LIBS@ -lstdc++ @PTHREAD_LIBS@

noinst_HEADERS = gnuradio.h gr_priv.h demod.h nfm.h am.h ssb.h wfm.h HrAGC.h


check_PROGRAMS = testgr

testgr_SOURCES = testgr.cc
testgr_LDADD = $(top_builddir)/src/libhamlib.la @GNURADIO_LIBS@
testgr_LDFLAGS = @BACKENDLNK@
testgr_DEPENDENCIES = @BACKENDEPS@ $(top_builddir)/src/libhamlib.la

