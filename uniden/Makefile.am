UNIDENSRC = bc895.c

lib_LTLIBRARIES = libhamlib-uniden.la
libhamlib_uniden_la_SOURCES = $(UNIDENSRC) uniden.c
libhamlib_uniden_la_LDFLAGS = -no-undefined -module -avoid-version
#libhamlib_uniden_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-uniden.a
libhamlib_uniden_a_SOURCES = $(UNIDENSRC) uniden.c
libhamlib_uniden_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = uniden.h
