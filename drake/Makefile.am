DRAKESRC = r8a.c r8b.c

lib_LTLIBRARIES = hamlib-drake.la
hamlib_drake_la_SOURCES = $(DRAKESRC) drake.c
hamlib_drake_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_drake_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = drake.h
