rigs_winradio_sources = rigs/winradio/wr1000.c rigs/winradio/wr1500.c \
	rigs/winradio/wr1550.c rigs/winradio/wr3100.c rigs/winradio/wr3150.c \
	rigs/winradio/wr3500.c rigs/winradio/wr3700.c rigs/winradio/g303.c \
	rigs/winradio/g305.c rigs/winradio/winradio.c rigs/winradio/winradio.h \
	rigs/winradio/linradio/radio_ioctl.h rigs/winradio/linradio/wrapi.h

if G313_LINUX_GNU
rigs_winradio_sources += rigs/winradio/g313-posix.c \
	rigs/winradio/linradio/wrg313api.c rigs/winradio/linradio/wrg313api.h
rigs_winradio_libhamlib_winradio_la_LIBADD = $(DL_LIBS)
endif

if G313_WINDOWS
rigs_winradio_sources += rigs/winradio/g313-win.c
endif

if BUILD_WINRADIO
noinst_LTLIBRARIES += rigs/winradio/libhamlib-winradio.la
endif
rigs_winradio_libhamlib_winradio_la_SOURCES = $(rigs_winradio_sources)
rigs_winradio_libhamlib_winradio_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/winradio -I$(top_srcdir)/rigs/winradio \
	$(AM_CPPFLAGS)

EXTRA_DIST += rigs/winradio/NOTES
