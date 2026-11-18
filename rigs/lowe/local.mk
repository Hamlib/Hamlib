rigs_lowe_sources = rigs/lowe/hf235.c rigs/lowe/lowe.c rigs/lowe/lowe.h

noinst_LTLIBRARIES += rigs/lowe/libhamlib-lowe.la
rigs_lowe_libhamlib_lowe_la_SOURCES = $(rigs_lowe_sources)

rigs_lowe_libhamlib_lowe_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/lowe -I$(top_srcdir)/rigs/lowe $(AM_CPPFLAGS)
