
lib_LTLIBRARIES = libhamlib++.la
libhamlib___la_SOURCES = rigclass.cc rotclass.cc ampclass.cc
libhamlib___la_LDFLAGS = -no-undefined -version-info $(ABI_VERSION):$(ABI_REVISION):$(ABI_AGE) $(LDFLAGS)
libhamlib___la_LIBADD = $(top_builddir)/src/libhamlib.la
AM_CXXFLAGS=$(CXXFLAGS)

check_PROGRAMS = testcpp

testcpp_SOURCES = testcpp.cc
testcpp_LDADD = libhamlib++.la $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS)
testcpp_DEPENDENCIES = libhamlib++.la

check_SCRIPTS = testcpp.sh

TESTS = $(check_SCRIPTS)


testcpp.sh:
	echo 'LD_LIBRARY_PATH=$(top_builddir)/c++/.libs:$(top_builddir)/dummy/.libs ./testcpp' > testcpp.sh
	chmod +x ./testcpp.sh

CLEANFILES = testcpp.sh
