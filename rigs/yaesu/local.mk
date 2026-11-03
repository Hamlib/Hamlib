## Yaesu radios that use the legacy CAT commands
rigs_yaesu_sources = \
	rigs/yaesu/ft100.c rigs/yaesu/ft100.h rigs/yaesu/ft747.c \
	rigs/yaesu/ft747.h rigs/yaesu/ft817.c rigs/yaesu/ft817.h \
	rigs/yaesu/ft847.c rigs/yaesu/ft847.h rigs/yaesu/ft890.c \
	rigs/yaesu/ft890.h rigs/yaesu/ft900.c rigs/yaesu/ft900.h \
	rigs/yaesu/ft920.c rigs/yaesu/ft920.h rigs/yaesu/ft1000mp.c \
	rigs/yaesu/ft1000mp.h rigs/yaesu/ft857.c rigs/yaesu/ft857.h \
	rigs/yaesu/ft897.c rigs/yaesu/ft897.h rigs/yaesu/ft990.c \
	rigs/yaesu/ft990.h rigs/yaesu/ft990v12.c rigs/yaesu/ft990v12.h \
	rigs/yaesu/frg8800.c rigs/yaesu/ft757gx.c rigs/yaesu/ft757gx.h \
	rigs/yaesu/ft600.h rigs/yaesu/ft600.c rigs/yaesu/ft736.c \
	rigs/yaesu/frg100.c rigs/yaesu/frg100.h rigs/yaesu/frg9600.c \
	rigs/yaesu/ft1000d.c rigs/yaesu/ft1000d.h rigs/yaesu/vr5000.c \
	rigs/yaesu/ft767gx.c rigs/yaesu/ft767gx.h rigs/yaesu/ft840.c \
	rigs/yaesu/ft840.h rigs/yaesu/ft980.c rigs/yaesu/ft980.h \
	rigs/yaesu/vx1700.c rigs/yaesu/vx1700.h rigs/yaesu/ftdx10.h \
	rigs/yaesu/ft710.c

## Yaesu radios that use the new Kenwood style CAT commands
rigs_yaesu_newcat_sources = \
	rigs/yaesu/newcat.c rigs/yaesu/newcat.h rigs/yaesu/ft450.c \
	rigs/yaesu/ft450.h rigs/yaesu/ft950.c rigs/yaesu/ft950.h \
	rigs/yaesu/ft991.c rigs/yaesu/ft991.h rigs/yaesu/ft2000.c \
	rigs/yaesu/ft2000.h rigs/yaesu/ft9000.c rigs/yaesu/ft9000.h \
	rigs/yaesu/ft5000.c rigs/yaesu/ft5000.h rigs/yaesu/ft1200.c \
	rigs/yaesu/ft1200.h rigs/yaesu/ft891.c rigs/yaesu/ft891.h \
	rigs/yaesu/ftdx101.c rigs/yaesu/ftdx101.h rigs/yaesu/ftdx101mp.c \
	rigs/yaesu/ft3000.c rigs/yaesu/ftdx10.c rigs/yaesu/ft710.h \
	rigs/yaesu/ftx1/ftx1.c rigs/yaesu/ftx1/ftx1.h rigs/yaesu/ftx1/ftx1_vfo.c \
	rigs/yaesu/ftx1/ftx1_freq.c rigs/yaesu/ftx1/ftx1_mode.c \
	rigs/yaesu/ftx1/ftx1_filter.c rigs/yaesu/ftx1/ftx1_noise.c \
	rigs/yaesu/ftx1/ftx1_func.c rigs/yaesu/ftx1/ftx1_preamp.c \
	rigs/yaesu/ftx1/ftx1_audio.c rigs/yaesu/ftx1/ftx1_tx.c \
	rigs/yaesu/ftx1/ftx1_mem.c rigs/yaesu/ftx1/ftx1_cw.c \
	rigs/yaesu/ftx1/ftx1_ctcss.c rigs/yaesu/ftx1/ftx1_scan.c \
	rigs/yaesu/ftx1/ftx1_info.c rigs/yaesu/ftx1/ftx1_ext.c \
	rigs/yaesu/ftx1/ftx1_clarifier.c rigs/yaesu/ftx1/ftx1_menu.c \
	rigs/yaesu/ftx1/ftx1_menu.h

noinst_LTLIBRARIES += rigs/yaesu/libhamlib-yaesu.la
rigs_yaesu_libhamlib_yaesu_la_SOURCES = \
	rigs/yaesu/yaesu.c rigs/yaesu/yaesu.h rigs/yaesu/level_gran_yaesu.h \
	$(rigs_yaesu_sources) $(rigs_yaesu_newcat_sources)

rigs_yaesu_libhamlib_yaesu_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/yaesu -I$(top_srcdir)/rigs/yaesu $(AM_CPPFLAGS)
