noinst_LTLIBRARIES += rotators/grbltrk/libhamlib-grbltrk.la

rotators_grbltrk_libhamlib_grbltrk_la_SOURCES = \
    rotators/grbltrk/grbltrk.c
rotators_grbltrk_libhamlib_grbltrk_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/grbltrk \
    -I$(top_srcdir)/rotators/grbltrk \
    $(AM_CPPFLAGS)
