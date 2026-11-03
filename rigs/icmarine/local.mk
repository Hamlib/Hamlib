noinst_LTLIBRARIES += rigs/icmarine/libhamlib-icmarine.la
rigs_icmarine_libhamlib_icmarine_la_SOURCES = \
    rigs/icmarine/icm700pro.c rigs/icmarine/icm710.c rigs/icmarine/icm710.h \
    rigs/icmarine/icm802.c rigs/icmarine/icm803.c rigs/icmarine/icmarine.c \
    rigs/icmarine/icmarine.h
rigs_icmarine_libhamlib_icmarine_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/icmarine -I$(top_srcdir)/rigs/icmarine $(AM_CPPFLAGS)
