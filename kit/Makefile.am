KITSRCLIST = elektor304.c

lib_LTLIBRARIES = hamlib-kit.la
hamlib_kit_la_SOURCES = $(KITSRCLIST) kit.c
hamlib_kit_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_kit_la_LIBADD = $(top_builddir)/lib/libmisc.la \
		       $(top_builddir)/src/libhamlib.la

noinst_HEADERS = kit.h

