noinst_LTLIBRARIES += rotators/skywatcher/libhamlib-skywatcher.la

rotators_skywatcher_libhamlib_skywatcher_la_SOURCES = \
    rotators/skywatcher/skywatcher.c \
    rotators/skywatcher/skywatcher.h
rotators_skywatcher_libhamlib_skywatcher_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/skywatcher \
    -I$(top_srcdir)/rotators/skywatcher \
    $(AM_CPPFLAGS)
