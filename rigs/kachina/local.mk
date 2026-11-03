rigs_kachina_sources = \
	rigs/kachina/505dsp.c rigs/kachina/kachina.c rigs/kachina/kachina.h

noinst_LTLIBRARIES += rigs/kachina/libhamlib-kachina.la
rigs_kachina_libhamlib_kachina_la_SOURCES = $(rigs_kachina_sources)

rigs_kachina_libhamlib_kachina_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/kachina -I$(top_srcdir)/rigs/kachina \
	$(AM_CPPFLAGS)
