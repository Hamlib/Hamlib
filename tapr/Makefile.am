TAPRSRCLIST = dsp10.c

pkglib_LTLIBRARIES = hamlib-tapr.la
hamlib_tapr_la_SOURCES = $(TAPRSRCLIST) tapr.c
hamlib_tapr_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_tapr_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = tapr.h

EXTRA_DIST = Android.mk
