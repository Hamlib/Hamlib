
lib_LTLIBRARIES = libhamlib++.la
libhamlib___la_SOURCES = rigclass.cc rotclass.cc
libhamlib___la_LDFLAGS = -no-undefined -version-info @ABI_VERSION@:5:0
libhamlib___la_LIBADD = $(top_builddir)/src/libhamlib.la

check_PROGRAMS = testcpp
TESTS = testcpp

testcpp_SOURCES = testcpp.cc
testcpp_LDADD = libhamlib++.la
testcpp_LDFLAGS = @BACKENDLNK@
testcpp_DEPENDENCIES = libhamlib++.la @BACKENDEPS@
