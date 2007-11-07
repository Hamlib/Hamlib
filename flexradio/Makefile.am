
lib_LTLIBRARIES = hamlib-flexradio.la
hamlib_flexradio_la_SOURCES = flexradio.c sdr1k.c dttsp.c
hamlib_flexradio_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_flexradio_la_LIBADD = $(top_builddir)/lib/libmisc.la \
				$(top_builddir)/src/libhamlib.la

noinst_HEADERS = flexradio.h
