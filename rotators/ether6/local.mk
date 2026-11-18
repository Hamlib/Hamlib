noinst_LTLIBRARIES += rotators/ether6/libhamlib-ether6.la

rotators_ether6_libhamlib_ether6_la_SOURCES = \
    rotators/ether6/ether6.c \
    rotators/ether6/ether6.h
rotators_ether6_libhamlib_ether6_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/ether6 \
    -I$(top_srcdir)/rotators/ether6 \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/ether6/README.ether6 \
    rotators/ether6/ether6.txt
