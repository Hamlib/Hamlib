
AUTOMAKE_OPTIONS = dejagnu

#DEJATOOL = rigctl testbcd
RUNTESTDEFAULTFLAGS = --tool rigctl --srcdir $$srcdir/testsuite RIGCTL=`pwd`/rigctl TESTBCD=`pwd`/testbcd

DISTCLEANFILES = rigctl.log rigctl.sum testbcd.log testbcd.sum

bin_PROGRAMS = rigctl rotctl
man_MANS = rigctl.1 rotctl.1

check_PROGRAMS = dumpmem testrig testtrn testbcd testfreq listrigs testloc @RIGMATRIX@

rigctl_SOURCES = rigctl.c dumpcaps.c

EXTRA_PROGRAMS = rigmatrix

# all the programs need this
LDADD = ../src/libhamlib.la ../lib/libmisc.la
DEPENDENCIES = ../src/libhamlib.la

## Linker options
listrigs_LDFLAGS = @BACKENDLNK@
dumpmem_LDFLAGS = @BACKENDLNK@
testrig_LDFLAGS = @BACKENDLNK@
testtrn_LDFLAGS = @BACKENDLNK@
testfreq_LDFLAGS = @BACKENDLNK@
rigctl_LDFLAGS = @BACKENDLNK@
rotctl_LDFLAGS = @ROT_BACKENDLNK@
 
# temporary hack
testbcd_LDFLAGS = -dlpreopen self
testloc_LDFLAGS = -dlpreopen self
 
# rigmatrix needs also libgd
rigmatrix_LDFLAGS = -lgd -lz @BACKENDLNK@

## Dependencies

dumpmem_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
testrig_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
testtrn_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
listrigs_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
rigctl_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
rotctl_DEPENDENCIES = $(DEPENDENCIES) @ROT_BACKENDEPS@
rigmatrix_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@



rigmatrix.html: rigmatrix_head.html rigmatrix listrigs
	mkdir -p html
	( cd html && cat ../rigmatrix_head.html && ../rigmatrix ) > html/rigmatrix.html
	for f in `./listrigs | tail +2 | cut -f1` ; do ( ./rigctl -m $$f -u > html/model$$f.txt || exit 0 ) ; done

EXTRA_DIST = rigmatrix_head.html $(man_MANS)

