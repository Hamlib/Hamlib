YAESUSRC = ft747.c ft817.c ft847.c

lib_LTLIBRARIES = libhamlib-yaesu.la
libhamlib_yaesu_la_SOURCES = $(YAESUSRC) yaesu.c
libhamlib_yaesu_la_LDFLAGS = -no-undefined -module -avoid-version
libhamlib_yaesu_la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib-yaesu.a
libhamlib_yaesu_a_SOURCES = $(YAESUSRC) yaesu.c
libhamlib_yaesu_a_LIBADD = ../src/libhamlib.la

noinst_HEADERS = ft747.h ft817.h ft847.h yaesu.h
