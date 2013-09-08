LOWESRC = hf235.c

noinst_LTLIBRARIES = libhamlib-lowe.la
libhamlib_lowe_la_SOURCES = $(LOWESRC) lowe.c
libhamlib_lowe_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_lowe_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_lowe_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = lowe.h

EXTRA_DIST = Android.mk
