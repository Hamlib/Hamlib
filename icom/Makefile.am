ICOMSRCLIST = ic706.c icr8500.c ic735.c ic775.c ic756.c icall.c

lib_LTLIBRARIES = libhamlib-icom.la
libhamlib_icom_la_SOURCES = $(ICOMSRCLIST) icom.c frame.c
libhamlib_icom_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_icom_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-icom.a
libhamlib_icom_a_SOURCES = $(ICOMSRCLIST) icom.c frame.c
libhamlib_icom_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = frame.h icom.h icom_defs.h
