noinst_LTLIBRARIES += rigs/alinco/libhamlib-alinco.la
rigs_alinco_libhamlib_alinco_la_SOURCES = \
    rigs/alinco/dx77.c rigs/alinco/dxsr8.c rigs/alinco/alinco.c rigs/alinco/alinco.h
rigs_alinco_libhamlib_alinco_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/alinco -I$(top_srcdir)/rigs/alinco $(AM_CPPFLAGS)
