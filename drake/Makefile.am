DRAKESRC = r8b.c

lib_LTLIBRARIES = libhamlib-drake.la
libhamlib_drake_la_SOURCES = $(DRAKESRC) drake.c
libhamlib_drake_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_drake_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = drake.h
