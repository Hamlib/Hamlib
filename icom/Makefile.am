ICOMSRCLIST = ic706.c icr8500.c ic735.c ic775.c ic756.c  \
              ic275.c ic475.c ic821h.c icr7000.c ic910.c ic970.c \
			  ic718.c os535.c os456.c omni.c

lib_LTLIBRARIES = hamlib-icom.la
hamlib_icom_la_SOURCES = $(ICOMSRCLIST) icom.c frame.c optoscan.c
hamlib_icom_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_icom_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = frame.h icom.h icom_defs.h
