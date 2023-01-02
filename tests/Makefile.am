# Current Autotools documentation suggests that DejaGNU is obsolete
# and replaced by Autotest.  TODO: implement Autotest

# AUTOMAKE_OPTIONS = dejagnu
# DEJATOOL = testfreq testbcd testloc rigctl

# For some reason this "if" is not working -- rigtestlibusb is still being included
# Fix for now is to change rigtestlibusb.c instead
if HAVE_LIBUSB
    TESTLIBUSB = rigtestlibusb
else
    TESTLIBUSB =
endif

DISTCLEANFILES = rigctl.log rigctl.sum testbcd.log testbcd.sum

bin_PROGRAMS = rigctl rigctld rigmem rigsmtr rigswr rotctl rotctld rigctlcom ampctl ampctld $(TESTLIBUSB)

#check_PROGRAMS = dumpmem testrig testrigopen testrigcaps testtrn testbcd testfreq listrigs testloc rig_bench testcache cachetest cachetest2 testcookie testgrid testsecurity
check_PROGRAMS = dumpmem testrig testrigopen testrigcaps testtrn testbcd testfreq listrigs testloc rig_bench testcache cachetest cachetest2 testcookie testgrid

RIGCOMMONSRC = rigctl_parse.c rigctl_parse.h dumpcaps.c uthash.h 
ROTCOMMONSRC = rotctl_parse.c rotctl_parse.h dumpcaps_rot.c uthash.h 
AMPCOMMONSRC = ampctl_parse.c ampctl_parse.h dumpcaps_amp.c uthash.h 

rigctl_SOURCES = rigctl.c $(RIGCOMMONSRC)
rigctld_SOURCES = rigctld.c $(RIGCOMMONSRC)
rigctlcom_SOURCES = rigctlcom.c $(RIGCOMMONSRC)
rigctltcp_SOURCES = rigctltcp.c $(RIGCOMMONSRC)
rotctl_SOURCES = rotctl.c $(ROTCOMMONSRC)
rotctld_SOURCES = rotctld.c $(ROTCOMMONSRC)
ampctl_SOURCES = ampctl.c $(AMPCOMMONSRC)
ampctld_SOURCES = ampctld.c $(AMPCOMMONSRC)
rigswr_SOURCES = rigswr.c
rigsmtr_SOURCES = rigsmtr.c
rigmem_SOURCES = rigmem.c memsave.c memload.c memcsv.c
if HAVE_LIBUSB
    rigtestlibusb_SOURCES = rigtestlibusb.c
endif

# include generated include files ahead of any in sources
rigctl_CPPFLAGS = -I$(top_builddir)/tests -I$(top_builddir)/src -I$(srcdir) -I$(top_builddir)/security $(AM_CPPFLAGS) 

# all the programs need this
LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS)

rigmem_CFLAGS = $(AM_CFLAGS) $(LIBXML2_CFLAGS) -I$(top_builddir)/src
rigctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src -I$(top_builddir)/security
rotctl_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
rotctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
ampctl_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
ampctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src
rigctlcom_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/security
rigctltcp_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/security
if HAVE_LIBUSB
    rigtestlibusb_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) $(LIBUSB_CFLAGS)
endif
#testsecurity_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_builddir)/src -I$(top_builddir)/security

rigctl_LDADD = $(PTHREAD_LIBS) $(READLINE_LIBS) $(LDADD)
rigctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
ampctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
ampctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigmem_LDADD = $(LIBXML2_LIBS) $(LDADD)
rigctlcom_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigctltcp_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
if HAVE_LIBUSB
    rigtestlibusb_LDADD = $(LIBUSB_LIBS)
endif

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
if HAVE_LIBUSB
    rigtestlibusb_LDFLAGS = $(WINEXELDFLAGS)
endif


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


EXTRA_DIST = rigmatrix_head.html rig_split_lst.awk testctld.pl testrotctld.pl

# Support 'make check' target for simple tests
check_SCRIPTS = testrig.sh testfreq.sh testbcd.sh testloc.sh testrigcaps.sh testcache.sh testcookie.sh testgrid.sh

TESTS = $(check_SCRIPTS)


testrig.sh:
	echo 'LD_LIBRARY_PATH=$(top_builddir)/src/.libs:$(top_builddir)/dummy/.libs ./testrig 1' > testrig.sh
	chmod +x ./testrig.sh

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

CLEANFILES = testrig.sh testfreq.sh testbcd.sh testloc.sh testrigcaps.sh testcache.sh testcookie.sh rigtestlibusb build-w32.sh build-w64.sh build-w64-jtsdk.sh testgrid.sh testrigcaps.sh
