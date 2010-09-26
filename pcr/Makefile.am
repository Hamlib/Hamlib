PCRSRCLIST = pcr1000.c pcr100.c pcr1500.c pcr2500.c

pkglib_LTLIBRARIES = hamlib-pcr.la
hamlib_pcr_la_SOURCES = $(PCRSRCLIST) pcr.c
hamlib_pcr_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_pcr_la_LIBADD = $(top_builddir)/src/libhamlib.la \
		       $(top_builddir)/lib/libmisc.la

noinst_HEADERS = pcr.h
