
WRSRCLIST = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c

lib_LTLIBRARIES = libhamlib-winradio.la
libhamlib_winradio_la_SOURCES = $(WRSRCLIST) winradio.c
libhamlib_winradio_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_winradio_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = winradio.h linradio/radio_ioctl.h linradio/wrapi.h
