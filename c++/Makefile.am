
lib_LTLIBRARIES = libhamlib++.la
libhamlib___la_SOURCES = rigclass.cc
libhamlib___la_LDFLAGS = -no-undefined -release @VERSION@
libhamlib___la_CXXFLAGS = -DHAMLIB_DLL $(AM_CXXFLAGS)
libhamlib___la_LIBADD = ../src/libhamlib.la

lib_LIBRARIES = libhamlib++.a
libhamlib___a_SOURCES = rigclass.cc
libhamlib___a_CXXFLAGS = -DHAMLIB_STATIC $(AM_CXXFLAGS)
libhamlib___a_LIBADD = ../src/libhamlib.la

