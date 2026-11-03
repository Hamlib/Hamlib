noinst_LTLIBRARIES += rotators/meade/libhamlib-meade.la

rotators_meade_libhamlib_meade_la_SOURCES = \
    rotators/meade/meade.c \
    rotators/meade/meade.h
rotators_meade_libhamlib_meade_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/meade \
    -I$(top_srcdir)/rotators/meade \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/meade/README.md
