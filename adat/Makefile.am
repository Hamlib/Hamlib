ADATSRC = adt_200a.c adat.c

pkglib_LTLIBRARIES = hamlib-adat.la
hamlib_adat_la_SOURCES = $(ADATSRC)
hamlib_adat_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_adat_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = adat.h adt_200a.h
