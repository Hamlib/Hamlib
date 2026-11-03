noinst_LTLIBRARIES += rotators/ars/libhamlib-ars.la

rotators_ars_libhamlib_ars_la_SOURCES = \
    rotators/ars/ars.c \
    rotators/ars/ars.h
rotators_ars_libhamlib_ars_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/ars \
    -I$(top_srcdir)/rotators/ars \
    $(AM_CPPFLAGS)
rotators_ars_libhamlib_ars_la_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
rotators_ars_libhamlib_ars_la_LDFLAGS = $(PTHREAD_LIBS)
