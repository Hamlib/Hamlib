ALINCOSRCLIST = dx77.c

lib_LTLIBRARIES = hamlib-alinco.la
hamlib_alinco_la_SOURCES = $(ALINCOSRCLIST) alinco.c
hamlib_alinco_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_alinco_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = alinco.h
