noinst_PROGRAMS = dumpcaps dumpmem testcaps testrig testtrn testbcd testfreq listrigs @RIGMATRIX@
bin_PROGRAMS = rigctl
EXTRA_PROGRAMS = rigmatrix

# all the programs need this
LDADD = ../src/libhamlib.la
DEPENDENCIES = ../src/libhamlib.la

BACKENDLNK = "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../yaesu/libhamlib-yaesu.la "-dlopen" ../kenwood/libhamlib-kenwood.la "-dlopen" ../aor/libhamlib-aor.la "-dlopen" ../dummy/libhamlib-dummy.la "-dlopen" ../pcr/libhamlib-pcr.la "-dlopen" ../alinco/libhamlib-alinco.la @WINRADIOLNK@
BACKENDEPS = ../icom/libhamlib-icom.la ../yaesu/libhamlib-yaesu.la ../kenwood/libhamlib-kenwood.la ../aor/libhamlib-aor.la ../dummy/libhamlib-dummy.la ../pcr/libhamlib-pcr.la ../alinco/libhamlib-alinco.la @WINRADIODEPS@

## Linker options
listrigs_LDADD = $(BACKENDLNK) $(LDADD)
dumpcaps_LDADD = $(BACKENDLNK) $(LDADD)
dumpmem_LDADD = $(BACKENDLNK) $(LDADD)
testcaps_LDADD = $(BACKENDLNK) $(LDADD)
testrig_LDADD = $(BACKENDLNK) $(LDADD)
testtrn_LDADD = $(BACKENDLNK) $(LDADD)
rigctl_LDADD = $(BACKENDLNK) $(LDADD) ../lib/libmisc.a
# rigmatrix needs also libgd
rigmatrix_LDADD = -lgd $(BACKENDLNK) $(LDADD)

## Dependencies

dumpcaps_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
dumpmem_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
testcaps_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
testrig_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
testtrn_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
listrigs_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
rigctl_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS) ../lib/libmisc.a
rigmatrix_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)

## Source list
testrig_SOURCES = testrig.c
testtrn_SOURCES = testtrn.c
testbcd_SOURCES = testbcd.c
testfreq_SOURCES = testfreq.c
dumpcaps_SOURCES = dumpcaps.c
dumpmem_SOURCES = dumpmem.c
testcaps_SOURCES = testcaps.c
listrigs_SOURCES = listrigs.c
rigctl_SOURCES = rigctl.c
rigmatrix_SOURCES = rigmatrix.c

rigmatrix.html: rigmatrix_head.html rigmatrix
	mkdir -p html
	cd html
	mv rigmatrix.html rigmatrix-old.html
	( cat ../rigmatrix_head.html && ../rigmatrix ) >  rigmatrix.html
	cd ..

noinst_DATA = rigmatrix_head.html
