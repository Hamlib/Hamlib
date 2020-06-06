# Current Autotools documentation suggests that DejaGNU is obsolete
# and replaced by Autotest.  TODO: implement Autotest

# AUTOMAKE_OPTIONS = dejagnu
# DEJATOOL = testfreq testbcd testloc rigctl

BUILT_SOURCES = hamlibdatetime.h

DISTCLEANFILES = rigctl.log rigctl.sum testbcd.log testbcd.sum hamlibdatetime.h

bin_PROGRAMS = rigctl rigctld rigmem rigsmtr rigswr rotctl rotctld rigctlcom ampctl ampctld

check_PROGRAMS = dumpmem testrig testtrn testbcd testfreq listrigs testloc rig_bench cachetest cachetest2

RIGCOMMONSRC = rigctl_parse.c rigctl_parse.h dumpcaps.c sprintflst.c sprintflst.h uthash.h hamlibdatetime.h
ROTCOMMONSRC = rotctl_parse.c rotctl_parse.h dumpcaps_rot.c uthash.h hamlibdatetime.h
AMPCOMMONSRC = ampctl_parse.c ampctl_parse.h dumpcaps_amp.c sprintflst.c sprintflst.h uthash.h hamlibdatetime.h

rigctl_SOURCES = rigctl.c $(RIGCOMMONSRC)
rigctld_SOURCES = rigctld.c $(RIGCOMMONSRC)
rigctlcom_SOURCES = rigctlcom.c $(RIGCOMMONSRC)
rotctl_SOURCES = rotctl.c $(ROTCOMMONSRC)
rotctld_SOURCES = rotctld.c $(ROTCOMMONSRC)
ampctl_SOURCES = ampctl.c $(AMPCOMMONSRC)
ampctld_SOURCES = ampctld.c $(AMPCOMMONSRC)
rigswr_SOURCES = rigswr.c
rigsmtr_SOURCES = rigsmtr.c
rigmem_SOURCES = rigmem.c memsave.c memload.c memcsv.c sprintflst.c sprintflst.h

rigctl_CPPFLAGS = -I$(top_srcdir) $(AM_CPPFLAGS)

# all the programs need this
LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS)

rigmem_CFLAGS = $(AM_CFLAGS) $(LIBXML2_CFLAGS)
rigctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
rotctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
ampctld_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
rigctlcom_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)

rigctl_LDADD = $(PTHREAD_LIBS) $(READLINE_LIBS) $(LDADD)
rigctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rotctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
ampctl_LDADD = $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
ampctld_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)
rigmem_LDADD = $(LIBXML2_LIBS) $(LDADD)
rigctlcom_LDADD = $(NET_LIBS) $(PTHREAD_LIBS) $(LDADD) $(READLINE_LIBS)

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


EXTRA_DIST = rigmatrix_head.html rig_split_lst.awk testctld.pl testrotctld.pl \
	hamlibdatetime.h.in

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

# If we have a .git directory then we will generate the hamlibdate.h file.
# if not, then copy the placeholder.
hamlibdatetime.h: FORCE
	echo "SHA=$(SHA)"
	test ! -x $(top_srcdir)/.git || echo "/* This date time is from the last non-merge commit to Hamlib. */" > $(srcdir)/hamlibdatetime.h
	test ! -x $(top_srcdir)/.git || echo "#define HAMLIBDATETIME "\"$$(TZ=UTC git --git-dir=$(top_srcdir)/.git log --no-merges --date='format-local:%a %b %d %H:%M:%S %Y %z SHA=' --format='%cd' -n 1)$$(git --git-dir=$(top_srcdir)/.git log --no-merges | grep commit | head -n 1 | cut -c8-13)\"  >> $(srcdir)/hamlibdatetime.h
	test -f $(srcdir)/hamlibdatetime.h || cp $(srcdir)/hamlibdatetime.h.in $(srcdir)/hamlibdatetime.h

FORCE: ;

CLEANFILES = testrig.sh testfreq.sh testbcd.sh testloc.sh
