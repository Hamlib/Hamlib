noinst_LTLIBRARIES += rigs/anytone/libhamlib-anytone.la

EXTRA_DIST += rigs/anytone/README.md
rigs_anytone_libhamlib_anytone_la_SOURCES = \
    rigs/anytone/anytone.c rigs/anytone/d578.c rigs/anytone/anytone.h
rigs_anytone_libhamlib_anytone_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/anytone -I$(top_srcdir)/rigs/anytone $(AM_CPPFLAGS)
