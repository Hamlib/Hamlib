noinst_LTLIBRARIES += rigs/commradio/libhamlib-commradio.la
rigs_commradio_libhamlib_commradio_la_SOURCES = \
    rigs/commradio/commradio.c rigs/commradio/commradio.h rigs/commradio/frame.c \
    rigs/commradio/frame.h rigs/commradio/ctx10.c
rigs_commradio_libhamlib_commradio_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/commradio -I$(top_srcdir)/rigs/commradio $(AM_CPPFLAGS)
