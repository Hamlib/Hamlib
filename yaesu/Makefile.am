YAESUSRC = ft747.c ft817.c ft847.c ft100.c

lib_LTLIBRARIES = libhamlib-yaesu.la
libhamlib_yaesu_la_SOURCES = $(YAESUSRC) yaesu.c
libhamlib_yaesu_la_LDFLAGS = -no-undefined -module -version-info 0:0:0
libhamlib_yaesu_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = ft747.h ft100.h ft817.h ft847.h yaesu.h yaesu_tones.h
