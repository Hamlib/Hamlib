noinst_LTLIBRARIES += rotators/apex/libhamlib-apex.la

rotators_apex_libhamlib_apex_la_SOURCES = \
    rotators/apex/apex.c \
    rotators/apex/apex.h \
    rotators/apex/sharedloop.c
rotators_apex_libhamlib_apex_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/apex \
    -I$(top_srcdir)/rotators/apex \
    $(AM_CPPFLAGS)
