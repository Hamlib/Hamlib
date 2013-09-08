DRAKESRC = r8a.c r8b.c

noinst_LTLIBRARIES = libhamlib-drake.la
libhamlib_drake_la_SOURCES = $(DRAKESRC) drake.c
libhamlib_drake_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_drake_la_LDFLAGS = -no-undefined -module -avoid-version
# hamlib_drake_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = drake.h

EXTRA_DIST = Android.mk
