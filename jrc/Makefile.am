JRCSRCLIST = nrd545.c

lib_LTLIBRARIES = libhamlib-jrc.la
libhamlib_jrc_la_SOURCES = $(JRCSRCLIST) jrc.c
libhamlib_jrc_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_jrc_la_CFLAGS = -DHAMLIB_DLL $(AM_CFLAGS)
libhamlib_jrc_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-jrc.a
libhamlib_jrc_a_SOURCES = $(JRCSRCLIST) jrc.c
libhamlib_jrc_a_CFLAGS = -DHAMLIB_STATIC $(AM_CFLAGS)
libhamlib_jrc_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = jrc.h
