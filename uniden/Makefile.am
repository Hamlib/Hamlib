UNIDENSRC = bc895.c bc245.c pro2052.c bc780.c bc250.c \
	    bcd396t.c bcd996t.c

lib_LTLIBRARIES = hamlib-uniden.la
hamlib_uniden_la_SOURCES = $(UNIDENSRC) uniden.c uniden_digital.c
hamlib_uniden_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_uniden_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = uniden.h uniden_digital.h
