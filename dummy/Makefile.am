
lib_LTLIBRARIES = hamlib-dummy.la
hamlib_dummy_la_SOURCES = dummy.c rot_dummy.c
hamlib_dummy_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_dummy_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = dummy.h rot_dummy.h
