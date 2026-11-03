noinst_LTLIBRARIES += rotators/celestron/libhamlib-celestron.la

rotators_celestron_libhamlib_celestron_la_SOURCES = \
    rotators/celestron/celestron.c \
    rotators/celestron/celestron.h
rotators_celestron_libhamlib_celestron_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/celestron \
    -I$(top_srcdir)/rotators/celestron \
    $(AM_CPPFLAGS)
