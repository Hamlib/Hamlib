LOWESRC = hf235.c

lib_LTLIBRARIES = hamlib-lowe.la
hamlib_lowe_la_SOURCES = $(LOWESRC) lowe.c
hamlib_lowe_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_lowe_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = lowe.h
