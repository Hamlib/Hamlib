SUBDIRS = linradio

WRSRCLIST = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c

lib_LTLIBRARIES = libhamlib-winradio.la
libhamlib_winradio_la_SOURCES = $(WRSRCLIST) winradio.c
libhamlib_winradio_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_winradio_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-winradio.a
libhamlib_winradio_a_SOURCES = $(WRSRCLIST) winradio.c
libhamlib_winradio_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = winradio.h
