YAESUSRC =  ft100.c ft747.c ft817.c ft847.c ft890.c ft900.c ft920.c \
		ft1000mp.c ft857.c ft990.c

lib_LTLIBRARIES = hamlib-yaesu.la
hamlib_yaesu_la_SOURCES = $(YAESUSRC) yaesu.c
hamlib_yaesu_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_yaesu_la_LIBADD = $(top_builddir)/lib/libmisc.la \
			$(top_builddir)/src/libhamlib.la

noinst_HEADERS = ft100.h ft747.h ft817.h ft847.h ft890.h ft900.h ft920.h \
            ft1000mp.h ft857.h ft990.h yaesu.h yaesu_tones.h
