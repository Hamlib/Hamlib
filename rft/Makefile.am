RFTSRC = ekd500.c

lib_LTLIBRARIES = hamlib-rft.la
hamlib_rft_la_SOURCES = $(RFTSRC) rft.c
hamlib_rft_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_rft_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = rft.h
