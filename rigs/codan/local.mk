noinst_LTLIBRARIES += rigs/codan/libhamlib-codan.la
rigs_codan_libhamlib_codan_la_SOURCES = \
    rigs/codan/codan.c rigs/codan/codan.h
rigs_codan_libhamlib_codan_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/codan -I$(top_srcdir)/rigs/codan $(AM_CPPFLAGS)

EXTRA_DIST += rigs/codan/README.codan
