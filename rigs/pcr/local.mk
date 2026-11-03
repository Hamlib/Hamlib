rigs_pcr_sources = \
	rigs/pcr/pcr1000.c rigs/pcr/pcr100.c rigs/pcr/pcr1500.c \
	rigs/pcr/pcr2500.c rigs/pcr/pcr.c rigs/pcr/pcr.h

noinst_LTLIBRARIES += rigs/pcr/libhamlib-pcr.la
rigs_pcr_libhamlib_pcr_la_SOURCES = $(rigs_pcr_sources)

rigs_pcr_libhamlib_pcr_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/pcr -I$(top_srcdir)/rigs/pcr $(AM_CPPFLAGS)
