PCRSRCLIST = pcr1000.c pcr100.c

lib_LTLIBRARIES = hamlib-pcr.la
hamlib_pcr_la_SOURCES = $(PCRSRCLIST) pcr.c
hamlib_pcr_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_pcr_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = pcr.h
