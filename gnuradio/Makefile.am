
CPPFLAGS = @GNURADIO_CFLAGS@

lib_LTLIBRARIES = libhamlib-gnuradio.la
libhamlib_gnuradio_la_SOURCES = gr.c gnuradio.cc

libhamlib_gnuradio_la_LDFLAGS = -no-undefined -module -version-info 0:0:0

# requires libgnuradio, libfftw and c++
libhamlib_gnuradio_la_LIBADD = ../src/libhamlib.la @GNURADIO_LIBS@ -lstdc++

noinst_HEADERS = gnuradio.h gr_priv.h


check_PROGRAMS = testgr

testgr_SOURCES = testgr.cc
testgr_LDADD = ../src/libhamlib.la @GNURADIO_LIBS@
testgr_LDFLAGS = -dlopen libhamlib-gnuradio.la
testgr_DEPENDENCIES = libhamlib-gnuradio.la ../src/libhamlib.la

