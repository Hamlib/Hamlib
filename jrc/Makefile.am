JRCSRCLIST = nrd535.c nrd545.c

lib_LTLIBRARIES = hamlib-jrc.la
hamlib_jrc_la_SOURCES = $(JRCSRCLIST) jrc.c
hamlib_jrc_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_jrc_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = jrc.h
