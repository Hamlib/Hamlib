# Current Autotools documentation suggests that DejaGNU is obsolete
# and replaced by Autotest.  TODO: implement Autotest

# AUTOMAKE_OPTIONS = dejagnu
# DEJATOOL = testfreq testbcd testloc rigctl

if TESTS_HAVE_LIBUSB
    TESTLIBUSB = rigtestlibusb
else
    TESTLIBUSB =
endif

DISTCLEANFILES = rigctl.log rigctl.sum testbcd.log testbcd.sum

bin_PROGRAMS = rigctl rigctld rigmem rigsmtr rigswr rotctl rotctld rigctlcom rigctltcp rigctlsync ampctl ampctld rigtestmcast rigtestmcastrx $(TESTLIBUSB)

# Keep test registries sorted and one entry per line.
# This keeps independent additions from editing shared lines.
DIRECT_TESTS = \
	testbandmetadata \
	testctlparser \
	testdebug \
	testdummyparm \
	testftx1parsers \
	testgeministatus \
	testgs100 \
	testicomts \
	testrigctlprotocol \
	testthd7x \
	testthd75emu

GENERATED_TEST_WRAPPERS = \
	test2038.sh \
	testbcd.sh \
	testcache.sh \
	testcookie.sh \
	testfreq.sh \
	testgrid.sh \
	testloc.sh \
	testnetrigctl.sh \
	testnetrigctllocale.sh \
	testrig.sh \
	testrigcaps.sh

check_PROGRAMS = \
	cachetest \
	cachetest2 \
	dumpmem \
	hamlibmodels \
	listrigs \
	rig_bench \
	test2038 \
	testbcd \
	testcache \
	testcookie \
	testfreq \
	testgrid \
	testloc \
	testmW2power \
	testnetrigctl \
	testrig \
	testrigcaps \
	testrigopen \
	$(DIRECT_TESTS)
# Document building testsecurity
### check_PROGRAMS += testsecurity

# rigstreamtest is a streaming diagnostic tool rather than a unit test, so it is
# built with everything else instead of only under "make check". It is not
# installed: it drives a radio's audio/IQ streams from the build tree.
noinst_PROGRAMS = rigstreamtest

RIGCOMMONSRC = rigctl_parse.c rigctl_parse.h rigctld_stream.c rigctld_stream.h rigctld_client.h dumpcaps.c dumpstate.c uthash.h rig_tests.c rig_tests.h dumpcaps.h
ROTCOMMONSRC = rotctl_parse.c rotctl_parse.h dumpcaps_rot.c uthash.h dumpcaps_rot.h
AMPCOMMONSRC = ampctl_parse.c ampctl_parse.h dumpcaps_amp.c uthash.h 

rigctl_SOURCES = rigctl.c $(RIGCOMMONSRC)
rigctld_SOURCES = rigctld.c $(RIGCOMMONSRC)
rigctlcom_SOURCES = rigctlcom.c $(RIGCOMMONSRC)
rigctltcp_SOURCES = rigctltcp.c $(RIGCOMMONSRC)
rigctlsync_SOURCES = rigctlsync.c $(RIGCOMMONSRC)
rotctl_SOURCES = rotctl.c $(ROTCOMMONSRC)
rotctld_SOURCES = rotctld.c $(ROTCOMMONSRC)
ampctl_SOURCES = ampctl.c $(AMPCOMMONSRC)
ampctld_SOURCES = ampctld.c $(AMPCOMMONSRC)
rigmem_SOURCES = rigmem.c memsave.c memload.c memcsv.c
rigstreamtest_SOURCES = rigstreamtest.c rigstreamtest_util.c rigstreamtest_util.h
if TESTS_HAVE_LIBUSB
    rigtestlibusb_SOURCES = rigtestlibusb.c
endif

# include generated include files ahead of any in sources
rigctl_CPPFLAGS = -I$(top_builddir)/tests -I$(top_srcdir)/src -I$(top_builddir)/src -I$(srcdir) -I$(top_builddir)/security $(AM_CPPFLAGS)

# all the programs need this
LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS) -lm

if ANDROID
LDADD += $(ANDROID_LIBS)
endif

