rigs_tci_sources = \
	rigs/tci/tci.c rigs/tci/tci2.c rigs/tci/tci2.h rigs/tci/sunsdr2-pro.c

noinst_LTLIBRARIES += rigs/tci/libhamlib-tci.la
rigs_tci_libhamlib_tci_la_SOURCES = $(rigs_tci_sources)

EXTRA_DIST += rigs/tci/README-TCI-2.0.md

rigs_tci_libhamlib_tci_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/tci -I$(top_srcdir)/rigs/tci $(AM_CPPFLAGS)
