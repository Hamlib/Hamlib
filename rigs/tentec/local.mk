rigs_tentec_sources = \
	rigs/tentec/rx320.c rigs/tentec/rx340.c rigs/tentec/rx350.c \
	rigs/tentec/rx331.c rigs/tentec/rx331.h rigs/tentec/tt550.c \
	rigs/tentec/tt550.h rigs/tentec/pegasus.c rigs/tentec/argonaut.c \
	rigs/tentec/orion.c rigs/tentec/orion.h rigs/tentec/jupiter.c \
	rigs/tentec/omnivii.c rigs/tentec/paragon.c rigs/tentec/tentec.c \
	rigs/tentec/tentec.h rigs/tentec/tentec2.c rigs/tentec/tentec2.h

noinst_LTLIBRARIES += rigs/tentec/libhamlib-tentec.la
rigs_tentec_libhamlib_tentec_la_SOURCES = $(rigs_tentec_sources)

EXTRA_DIST += rigs/tentec/README.tentec

rigs_tentec_libhamlib_tentec_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/tentec -I$(top_srcdir)/rigs/tentec $(AM_CPPFLAGS)
