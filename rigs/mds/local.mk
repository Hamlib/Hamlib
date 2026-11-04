rigs_mds_sources = \
	rigs/mds/mds.c rigs/mds/mds.h rigs/mds/4710.c rigs/mds/9710.c

noinst_LTLIBRARIES += rigs/mds/libhamlib-mds.la
rigs_mds_libhamlib_mds_la_SOURCES = $(rigs_mds_sources)

rigs_mds_libhamlib_mds_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/mds -I$(top_srcdir)/rigs/mds $(AM_CPPFLAGS)
