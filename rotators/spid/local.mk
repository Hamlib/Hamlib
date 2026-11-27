noinst_LTLIBRARIES += rotators/spid/libhamlib-spid.la

rotators_spid_libhamlib_spid_la_SOURCES = \
    rotators/spid/spid.c \
    rotators/spid/spid.h
rotators_spid_libhamlib_spid_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/spid \
    -I$(top_srcdir)/rotators/spid \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/spid/spid.txt
