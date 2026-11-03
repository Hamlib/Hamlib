noinst_LTLIBRARIES += rigs/guohetec/libhamlib-guohetec.la
rigs_guohetec_libhamlib_guohetec_la_SOURCES = \
    rigs/guohetec/guohetec.c rigs/guohetec/guohetec.h rigs/guohetec/pmr171.c \
    rigs/guohetec/q900.c
rigs_guohetec_libhamlib_guohetec_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/guohetec -I$(top_srcdir)/rigs/guohetec $(AM_CPPFLAGS)
