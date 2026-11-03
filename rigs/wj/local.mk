rigs_wj_sources = rigs/wj/wj8888.c rigs/wj/wj.c rigs/wj/wj.h

noinst_LTLIBRARIES += rigs/wj/libhamlib-wj.la
rigs_wj_libhamlib_wj_la_SOURCES = $(rigs_wj_sources)

rigs_wj_libhamlib_wj_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/wj -I$(top_srcdir)/rigs/wj $(AM_CPPFLAGS)
