rigs_skanti_sources = \
	rigs/skanti/trp8000.c rigs/skanti/trp8255.c rigs/skanti/skanti.c \
	rigs/skanti/skanti.h

noinst_LTLIBRARIES += rigs/skanti/libhamlib-skanti.la
rigs_skanti_libhamlib_skanti_la_SOURCES = $(rigs_skanti_sources)

rigs_skanti_libhamlib_skanti_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/skanti -I$(top_srcdir)/rigs/skanti $(AM_CPPFLAGS)
