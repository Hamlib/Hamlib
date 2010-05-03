RSSRC = esmc.c eb200.c

lib_LTLIBRARIES = hamlib-rs.la
hamlib_rs_la_SOURCES = $(RSSRC) rs.c
hamlib_rs_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_rs_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = rs.h
