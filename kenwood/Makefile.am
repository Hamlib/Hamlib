TSSRCLIST = ts870s.c ts570.c ts450s.c ts2000.c thd7.c thf7.c

lib_LTLIBRARIES = libhamlib-kenwood.la
libhamlib_kenwood_la_SOURCES = $(TSSRCLIST) kenwood.c th.c
libhamlib_kenwood_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_kenwood_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = kenwood.h th.h
