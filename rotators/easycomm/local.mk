noinst_LTLIBRARIES += rotators/easycomm/libhamlib-easycomm.la

rotators_easycomm_libhamlib_easycomm_la_SOURCES = \
    rotators/easycomm/easycomm.c \
    rotators/easycomm/easycomm.h
rotators_easycomm_libhamlib_easycomm_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/easycomm \
    -I$(top_srcdir)/rotators/easycomm \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/easycomm/easycomm.txt
