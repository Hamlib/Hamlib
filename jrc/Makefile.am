JRCSRCLIST = nrd535.c nrd545.c nrd525.c

noinst_LTLIBRARIES = libhamlib-jrc.la
libhamlib_jrc_la_SOURCES = $(JRCSRCLIST) jrc.c
libhamlib_jrc_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_jrc_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_jrc_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = jrc.h

EXTRA_DIST = Android.mk
