PCRSRCLIST = pcr1000.c pcr100.c

lib_LTLIBRARIES = libhamlib-pcr.la
libhamlib_pcr_la_SOURCES = $(PCRSRCLIST) pcr.c
libhamlib_pcr_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_pcr_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = pcr.h
