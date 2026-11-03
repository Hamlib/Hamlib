noinst_LTLIBRARIES += rigs/gomspace/libhamlib-gomspace.la
rigs_gomspace_libhamlib_gomspace_la_SOURCES = \
    rigs/gomspace/gs100.c rigs/gomspace/gs100.h
rigs_gomspace_libhamlib_gomspace_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/gomspace -I$(top_srcdir)/rigs/gomspace $(AM_CPPFLAGS)
