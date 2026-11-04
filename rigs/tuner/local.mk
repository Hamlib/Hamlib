rigs_tuner_sources = \
	rigs/tuner/v4l.c rigs/tuner/v4l2.c rigs/tuner/tuner.c rigs/tuner/tuner.h \
	rigs/tuner/videodev.h rigs/tuner/videodev2.h

noinst_LTLIBRARIES += rigs/tuner/libhamlib-tuner.la
rigs_tuner_libhamlib_tuner_la_SOURCES = $(rigs_tuner_sources)

rigs_tuner_libhamlib_tuner_la_CPPFLAGS = \
	-I$(top_builddir)/rigs/tuner -I$(top_srcdir)/rigs/tuner $(AM_CPPFLAGS)
