
pkglib_LTLIBRARIES = hamlib-dummy.la
hamlib_dummy_la_SOURCES = dummy.c rot_dummy.c netrigctl.c netrotctl.c
hamlib_dummy_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_dummy_la_LIBADD = $(top_builddir)/src/libhamlib.la \
			 @MATH_LIBS@

noinst_HEADERS = dummy.h rot_dummy.h

EXTRA_DIST = Android.mk
