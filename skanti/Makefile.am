SKANTISRCLIST = trp8000.c

lib_LTLIBRARIES = hamlib-skanti.la
hamlib_skanti_la_SOURCES = $(SKANTISRCLIST) skanti.c
hamlib_skanti_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_skanti_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = skanti.h
