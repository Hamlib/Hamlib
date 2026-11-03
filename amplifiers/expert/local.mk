noinst_LTLIBRARIES += amplifiers/expert/libhamlib-expert.la

amplifiers_expert_libhamlib_expert_la_SOURCES = \
    amplifiers/expert/expert.c \
    amplifiers/expert/expert.h
amplifiers_expert_libhamlib_expert_la_CPPFLAGS = \
    -I$(top_builddir)/amplifiers/expert \
    -I$(top_srcdir)/amplifiers/expert \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    amplifiers/expert/README.expert
