UNIDENSRC = bc895.c bc898.c bc245.c pro2052.c bc780.c bc250.c \
	    bcd396t.c bcd996t.c

noinst_LTLIBRARIES = libhamlib-uniden.la
libhamlib_uniden_la_SOURCES = $(UNIDENSRC) uniden.c uniden_digital.c
libhamlib_uniden_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_uniden_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_uniden_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = uniden.h uniden_digital.h

EXTRA_DIST = Android.mk
