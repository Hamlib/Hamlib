PCRSRCLIST = pcr1000.c pcr100.c

lib_LTLIBRARIES = libhamlib-pcr.la
libhamlib_pcr_la_SOURCES = $(PCRSRCLIST) pcr.c
libhamlib_pcr_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_pcr_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-pcr.a
libhamlib_pcr_a_SOURCES = $(PCRSRCLIST) pcr.c
libhamlib_pcr_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = pcr.h
