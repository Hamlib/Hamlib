
WRSRCLIST = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c \
	    g303.c g313.c

lib_LTLIBRARIES = hamlib-winradio.la
hamlib_winradio_la_SOURCES = $(WRSRCLIST) winradio.c
hamlib_winradio_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_winradio_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = winradio.h linradio/radio_ioctl.h linradio/wrapi.h
