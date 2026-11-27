rigs_motorola_rs_sources = \
	rigs/motorola/motorola.c rigs/motorola/micom.c rigs/motorola/motorola.h

noinst_LTLIBRARIES += rigs/motorola/libhamlib-motorola.la
rigs_motorola_libhamlib_motorola_la_SOURCES = $(rigs_motorola_rs_sources)

rigs_motorola_libhamlib_motorola_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/motorola -I$(top_srcdir)/rigs/motorola \
	$(AM_CPPFLAGS)
