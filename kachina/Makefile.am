ALINCOSRCLIST = 505dsp.c

lib_LTLIBRARIES = hamlib-kachina.la
hamlib_kachina_la_SOURCES = $(ALINCOSRCLIST) kachina.c
hamlib_kachina_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_kachina_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = kachina.h
