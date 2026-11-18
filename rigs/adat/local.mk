noinst_LTLIBRARIES += rigs/adat/libhamlib-adat.la
rigs_adat_libhamlib_adat_la_SOURCES = \
    rigs/adat/adt_200a.c rigs/adat/adt_200a.h rigs/adat/adat.c rigs/adat/adat.h
rigs_adat_libhamlib_adat_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/adat -I$(top_srcdir)/rigs/adat $(AM_CPPFLAGS)
