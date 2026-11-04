noinst_LTLIBRARIES += rigs/flexradio/libhamlib-flexradio.la

EXTRA_DIST += rigs/flexradio/README
rigs_flexradio_libhamlib_flexradio_la_SOURCES = \
    rigs/flexradio/flexradio.c rigs/flexradio/flexradio.h rigs/flexradio/sdr1k.c \
    rigs/flexradio/dttsp.c rigs/flexradio/smartsdr.c rigs/flexradio/smartsdr_caps.h
rigs_flexradio_libhamlib_flexradio_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/flexradio -I$(top_srcdir)/rigs/flexradio $(AM_CPPFLAGS)
