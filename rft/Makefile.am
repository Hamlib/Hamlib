RFTSRC = ekd500.c

noinst_LTLIBRARIES = libhamlib-rft.la
libhamlib_rft_la_SOURCES = $(RFTSRC) rft.c
libhamlib_rft_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_rft_la_LDFLAGS = -no-undefined -module -avoid-version
# hamlib_rft_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = rft.h

EXTRA_DIST = Android.mk
