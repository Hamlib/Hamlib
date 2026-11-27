noinst_LTLIBRARIES += rigs/elad/libhamlib-elad.la
rigs_elad_libhamlib_elad_la_SOURCES = \
    rigs/elad/elad.c rigs/elad/elad.h rigs/elad/fdm_duo.c
rigs_elad_libhamlib_elad_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/elad -I$(top_srcdir)/rigs/elad $(AM_CPPFLAGS)
