WJSRCLIST = wj8888.c

lib_LTLIBRARIES = hamlib-wj.la
hamlib_wj_la_SOURCES = $(WJSRCLIST) wj.c
hamlib_wj_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_wj_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = wj.h
