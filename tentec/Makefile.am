TENTECSRCLIST = rx320.c pegasus.c argonaut.c

lib_LTLIBRARIES = hamlib-tentec.la
hamlib_tentec_la_SOURCES = $(TENTECSRCLIST) tentec.c tentec2.c
hamlib_tentec_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_tentec_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = tentec.h tentec2.h
