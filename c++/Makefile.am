
lib_LTLIBRARIES = libhamlib++.la
libhamlib___la_SOURCES = rigclass.cc rotclass.cc
libhamlib___la_LDFLAGS = -no-undefined -release @VERSION@ -version-info 0:0:0
libhamlib___la_LIBADD = ../src/libhamlib.la

check_PROGRAMS = testcpp

testcpp_SOURCES = testcpp.cc
testcpp_LDADD = libhamlib++.la
testcpp_LDFLAGS = @BACKENDLNK@
testcpp_DEPENDENCIES = libhamlib++.la @BACKENDEPS@
