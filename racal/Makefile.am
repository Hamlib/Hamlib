RACALSRCLIST = ra6790.c ra3702.c

noinst_LTLIBRARIES = libhamlib-racal.la
libhamlib_racal_la_SOURCES = $(RACALSRCLIST) racal.c ra37xx.c
libhamlib_racal_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_racal_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_racal_la_LIBADD = $(top_builddir)/src/libhamlib.la \
#						 $(top_builddir)/lib/libmisc.la

noinst_HEADERS = racal.h ra37xx.h

EXTRA_DIST = Android.mk
