if BUILD_INDI
noinst_LTLIBRARIES += rotators/indi/libhamlib-indi.la
endif

rotators_indi_libhamlib_indi_la_SOURCES = \
    rotators/indi/indi.c \
    rotators/indi/indi_wrapper.cpp \
    rotators/indi/indi_wrapper.hpp \
    rotators/indi/indi_wrapper.h
rotators_indi_libhamlib_indi_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/indi \
    -I$(top_srcdir)/rotators/indi \
    $(AM_CPPFLAGS)
rotators_indi_libhamlib_indi_la_LDFLAGS = $(INDI_LIBS)