rigmem_CFLAGS = $(AM_CFLAGS) $(LIBXML2_CFLAGS) -I$(top_builddir)/src
rigctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_srcdir)/src -I$(top_builddir)/src -I$(top_builddir)/security
rotctl_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
rotctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
ampctl_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
ampctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
rigctlcom_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_srcdir)/src -I$(top_builddir)/security
rigctltcp_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_srcdir)/src -I$(top_builddir)/security
rigctlsync_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_srcdir)/src -I$(top_builddir)/security
testdebug_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
testnetrigctl_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
testctlparser_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
testgeministatus_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
testgs100_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
testicomts_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/icom
testgeministatus_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/amplifiers/gemini
testftx1parsers_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/yaesu/ftx1
if TESTS_HAVE_LIBUSB
    rigtestlibusb_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) $(LIBUSB_CFLAGS)
endif

# Document building testsecurity
### testsecurity_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_srcdir)/security

rigctl_LDADD = $(PTHREAD_LIBS) $(READLINE_LIBS) $(LDADD)
rigctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
ampctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
ampctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigmem_LDADD = $(LIBXML2_LIBS) $(LDADD)
rigctlcom_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigctltcp_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigctlsync_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
testdebug_LDADD = $(PTHREAD_LIBS) $(LDADD)
rigstreamtest_LDADD = $(PTHREAD_LIBS) $(LDADD)
rigstreamtest_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
rigstreamtest_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src \
    $(AM_CPPFLAGS)
testnetrigctl_LDADD = $(PTHREAD_LIBS) $(top_builddir)/rigs/dummy/libhamlib-dummy.la $(LDADD)
testctlparser_SOURCES = testctlparser.c $(RIGCOMMONSRC)
testctlparser_LDADD = $(PTHREAD_LIBS) $(READLINE_LIBS) $(top_builddir)/rigs/dummy/libhamlib-dummy.la $(LDADD)
testrigctlprotocol_LDADD = $(LDADD)
testicomts_LDADD = $(top_builddir)/rigs/icom/libhamlib-icom.la $(LDADD)
testgeministatus_LDADD = $(PTHREAD_LIBS) $(top_builddir)/amplifiers/gemini/libhamlib-gemini.la $(LDADD)
testgs100_LDADD = $(PTHREAD_LIBS) $(top_builddir)/rigs/gomspace/libhamlib-gomspace.la $(LDADD)
testftx1parsers_LDADD = $(top_builddir)/rigs/yaesu/libhamlib-yaesu.la $(LDADD)
if TESTS_HAVE_LIBUSB
    rigtestlibusb_LDADD = $(LIBUSB_LIBS)
endif

# Document linking testsecurity
### testsecurity_LDADD = $(PTHREAD_LIBS) $(LDADD) $(top_builddir)/security/libsecurity.la

# Linker options
rigctl_LDFLAGS = $(WINEXELDFLAGS)
rigswr_LDFLAGS = $(WINEXELDFLAGS)
rigsmtr_LDFLAGS = $(WINEXELDFLAGS)
rigmem_LDFLAGS = $(WINEXELDFLAGS)
rotctl_LDFLAGS = $(WINEXELDFLAGS)
ampctl_LDFLAGS = $(WINEXELDFLAGS)
rigctld_LDFLAGS = $(WINEXELDFLAGS)
rotctld_LDFLAGS = $(WINEXELDFLAGS)
ampctld_LDFLAGS = $(WINEXELDFLAGS)
rigctlcom_LDFLAGS = $(WINEXELDFLAGS)
rigctltcp_LDFLAGS = $(WINEXELDFLAGS)
rigctlsync_LDFLAGS = $(WINEXELDFLAGS)
if TESTS_HAVE_LIBUSB
    rigtestlibusb_LDFLAGS = $(WINEXELDFLAGS)
endif

# Document linking testsecurity with MinGW
### testsecurity_LDFLAGS = $(WINEXELDFLAGS)

