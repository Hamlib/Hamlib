noinst_LTLIBRARIES += rotators/rotorez/libhamlib-rotorez.la

rotators_rotorez_libhamlib_rotorez_la_SOURCES = \
    rotators/rotorez/rotorez.c \
    rotators/rotorez/rotorez.h
rotators_rotorez_libhamlib_rotorez_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/rotorez \
    -I$(top_srcdir)/rotators/rotorez \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/rotorez/README.rotorez \
    rotators/rotorez/rotorez.txt
