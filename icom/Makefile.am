ICOMSRCLIST = ic706.c icr8500.c ic735.c ic775.c ic756.c icall.c ic275.c ic475.c

lib_LTLIBRARIES = libhamlib-icom.la
libhamlib_icom_la_SOURCES = $(ICOMSRCLIST) icom.c frame.c
libhamlib_icom_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_icom_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = frame.h icom.h icom_defs.h
