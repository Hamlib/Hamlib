
lib_LTLIBRARIES = libhamlib++.la
libhamlib___la_SOURCES = rigclass.cc rotclass.cc
libhamlib___la_LDFLAGS = -no-undefined -version-info $(ABI_VERSION):$(ABI_REVISION):$(ABI_AGE)
libhamlib___la_LIBADD = $(top_builddir)/src/libhamlib.la

check_PROGRAMS = testcpp

testcpp_SOURCES = testcpp.cc
testcpp_LDADD = libhamlib++.la
testcpp_DEPENDENCIES = libhamlib++.la

check_SCRIPTS = testcpp.sh

TESTS = $(check_SCRIPTS)


testcpp.sh:
	echo 'LD_LIBRARY_PATH=$(top_builddir)/c++/.libs:$(top_builddir)/dummy/.libs ./testcpp' > testcpp.sh
	chmod +x ./testcpp.sh

CLEANFILES = testcpp.sh
