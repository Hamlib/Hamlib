noinst_LTLIBRARIES += rotators/flir/libhamlib-flir.la

rotators_flir_libhamlib_flir_la_SOURCES = \
    rotators/flir/flir.c \
    rotators/flir/flir.h
rotators_flir_libhamlib_flir_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/flir \
    -I$(top_srcdir)/rotators/flir \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/flir/README.md
