rigs_uniden_sources = \
	rigs/uniden/bc895.c rigs/uniden/bc898.c rigs/uniden/bc245.c \
	rigs/uniden/pro2052.c rigs/uniden/bc780.c rigs/uniden/bc250.c \
	rigs/uniden/bcd396t.c rigs/uniden/bcd996t.c rigs/uniden/uniden.c \
	rigs/uniden/uniden.h rigs/uniden/uniden_digital.c \
	rigs/uniden/uniden_digital.h

noinst_LTLIBRARIES += rigs/uniden/libhamlib-uniden.la
rigs_uniden_libhamlib_uniden_la_SOURCES = $(rigs_uniden_sources)

rigs_uniden_libhamlib_uniden_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/uniden -I$(top_srcdir)/rigs/uniden $(AM_CPPFLAGS)
