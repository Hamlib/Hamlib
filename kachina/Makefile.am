ALINCOSRCLIST = 505dsp.c

lib_LTLIBRARIES = libhamlib-kachina.la
libhamlib_kachina_la_SOURCES = $(ALINCOSRCLIST) kachina.c
libhamlib_kachina_la_LDFLAGS = -no-undefined -module -avoid-version
#libhamlib_kachina_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-kachina.a
libhamlib_kachina_a_SOURCES = $(ALINCOSRCLIST) kachina.c
libhamlib_kachina_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = kachina.h
