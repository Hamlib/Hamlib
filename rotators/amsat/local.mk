noinst_LTLIBRARIES += rotators/amsat/libhamlib-amsat.la

rotators_amsat_libhamlib_amsat_la_SOURCES = \
    rotators/amsat/if100.c
rotators_amsat_libhamlib_amsat_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/amsat \
    -I$(top_srcdir)/rotators/amsat \
    $(AM_CPPFLAGS)
