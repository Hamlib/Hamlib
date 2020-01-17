GNURADIOSRC =  mc4020.c graudio.c gnuradio.h gr.c gnuradio.cc am.h demod.h \
	gr_priv.h HrAGC.h nfm.h ssb.h wfm.h

AM_CFLAGS += @GNURADIO_CFLAGS@ $(PTHREAD_CFLAGS)
AM_CXXFLAGS += @GNURADIO_CFLAGS@ $(PTHREAD_CFLAGS)

noinst_LTLIBRARIES = hamlib-gnuradio.la
hamlib_gnuradio_la_SOURCES = $(GNURADIOSRC)

# requires libgnuradio, libfftw and c++
hamlib_gnuradio_la_LIBADD = $(top_builddir)/src/libhamlib.la @GNURADIO_LIBS@ -lstdc++ $(PTHREAD_LIBS)


check_PROGRAMS = testgr

testgr_SOURCES = testgr.cc
testgr_LDADD = $(top_builddir)/src/libhamlib.la @GNURADIO_LIBS@
testgr_DEPENDENCIES = $(top_builddir)/src/libhamlib.la
