rigs_simplecat_sources = rigs/simplecat/simplecat.c rigs/simplecat/simplecat.h

noinst_LTLIBRARIES += rigs/simplecat/libhamlib-simplecat.la
rigs_simplecat_libhamlib_simplecat_la_SOURCES = $(rigs_simplecat_sources)

rigs_simplecat_libhamlib_simplecat_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/simplecat -I$(top_srcdir)/rigs/simplecat \
	$(AM_CPPFLAGS)
