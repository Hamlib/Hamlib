RACALSRCLIST = ra6790.c

lib_LTLIBRARIES = hamlib-racal.la
hamlib_racal_la_SOURCES = $(RACALSRCLIST) racal.c
hamlib_racal_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_racal_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = racal.h
