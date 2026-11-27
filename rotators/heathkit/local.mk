noinst_LTLIBRARIES += rotators/heathkit/libhamlib-heathkit.la

rotators_heathkit_libhamlib_heathkit_la_SOURCES = \
    rotators/heathkit/hd1780.c \
    rotators/heathkit/hd1780.h
rotators_heathkit_libhamlib_heathkit_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/heathkit \
    -I$(top_srcdir)/rotators/heathkit \
    $(AM_CPPFLAGS)
