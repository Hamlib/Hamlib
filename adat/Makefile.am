ADATSRC = adt_200a.c adat.c

noinst_LTLIBRARIES = libhamlib-adat.la
libhamlib_adat_la_SOURCES = $(ADATSRC)
# libhamlib_adat_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_adat_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# hamlib_adat_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = adat.h adt_200a.h

EXTRA_DIST = Android.mk
