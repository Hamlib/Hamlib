TENTECSRCLIST = rx320.c

lib_LTLIBRARIES = libhamlib-tentec.la
libhamlib_tentec_la_SOURCES = $(TENTECSRCLIST) tentec.c
libhamlib_tentec_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_tentec_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = tentec.h
