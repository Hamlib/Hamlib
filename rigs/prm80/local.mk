rigs_prm80_sources = rigs/prm80/prm8060.c rigs/prm80/prm80.c rigs/prm80/prm80.h

noinst_LTLIBRARIES += rigs/prm80/libhamlib-prm80.la
rigs_prm80_libhamlib_prm80_la_SOURCES = $(rigs_prm80_sources)

rigs_prm80_libhamlib_prm80_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/prm80 -I$(top_srcdir)/rigs/prm80 $(AM_CPPFLAGS)
