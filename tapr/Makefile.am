TAPRSRCLIST = dsp10.c

noinst_LTLIBRARIES = libhamlib-tapr.la
libhamlib_tapr_la_SOURCES = $(TAPRSRCLIST) tapr.c
libhamlib_tapr_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_tapr_la_LDFLAGS = -no-undefined -module -avoid-version
# hamlib_tapr_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = tapr.h

EXTRA_DIST = Android.mk
