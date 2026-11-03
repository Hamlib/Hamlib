noinst_LTLIBRARIES += amplifiers/gemini/libhamlib-gemini.la

amplifiers_gemini_libhamlib_gemini_la_SOURCES = \
    amplifiers/gemini/gemini.c \
    amplifiers/gemini/dx1200.c \
    amplifiers/gemini/gemini.h
amplifiers_gemini_libhamlib_gemini_la_CPPFLAGS = \
    -I$(top_builddir)/amplifiers/gemini \
    -I$(top_srcdir)/amplifiers/gemini \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    amplifiers/gemini/README.gemini
