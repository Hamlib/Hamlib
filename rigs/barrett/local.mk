noinst_LTLIBRARIES += rigs/barrett/libhamlib-barrett.la
rigs_barrett_libhamlib_barrett_la_SOURCES = \
    rigs/barrett/barrett.c rigs/barrett/barrett.h rigs/barrett/950.c rigs/barrett/4050.c \
    rigs/barrett/4100.c
rigs_barrett_libhamlib_barrett_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/barrett -I$(top_srcdir)/rigs/barrett $(AM_CPPFLAGS)
