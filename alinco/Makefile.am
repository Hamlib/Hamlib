ALINCOSRCLIST = dx77.c

noinst_LTLIBRARIES = libhamlib-alinco.la
libhamlib_alinco_la_SOURCES = $(ALINCOSRCLIST) alinco.c
libhamlib_alinco_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_alinco_la_LDFLAGS = -no-undefined -module -avoid-version
# hamlib_alinco_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = alinco.h

EXTRA_DIST = Android.mk
