noinst_LTLIBRARIES += rotators/prosistel/libhamlib-prosistel.la

rotators_prosistel_libhamlib_prosistel_la_SOURCES = \
    rotators/prosistel/prosistel.c \
    rotators/prosistel/prosistel.h
rotators_prosistel_libhamlib_prosistel_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/prosistel \
    -I$(top_srcdir)/rotators/prosistel \
    $(AM_CPPFLAGS)
