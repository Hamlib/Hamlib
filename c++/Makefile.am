if ENABLE_CXX

lib_LTLIBRARIES = libhamlib++.la
libhamlib___la_SOURCES = rigclass.cc rotclass.cc ampclass.cc
libhamlib___la_LDFLAGS = -no-undefined -version-info $(ABI_VERSION):$(ABI_REVISION):$(ABI_AGE) $(LDFLAGS)
libhamlib___la_LIBADD = $(top_builddir)/src/libhamlib.la

check_PROGRAMS = testcpp

testcpp_SOURCES = testcpp.cc
testcpp_LDADD = libhamlib++.la $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS)
testcpp_DEPENDENCIES = libhamlib++.la

TESTS = $(check_PROGRAMS)

$(top_builddir)/src/libhamlib.la:
	$(MAKE) -C $(top_builddir)/src/ libhamlib.la

$(top_builddir)/lib/libmisc.la:
	$(MAKE) -C $(top_builddir)/lib/ libmisc.la

endif
