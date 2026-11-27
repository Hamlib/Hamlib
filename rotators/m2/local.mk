noinst_LTLIBRARIES += rotators/m2/libhamlib-m2.la

rotators_m2_libhamlib_m2_la_SOURCES = \
    rotators/m2/rc2800.c \
    rotators/m2/rc2800.h
rotators_m2_libhamlib_m2_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/m2 \
    -I$(top_srcdir)/rotators/m2 \
    $(AM_CPPFLAGS)
