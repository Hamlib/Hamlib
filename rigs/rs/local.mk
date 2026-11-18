rigs_rs_sources = \
	rigs/rs/esmc.c rigs/rs/eb200.c rigs/rs/rs.c rigs/rs/rs.h rigs/rs/gp2000.c \
	rigs/rs/gp2000.h rigs/rs/xk2100.c rigs/rs/ek89x.c rigs/rs/ek89x.h \
	rigs/rs/xk852.h rigs/rs/xk852.c

noinst_LTLIBRARIES += rigs/rs/libhamlib-rs.la
rigs_rs_libhamlib_rs_la_SOURCES = $(rigs_rs_sources)

rigs_rs_libhamlib_rs_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/rs -I$(top_srcdir)/rigs/rs $(AM_CPPFLAGS)
