## Process this file with automake to produce Makefile.in

## Yeasu radios that use the legacy CAT commands
YAESUSRC = ft100.c ft100.h ft747.c ft747.h ft817.c ft817.h ft847.c ft847.h \
	ft890.c ft890.h ft900.c ft900.h ft920.c ft920.h ft1000mp.c ft1000mp.h \
	ft857.c ft857.h ft897.c ft897.h ft990.c ft990.h ft990v12.c ft990v12.h frg8800.c ft757gx.c \
	ft757gx.h ft600.h ft600.c ft736.c frg100.c frg100.h frg9600.c ft1000d.c \
	ft1000d.h vr5000.c ft767gx.c ft767gx.h ft840.c ft840.h ft980.c ft980.h \
	vx1700.c vx1700.h ftdx10.h ft710.c

## Yaesu radios that use the new Kenwood style CAT commands
NEWCATSRC = newcat.c newcat.h ft450.c ft450.h ft950.c ft950.h ft991.c ft991.h \
	ft2000.c ft2000.h ft9000.c ft9000.h ft5000.c ft5000.h ft1200.c ft1200.h \
	ft891.c ft891.h ftdx101.c ftdx101.h ftdx101mp.c ft3000.c ftdx10.c ft710.h \
	ftx1/ftx1.c ftx1/ftx1.h \
	ftx1/ftx1_vfo.c ftx1/ftx1_freq.c ftx1/ftx1_mode.c ftx1/ftx1_filter.c \
	ftx1/ftx1_noise.c ftx1/ftx1_func.c ftx1/ftx1_preamp.c ftx1/ftx1_audio.c \
	ftx1/ftx1_tx.c ftx1/ftx1_mem.c ftx1/ftx1_cw.c ftx1/ftx1_ctcss.c \
	ftx1/ftx1_scan.c ftx1/ftx1_info.c ftx1/ftx1_ext.c ftx1/ftx1_clarifier.c \
	ftx1/ftx1_menu.c ftx1/ftx1_menu.h

noinst_LTLIBRARIES = libhamlib-yaesu.la
libhamlib_yaesu_la_SOURCES = yaesu.c yaesu.h level_gran_yaesu.h $(YAESUSRC) $(NEWCATSRC)

EXTRA_DIST = Android.mk
