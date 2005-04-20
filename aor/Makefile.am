AORSRCLIST = ar8200.c ar8000.c ar5000.c ar3000.c ar7030.c ar3030.c \
	     ar2700.c ar8600.c

lib_LTLIBRARIES = hamlib-aor.la
hamlib_aor_la_SOURCES = $(AORSRCLIST) aor.c
hamlib_aor_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_aor_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = aor.h
