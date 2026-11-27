noinst_LTLIBRARIES += rigs/drake/libhamlib-drake.la
rigs_drake_libhamlib_drake_la_SOURCES = \
    rigs/drake/r8.c rigs/drake/r8a.c rigs/drake/r8b.c rigs/drake/drake.c \
    rigs/drake/drake.h
rigs_drake_libhamlib_drake_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/drake -I$(top_srcdir)/rigs/drake $(AM_CPPFLAGS)
