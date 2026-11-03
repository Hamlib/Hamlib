noinst_LTLIBRARIES += rotators/gs232a/libhamlib-gs232a.la

rotators_gs232a_libhamlib_gs232a_la_SOURCES = \
    rotators/gs232a/gs232a.c \
    rotators/gs232a/gs232a.h \
    rotators/gs232a/gs232b.c \
    rotators/gs232a/gs232.c
rotators_gs232a_libhamlib_gs232a_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/gs232a \
    -I$(top_srcdir)/rotators/gs232a \
    $(AM_CPPFLAGS)
