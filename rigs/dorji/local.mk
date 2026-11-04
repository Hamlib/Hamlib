noinst_LTLIBRARIES += rigs/dorji/libhamlib-dorji.la
rigs_dorji_libhamlib_dorji_la_SOURCES = \
    rigs/dorji/dorji.c rigs/dorji/dorji.h rigs/dorji/dra818.c rigs/dorji/dra818.h
rigs_dorji_libhamlib_dorji_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/dorji -I$(top_srcdir)/rigs/dorji $(AM_CPPFLAGS)
