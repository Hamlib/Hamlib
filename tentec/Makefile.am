TENTECSRCLIST = rx320.c

lib_LTLIBRARIES = libhamlib-tentec.la
libhamlib_tentec_la_SOURCES = $(TENTECSRCLIST) tentec.c
libhamlib_tentec_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_tentec_la_CFLAGS = -DHAMLIB_DLL $(AM_CFLAGS)
libhamlib_tentec_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-tentec.a
libhamlib_tentec_a_SOURCES = $(TENTECSRCLIST) tentec.c
libhamlib_tentec_a_CFLAGS = -DHAMLIB_STATIC $(AM_CFLAGS)
libhamlib_tentec_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = tentec.h
