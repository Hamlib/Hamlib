noinst_LTLIBRARIES += rotators/saebrtrack/libhamlib-saebrtrack.la

rotators_saebrtrack_libhamlib_saebrtrack_la_SOURCES = \
    rotators/saebrtrack/saebrtrack.c \
    rotators/saebrtrack/saebrtrack.h
rotators_saebrtrack_libhamlib_saebrtrack_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/saebrtrack \
    -I$(top_srcdir)/rotators/saebrtrack \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/saebrtrack/saebrtrack.txt
