noinst_LTLIBRARIES += rotators/radant/libhamlib-radant.la

rotators_radant_libhamlib_radant_la_SOURCES = \
    rotators/radant/radant.c \
    rotators/radant/radant.h
rotators_radant_libhamlib_radant_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/radant \
    -I$(top_srcdir)/rotators/radant \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/radant/radant.txt
