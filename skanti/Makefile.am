SKANTISRCLIST = trp8000.c trp8255.c

noinst_LTLIBRARIES = libhamlib-skanti.la
libhamlib_skanti_la_SOURCES = $(SKANTISRCLIST) skanti.c
libhamlib_skanti_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_skanti_la_LDFLAGS = -no-undefined -module -avoid-version
# hamlib_skanti_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = skanti.h

EXTRA_DIST = Android.mk
