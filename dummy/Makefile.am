lib_LTLIBRARIES = libhamlib-dummy.la
libhamlib_dummy_la_SOURCES = dummy.c
libhamlib_dummy_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_dummy_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-dummy.a
libhamlib_dummy_a_SOURCES = dummy.c
libhamlib_dummy_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = dummy.h
