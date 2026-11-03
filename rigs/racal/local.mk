rigs_racal_sources = \
	rigs/racal/ra6790.c rigs/racal/ra3702.c rigs/racal/ra37xx.c \
	rigs/racal/ra37xx.h rigs/racal/racal.c rigs/racal/racal.h

noinst_LTLIBRARIES += rigs/racal/libhamlib-racal.la
rigs_racal_libhamlib_racal_la_SOURCES = $(rigs_racal_sources)

rigs_racal_libhamlib_racal_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/racal -I$(top_srcdir)/rigs/racal $(AM_CPPFLAGS)
