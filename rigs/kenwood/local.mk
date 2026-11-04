rigs_kenwood_ts_sources = \
	rigs/kenwood/ts850.c rigs/kenwood/ts870s.c rigs/kenwood/ts570.c \
	rigs/kenwood/ts450s.c rigs/kenwood/ts950.c rigs/kenwood/ts50s.c \
	rigs/kenwood/ts790.c rigs/kenwood/ts2000.c rigs/kenwood/k2.c \
	rigs/kenwood/k3.c rigs/kenwood/xg3.c rigs/kenwood/ts930.c \
	rigs/kenwood/ts680.c rigs/kenwood/ts690.c rigs/kenwood/ts140.c \
	rigs/kenwood/ts480.c rigs/kenwood/trc80.c rigs/kenwood/ts590.c \
	rigs/kenwood/ts890s.c rigs/kenwood/ts990s.c rigs/kenwood/ts990s.h \
	rigs/kenwood/flex6xxx.c rigs/kenwood/pihpsdr.c rigs/kenwood/tx500.c \
	rigs/kenwood/trusdx.c

rigs_kenwood_ic10_sources = \
	rigs/kenwood/ts440.c rigs/kenwood/ts940.c rigs/kenwood/ts711.c \
	rigs/kenwood/ts811.c rigs/kenwood/r5000.c

rigs_kenwood_th_sources = \
	rigs/kenwood/thd7.c rigs/kenwood/thf7.c rigs/kenwood/thg71.c \
	rigs/kenwood/tmd700.c rigs/kenwood/tmv7.c rigs/kenwood/thf6a.c \
	rigs/kenwood/thd72.c rigs/kenwood/tmd710.c rigs/kenwood/thd74.c

rigs_kenwood_sources = \
	rigs/kenwood/kenwood.c rigs/kenwood/kenwood.h rigs/kenwood/th.c \
	rigs/kenwood/th.h rigs/kenwood/thd7x.c rigs/kenwood/thd7x.h \
	rigs/kenwood/ic10.c rigs/kenwood/ic10.h rigs/kenwood/elecraft.c \
	rigs/kenwood/elecraft.h rigs/kenwood/transfox.c rigs/kenwood/flex.c \
	rigs/kenwood/flex.h rigs/kenwood/level_gran_kenwood.h \
	rigs/kenwood/level_gran_elecraft.h rigs/kenwood/hamgeek.c \
	rigs/kenwood/hamgeek.h

noinst_LTLIBRARIES += rigs/kenwood/libhamlib-kenwood.la
rigs_kenwood_libhamlib_kenwood_la_SOURCES = \
	$(rigs_kenwood_ts_sources) $(rigs_kenwood_th_sources) \
	$(rigs_kenwood_ic10_sources) $(rigs_kenwood_sources)

rigs_kenwood_libhamlib_kenwood_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/kenwood -I$(top_srcdir)/rigs/kenwood \
	$(AM_CPPFLAGS)
