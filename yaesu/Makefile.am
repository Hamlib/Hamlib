YAESUSRC = ft747.c ft817.c ft847.c ft100.c ft920.c

lib_LTLIBRARIES = hamlib-yaesu.la
hamlib_yaesu_la_SOURCES = $(YAESUSRC) yaesu.c
hamlib_yaesu_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_yaesu_la_LIBADD = ../src/libhamlib.la

noinst_HEADERS = ft747.h ft100.h ft817.h ft847.h ft920.h yaesu.h yaesu_tones.h
