AORSRCLIST = ar8200.c

lib_LTLIBRARIES = libhamlib-aor.la
libhamlib_aor_la_SOURCES = $(AORSRCLIST) aor.c
libhamlib_aor_la_LDFLAGS = -module -avoid-version
libhamlib_aor_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-aor.a
libhamlib_aor_a_SOURCES = $(AORSRCLIST) aor.c
libhamlib_aor_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = aor.h
