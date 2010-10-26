PRM80SRCLIST = prm8060.c

pkglib_LTLIBRARIES = hamlib-prm80.la
hamlib_prm80_la_SOURCES = $(PRM80SRCLIST) prm80.c
hamlib_prm80_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_prm80_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = prm80.h
