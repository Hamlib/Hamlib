AORSRCLIST = ar8200.c ar8000.c

lib_LTLIBRARIES = libhamlib-aor.la
libhamlib_aor_la_SOURCES = $(AORSRCLIST) aor.c
libhamlib_aor_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_aor_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = aor.h
