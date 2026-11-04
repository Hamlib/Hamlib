if HAVE_ANDROID_SENSOR
noinst_LTLIBRARIES += rotators/androidsensor/libhamlib-androidsensor.la
endif

rotators_androidsensor_libhamlib_androidsensor_la_SOURCES = \
    rotators/androidsensor/androidsensor.cpp \
    rotators/androidsensor/androidsensor.h \
    rotators/androidsensor/ndkimu.cpp \
    rotators/androidsensor/ndkimu.h
rotators_androidsensor_libhamlib_androidsensor_la_CPPFLAGS = \
    -I$(top_builddir)/rotators/androidsensor \
    -I$(top_srcdir)/rotators/androidsensor \
    $(AM_CPPFLAGS)
rotators_androidsensor_libhamlib_androidsensor_la_CXXFLAGS = -fno-sized-deallocation
rotators_androidsensor_libhamlib_androidsensor_la_LIBADD = -landroid
