WRSRCLIST = wr1000.c wr1500.c wr1550.c wr3100.c wr3150.c wr3500.c wr3700.c

LDFLAGS = -no-undefined -module -avoid-version
if WINRADIO
#lib_LTLIBRARIES = @LTLIBWINRADIO@
lib_LTLIBRARIES = libhamlib-winradio.la
libhamlib_winradio_la_SOURCES = winradio.c $(WRSRCLIST)
#libhamlib_winradio_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_winradio_la_LIBADD = ../src/libhamlib.la
#EXTRA_LTLIBRARIES = libhamlib-winradio.la

#lib_LIBRARIES = @LIBWINRADIO@
lib_LIBRARIES = libhamlib-winradio.a
libhamlib_winradio_a_SOURCES = winradio.c $(WRSRCLIST)
libhamlib_winradio_a_LIBADD = ../src/libhamlib.la
#EXTRA_LIBRARIES = libhamlib-winradio.a
else
BUILT_SOURCES = winradio.c $(WRSRCLIST)
endif

noinst_HEADERS = winradio.h
