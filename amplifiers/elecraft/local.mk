noinst_LTLIBRARIES += amplifiers/elecraft/libhamlib-elecraft.la

amplifiers_elecraft_libhamlib_elecraft_la_SOURCES = \
    amplifiers/elecraft/kpa1500.c \
    amplifiers/elecraft/kpa.c \
    amplifiers/elecraft/kpa.h
amplifiers_elecraft_libhamlib_elecraft_la_CPPFLAGS = \
    -I$(top_builddir)/amplifiers/elecraft \
    -I$(top_srcdir)/amplifiers/elecraft \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    amplifiers/elecraft/README.elecraft
