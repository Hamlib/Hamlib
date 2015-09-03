# Current Autotools documentation suggests that DejaGNU is obsolete
# and replaced by Autotest.  TODO: implement Autotest

# AUTOMAKE_OPTIONS = dejagnu
# DEJATOOL = testfreq testbcd testloc rigctl

DISTCLEANFILES = rigctl.log rigctl.sum testbcd.log testbcd.sum

bin_PROGRAMS = rigctl rigmem rigswr rigsmtr rotctl rigctld rotctld

man_MANS = rigctl.1 rigmem.1 rigswr.1 rigsmtr.1 rotctl.1 rigctld.8 rotctld.8

check_PROGRAMS = dumpmem testrig testtrn testbcd testfreq listrigs testloc rig_bench

RIGCOMMONSRC = rigctl_parse.c rigctl_parse.h dumpcaps.c sprintflst.c sprintflst.h uthash.h
ROTCOMMONSRC = rotctl_parse.c rotctl_parse.h dumpcaps_rot.c uthash.h

rigctl_SOURCES = rigctl.c $(RIGCOMMONSRC)
rigctld_SOURCES = rigctld.c $(RIGCOMMONSRC)
rotctl_SOURCES = rotctl.c $(ROTCOMMONSRC)
rotctld_SOURCES = rotctld.c $(ROTCOMMONSRC)
rigswr_SOURCES = rigswr.c
rigsmtr_SOURCES = rigsmtr.c
rigmem_SOURCES = rigmem.c memsave.c memload.c memcsv.c sprintflst.c sprintflst.h


# all the programs need this
LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la

rigmem_CFLAGS = $(AM_CFLAGS) $(LIBXML2_CFLAGS)
rigctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
rotctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)

rigctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigmem_LDADD = $(LIBXML2_LIBS) $(LDADD)

# Linker options
rigctl_LDFLAGS = $(WINEXELDFLAGS)
rigswr_LDFLAGS = $(WINEXELDFLAGS)
rigsmtr_LDFLAGS = $(WINEXELDFLAGS)
rigmem_LDFLAGS = $(WINEXELDFLAGS)
rotctl_LDFLAGS = $(WINEXELDFLAGS)
rigctld_LDFLAGS = $(WINEXELDFLAGS)
rotctld_LDFLAGS = $(WINEXELDFLAGS)


if HTML_MATRIX
EXTRA_PROGRAMS = rigmatrix
# rigmatrix needs libgd
rigmatrix_LDFLAGS = -lgd -lz
rigmatrix.html: rigmatrix_head.html rigmatrix listrigs
	mkdir -p sup-info/support
	( cat $(srcdir)/rigmatrix_head.html && cd sup-info && ../rigmatrix ) > sup-info/rigmatrix.html
	for f in `./listrigs | tail -n +2 | cut -f1` ; do ( ./rigctl -m $$f -u > sup-info/support/model$$f.txt || exit 0 ) ; done
	./rigctl -l |sort -n | $(srcdir)/rig_split_lst.awk -v lst_dir="sup-info"
endif


EXTRA_DIST = rigmatrix_head.html rig_split_lst.awk $(man_MANS) testctld.pl testrotctld.pl

# Support 'make check' target for simple tests
check_SCRIPTS = testrig.sh testfreq.sh testbcd.sh testloc.sh

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


CLEANFILES = testrig.sh testfreq.sh testbcd.sh testloc.sh
