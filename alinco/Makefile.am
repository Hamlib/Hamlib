ALINCOSRCLIST = dx77.c

lib_LTLIBRARIES = libhamlib-alinco.la
libhamlib_alinco_la_SOURCES = $(ALINCOSRCLIST) alinco.c
libhamlib_alinco_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_alinco_la_CFLAGS = -DHAMLIB_DLL $(AM_CFLAGS)
libhamlib_alinco_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-alinco.a
libhamlib_alinco_a_SOURCES = $(ALINCOSRCLIST) alinco.c
libhamlib_alinco_a_CFLAGS = -DHAMLIB_STATIC $(AM_CFLAGS)
libhamlib_alinco_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = alinco.h
