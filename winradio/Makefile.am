WRSRC = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c \
	g303.c g305.c \
	winradio.c winradio.h linradio/radio_ioctl.h linradio/wrapi.h

if G313_LINUX_GNU
WRSRC += g313-posix.c linradio/wrg313api.c linradio/wrg313api.h
libhamlib_winradio_la_LIBADD = $(DL_LIBS)
endif

if G313_WINDOWS
WRSRC += g313-win.c
endif

noinst_LTLIBRARIES = libhamlib-winradio.la
libhamlib_winradio_la_SOURCES = $(WRSRC)

EXTRA_DIST = NOTES Android.mk