if HTML_MATRIX
EXTRA_PROGRAMS = rigmatrix
# rigmatrix needs libgd
rigmatrix_LDFLAGS = -lgd -lz
rigmatrix.html: rigmatrix_head.html rigmatrix listrigs
	mkdir -p sup-info/support
	( cat $(srcdir)/rigmatrix_head.html && cd sup-info && ../rigmatrix ) > sup-info/rigmatrix.html
	for f in $$(./listrigs | tail -n +2 | cut -f1) ; do ( ./rigctl -m $$f -u > sup-info/support/model$$f.txt || exit 0 ) ; done
	./rigctl -l |sort -n | $(srcdir)/rig_split_lst.awk -v lst_dir="sup-info"
endif


EXTRA_DIST = \
	amptest.sh \
	cachetest.sh \
	hamlib_tuner_control \
	rig_split_lst.awk \
	rigmatrix_head.html \
	rigstreamtest.sh \
	testcaps.sh \
	testctlbounds.sh \
	testctld.pl \
	testrotctld.pl

# Support 'make check' target for simple tests
# Omitting cachetest.sh because it needs 2 instances of rigctld running
check_SCRIPTS = \
	amptest.sh \
	rigstreamtest.sh \
	testcaps.sh \
	testctlbounds.sh \
	$(GENERATED_TEST_WRAPPERS)

TESTS = $(check_SCRIPTS) $(DIRECT_TESTS)

$(top_builddir)/src/libhamlib.la:
	$(MAKE) -C $(top_builddir)/src/ libhamlib.la

$(top_builddir)/lib/libmisc.la:
	$(MAKE) -C $(top_builddir)/lib/ libmisc.la

$(top_builddir)/rigs/dummy/libhamlib-dummy.la:
	$(MAKE) -C $(top_builddir)/rigs/dummy/ libhamlib-dummy.la

$(top_builddir)/rigs/icom/libhamlib-icom.la:
	$(MAKE) -C $(top_builddir)/rigs/icom/ libhamlib-icom.la

$(top_builddir)/amplifiers/gemini/libhamlib-gemini.la:
	$(MAKE) -C $(top_builddir)/amplifiers/gemini/ libhamlib-gemini.la

$(top_builddir)/rigs/gomspace/libhamlib-gomspace.la:
	$(MAKE) -C $(top_builddir)/rigs/gomspace/ libhamlib-gomspace.la

$(top_builddir)/rigs/yaesu/libhamlib-yaesu.la:
	$(MAKE) -C $(top_builddir)/rigs/yaesu/ libhamlib-yaesu.la

testrig.sh:
	echo 'LD_LIBRARY_PATH=$(top_builddir)/src/.libs:$(top_builddir)/dummy/.libs ./testrig 1' > testrig.sh
	chmod +x ./testrig.sh

testnetrigctl.sh:
	echo './testnetrigctl' > testnetrigctl.sh
	chmod +x ./testnetrigctl.sh

testnetrigctllocale.sh:
	echo './testnetrigctl --locale' > testnetrigctllocale.sh
	chmod +x ./testnetrigctllocale.sh

testfreq.sh:
	echo './testfreq' > testfreq.sh
	chmod +x ./testfreq.sh

testbcd.sh:
	echo './testbcd 146520000 10' > testbcd.sh
	chmod +x ./testbcd.sh

testloc.sh:
	echo './testloc EM79UT96LW 5' > testloc.sh
	chmod +x ./testloc.sh

testrigcaps.sh:
	echo './testrigcaps' > testrigcaps.sh
	chmod +x ./testrigcaps.sh

testcache.sh:
	echo './testcache 1' > testcache.sh
	chmod +x ./testcache.sh

testcookie.sh:
	echo './testcookie 1' > testcookie.sh
	chmod +x ./testcookie.sh

testgrid.sh:
	echo './testgrid' > testgrid.sh
	chmod +x ./testgrid.sh

test2038.sh:
	echo 'LD_LIBRARY_PATH=$(top_builddir)/src/.libs:$(top_builddir)/dummy/.libs ./test2038 1' > test2038.sh
	chmod +x ./test2038.sh

CLEANFILES = \
	build-w32.sh \
	build-w64-jtsdk.sh \
	build-w64.sh \
	rigmatrix \
	rigtestlibusb \
	tuner_control.log \
	$(GENERATED_TEST_WRAPPERS)
