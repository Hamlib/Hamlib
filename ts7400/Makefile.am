EXTRA_DIST = include/ep93xx_adc.h include/io.h include/peekpoke.h \
	include/readADC.h include/io.c include/peekpoke.c include/readADC.c \
	include/test7400ADC.c Android.mk

noinst_LTLIBRARIES = libhamlib-ts7400.la
libhamlib_ts7400_la_SOURCES = ts7400.c
libhamlib_ts7400_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_ts7400_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_ts7400_la_LIBADD = $(top_builddir)/src/libhamlib.la \
#			 @MATH_LIBS@

noinst_HEADERS = ts7400.h
