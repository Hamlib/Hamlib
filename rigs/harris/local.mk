noinst_LTLIBRARIES += rigs/harris/libhamlib-harris.la
rigs_harris_libhamlib_harris_la_SOURCES = \
    rigs/harris/harris.c rigs/harris/harris.h rigs/harris/prc138.c rigs/harris/prc138.h
rigs_harris_libhamlib_harris_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/harris -I$(top_srcdir)/rigs/harris $(AM_CPPFLAGS)
