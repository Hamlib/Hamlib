TSSRCLIST = ts870s.c ts570.c

lib_LTLIBRARIES = libhamlib-kenwood.la
libhamlib_kenwood_la_SOURCES = $(TSSRCLIST) kenwood.c
libhamlib_kenwood_la_LDFLAGS = -no-undefined -module -avoid-version
#libhamlib_kenwood_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-kenwood.a
libhamlib_kenwood_a_SOURCES = $(TSSRCLIST) kenwood.c
libhamlib_kenwood_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = kenwood.h
