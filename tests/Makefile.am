
bin_PROGRAMS = rigctl
man_MANS = rigctl.1

noinst_PROGRAMS = dumpcaps dumpmem testcaps testrig testtrn testbcd testfreq listrigs testcpp testloc @RIGMATRIX@

EXTRA_PROGRAMS = rigmatrix

# all the programs need this
LDADD = ../src/libhamlib.la
DEPENDENCIES = ../src/libhamlib.la

## Linker options
listrigs_LDFLAGS = @BACKENDLNK@
dumpcaps_LDFLAGS = @BACKENDLNK@
dumpmem_LDFLAGS = @BACKENDLNK@
testcaps_LDFLAGS = @BACKENDLNK@
testrig_LDFLAGS = @BACKENDLNK@
testtrn_LDFLAGS = @BACKENDLNK@
testfreq_LDFLAGS = @BACKENDLNK@
rigctl_LDFLAGS = @BACKENDLNK@
 
# temporary hack
testbcd_LDFLAGS = -dlpreopen self
testloc_LDFLAGS = -dlpreopen self
 
# rigmatrix needs also libgd
rigmatrix_LDFLAGS = -lgd -lz @BACKENDLNK@
testcpp_LDADD = ../c++/libhamlib++.la
testcpp_LDFLAGS = @BACKENDLNK@

## Dependencies

dumpcaps_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
dumpmem_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
testcaps_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
testrig_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
testtrn_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
listrigs_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
rigctl_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
rigmatrix_DEPENDENCIES = $(DEPENDENCIES) @BACKENDEPS@
testcpp_DEPENDENCIES = ../c++/libhamlib++.la @BACKENDEPS@

## Non implicit sources
testcpp_SOURCES = testcpp.cc


rigmatrix.html: rigmatrix_head.html rigmatrix listrigs dumpcaps
	mkdir -p html
	( cd html && cat ../rigmatrix_head.html && ../rigmatrix ) > html/rigmatrix.html
	for f in `./listrigs | tail +2 | cut -f1` ; do ./dumpcaps $$f > html/model$$f.txt ; done

EXTRA_DIST = rigmatrix_head.html $(man_MANS)

