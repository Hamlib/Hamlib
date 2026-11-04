noinst_LTLIBRARIES += rigs/jrc/libhamlib-jrc.la
rigs_jrc_libhamlib_jrc_la_SOURCES = \
    rigs/jrc/nrd535.c rigs/jrc/nrd545.c rigs/jrc/nrd525.c rigs/jrc/jrc.c rigs/jrc/jrc.h \
    rigs/jrc/jst145.c
rigs_jrc_libhamlib_jrc_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/jrc -I$(top_srcdir)/rigs/jrc $(AM_CPPFLAGS)
