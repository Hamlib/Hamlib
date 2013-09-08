
WRSRCLIST = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c \
	    g303.c g313.c g305.c linradio/wrg313api.c

noinst_LTLIBRARIES = libhamlib-winradio.la
libhamlib_winradio_la_SOURCES = $(WRSRCLIST) winradio.c
libhamlib_winradio_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_winradio_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_winradio_la_LIBADD = $(top_builddir)/src/libhamlib.la

EXTRA_DIST = NOTES Android.mk

noinst_HEADERS = winradio.h linradio/radio_ioctl.h linradio/wrapi.h linradio/wrg313api.h
