PRM80SRCLIST = prm8060.c

noinst_LTLIBRARIES = libhamlib-prm80.la
libhamlib_prm80_la_SOURCES = $(PRM80SRCLIST) prm80.c
libhamlib_prm80_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_prm80_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_prm80_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = prm80.h

EXTRA_DIST = Android.mk
