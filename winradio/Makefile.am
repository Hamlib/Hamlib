WRSRCLIST = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c

LDFLAGS = -module -avoid-version
if WINRADIO
#lib_LTLIBRARIES = @LTLIBWINRADIO@
lib_LTLIBRARIES = libhamlib-winradio.la
libhamlib_winradio_la_SOURCES = winradio.c $(WRSRCLIST)
libhamlib_winradio_la_LIBADD = ../src/libhamlib.la
#EXTRA_LTLIBRARIES = libhamlib-winradio.la
endif

lib_LIBRARIES = @LIBWINRADIO@
libhamlib_winradio_a_SOURCES = winradio.c $(WRSRCLIST)
libhamlib_winradio_a_LIBADD = ../src/libhamlib.la
EXTRA_LIBRARIES = libhamlib-winradio.a

noinst_HEADERS = winradio.h
