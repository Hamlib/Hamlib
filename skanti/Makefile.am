SKANTISRCLIST = trp8000.c trp8255.c

pkglib_LTLIBRARIES = hamlib-skanti.la
hamlib_skanti_la_SOURCES = $(SKANTISRCLIST) skanti.c
hamlib_skanti_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_skanti_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = skanti.h
