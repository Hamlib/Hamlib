noinst_LTLIBRARIES += rotators/ts7400/libhamlib-ts7400.la

rotators_ts7400_libhamlib_ts7400_la_SOURCES = \
    rotators/ts7400/ts7400.c \
    rotators/ts7400/ts7400.h
rotators_ts7400_libhamlib_ts7400_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/ts7400 \
    -I$(top_srcdir)/rotators/ts7400 \
    $(AM_CPPFLAGS)

EXTRA_DIST += \
    rotators/ts7400/include/ep93xx_adc.h \
    rotators/ts7400/include/peekpoke.h \
    rotators/ts7400/include/io.c \
    rotators/ts7400/include/peekpoke.c \
    rotators/ts7400/include/readADC.c \
    rotators/ts7400/include/test7400ADC.c
