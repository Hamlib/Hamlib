rigs_rft_sources = rigs/rft/ekd500.c rigs/rft/rft.c rigs/rft/rft.h

noinst_LTLIBRARIES += rigs/rft/libhamlib-rft.la
rigs_rft_libhamlib_rft_la_SOURCES = $(rigs_rft_sources)

rigs_rft_libhamlib_rft_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/rft -I$(top_srcdir)/rigs/rft $(AM_CPPFLAGS)
