UNIDENSRC = bc895.c

lib_LTLIBRARIES = hamlib-uniden.la
hamlib_uniden_la_SOURCES = $(UNIDENSRC) uniden.c
hamlib_uniden_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_uniden_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = uniden.h
