noinst_LTLIBRARIES += rotators/ioptron/libhamlib-ioptron.la

rotators_ioptron_libhamlib_ioptron_la_SOURCES = \
    rotators/ioptron/rot_ioptron.c \
    rotators/ioptron/rot_ioptron.h
rotators_ioptron_libhamlib_ioptron_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/ioptron \
    -I$(top_srcdir)/rotators/ioptron \
    $(AM_CPPFLAGS)
