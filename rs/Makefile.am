RSSRC = esmc.c eb200.c

noinst_LTLIBRARIES = libhamlib-rs.la
libhamlib_rs_la_SOURCES = $(RSSRC) rs.c
libhamlib_rs_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_rs_la_LDFLAGS = -no-undefined -module -avoid-version
# hamlib_rs_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = rs.h

EXTRA_DIST = Android.mk
