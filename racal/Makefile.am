RACALSRCLIST = ra6790.c ra3702.c

lib_LTLIBRARIES = hamlib-racal.la
hamlib_racal_la_SOURCES = $(RACALSRCLIST) racal.c ra37xx.c
hamlib_racal_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_racal_la_LIBADD = $(top_builddir)/src/libhamlib.la \
						 $(top_builddir)/lib/libmisc.la

noinst_HEADERS = racal.h ra37xx.h
