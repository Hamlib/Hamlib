TUNERSRCLIST = v4l.c v4l2.c

pkglib_LTLIBRARIES = hamlib-tuner.la
hamlib_tuner_la_SOURCES = $(TUNERSRCLIST) tuner.c
hamlib_tuner_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_tuner_la_LIBADD = $(top_builddir)/src/libhamlib.la

noinst_HEADERS = tuner.h videodev.h videodev2.h
