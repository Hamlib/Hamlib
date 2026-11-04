noinst_LTLIBRARIES += rotators/sartek/libhamlib-sartek.la

rotators_sartek_libhamlib_sartek_la_SOURCES = \
    rotators/sartek/sartek.c \
    rotators/sartek/sartek.h
rotators_sartek_libhamlib_sartek_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/sartek \
    -I$(top_srcdir)/rotators/sartek \
    $(AM_CPPFLAGS)
