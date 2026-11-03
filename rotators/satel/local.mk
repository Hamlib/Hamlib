noinst_LTLIBRARIES += rotators/satel/libhamlib-satel.la

rotators_satel_libhamlib_satel_la_SOURCES = \
    rotators/satel/satel.c \
    rotators/satel/satel.h
rotators_satel_libhamlib_satel_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/satel \
    -I$(top_srcdir)/rotators/satel \
    $(AM_CPPFLAGS)
