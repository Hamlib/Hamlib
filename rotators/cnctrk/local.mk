noinst_LTLIBRARIES += rotators/cnctrk/libhamlib-cnctrk.la

rotators_cnctrk_libhamlib_cnctrk_la_SOURCES = \
    rotators/cnctrk/cnctrk.c
rotators_cnctrk_libhamlib_cnctrk_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/cnctrk \
    -I$(top_srcdir)/rotators/cnctrk \
    $(AM_CPPFLAGS)
