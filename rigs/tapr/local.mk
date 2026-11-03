rigs_tapr_sources = rigs/tapr/dsp10.c rigs/tapr/tapr.c rigs/tapr/tapr.h

noinst_LTLIBRARIES += rigs/tapr/libhamlib-tapr.la
rigs_tapr_libhamlib_tapr_la_SOURCES = $(rigs_tapr_sources)

rigs_tapr_libhamlib_tapr_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/tapr -I$(top_srcdir)/rigs/tapr $(AM_CPPFLAGS)
