## Process this file with automake to produce Makefile.in

## Yeasu radios that use the legacy CAT commands
YAESUSRC =  ft100.c ft747.c ft817.c ft847.c ft890.c ft900.c ft920.c \
		ft1000mp.c ft857.c ft897.c ft990.c frg8800.c \
		ft757gx.c ft736.c frg100.c frg9600.c ft1000d.c \
		vr5000.c ft767gx.c

## Yaesu radios that use the new Kenwood style CAT commands		
NEWCATSRC = newcat.c ft450.c ft950.c

lib_LTLIBRARIES = hamlib-yaesu.la
hamlib_yaesu_la_SOURCES = $(YAESUSRC) $(NEWCATSRC) yaesu.c
hamlib_yaesu_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_yaesu_la_LIBADD = $(top_builddir)/lib/libmisc.la \
			 @MATH_LIBS@ \
			$(top_builddir)/src/libhamlib.la

noinst_HEADERS = ft100.h ft747.h ft817.h ft847.h ft890.h ft900.h ft920.h \
		 ft1000mp.h ft857.h ft897.h ft990.h yaesu.h yaesu_tones.h \
		 ft757gx.h ft767gx.h ft450.h ft950.h newcat.h
