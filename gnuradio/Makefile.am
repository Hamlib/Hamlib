
AM_CPPFLAGS = @GNURADIO_CFLAGS@

lib_LTLIBRARIES = hamlib-gnuradio.la
hamlib_gnuradio_la_SOURCES = gr.c gnuradio.cc

hamlib_gnuradio_la_LDFLAGS = -no-undefined -module -avoid-version

# requires libgnuradio, libfftw and c++
hamlib_gnuradio_la_LIBADD = $(top_builddir)/src/libhamlib.la @GNURADIO_LIBS@ -lstdc++

noinst_HEADERS = gnuradio.h gr_priv.h


check_PROGRAMS = testgr

testgr_SOURCES = testgr.cc
testgr_LDADD = $(top_builddir)/src/libhamlib.la @GNURADIO_LIBS@
testgr_LDFLAGS = -dlopen hamlib-gnuradio.la
testgr_DEPENDENCIES = hamlib-gnuradio.la $(top_builddir)/src/libhamlib.la

