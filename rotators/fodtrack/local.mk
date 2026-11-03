noinst_LTLIBRARIES += rotators/fodtrack/libhamlib-fodtrack.la

rotators_fodtrack_libhamlib_fodtrack_la_SOURCES = \
    rotators/fodtrack/fodtrack.c \
    rotators/fodtrack/fodtrack.h
rotators_fodtrack_libhamlib_fodtrack_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/fodtrack \
    -I$(top_srcdir)/rotators/fodtrack \
    $(AM_CPPFLAGS)
