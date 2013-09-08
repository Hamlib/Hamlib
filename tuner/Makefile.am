TUNERSRCLIST = v4l.c v4l2.c

noinst_LTLIBRARIES = libhamlib-tuner.la
libhamlib_tuner_la_SOURCES = $(TUNERSRCLIST) tuner.c
libhamlib_tuner_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
# libhamlib_tuner_la_LDFLAGS = -no-undefined -module -avoid-version
#hamlib_tuner_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = tuner.h videodev.h videodev2.h

EXTRA_DIST = Android.mk
