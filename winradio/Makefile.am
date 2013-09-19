WRSRC = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c \
	g303.c g313.c g305.c linradio/wrg313api.c linradio/wrg313api.h \
	winradio.c winradio.h linradio/radio_ioctl.h linradio/wrapi.h

noinst_LTLIBRARIES = libhamlib-winradio.la
libhamlib_winradio_la_SOURCES = $(WRSRC)
libhamlib_winradio_la_LDFLAGS = @DL_LIB@

EXTRA_DIST = NOTES Android.mk
