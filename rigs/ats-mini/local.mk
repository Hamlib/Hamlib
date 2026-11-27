noinst_LTLIBRARIES += rigs/ats-mini/libhamlib-ats-mini.la
rigs_ats_mini_libhamlib_ats_mini_la_SOURCES = \
    rigs/ats-mini/ats.c rigs/ats-mini/ats.h
rigs_ats_mini_libhamlib_ats_mini_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/ats-mini -I$(top_srcdir)/rigs/ats-mini $(AM_CPPFLAGS)
