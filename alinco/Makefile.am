ALINCOSRCLIST = dx77.c

lib_LTLIBRARIES = libhamlib-alinco.la
libhamlib_alinco_la_SOURCES = $(ALINCOSRCLIST) alinco.c
libhamlib_alinco_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_alinco_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = alinco.h
