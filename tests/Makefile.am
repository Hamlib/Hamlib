
bin_PROGRAMS = rigctl
man_MANS = rigctl.1

noinst_PROGRAMS = dumpcaps dumpmem testcaps testrig testtrn testbcd testfreq listrigs testcpp @RIGMATRIX@

EXTRA_PROGRAMS = rigmatrix

# all the programs need this
LDADD = ../src/libhamlib.la ../lib/libmisc.a
DEPENDENCIES = ../src/libhamlib.la

## The quotes around -dlopen below fool automake into accepting it
BACKENDLNK = -dlopen self -dlopen ../icom/libhamlib-icom.la -dlopen ../yaesu/libhamlib-yaesu.la -dlopen ../kenwood/libhamlib-kenwood.la -dlopen ../aor/libhamlib-aor.la -dlopen ../dummy/libhamlib-dummy.la -dlopen ../pcr/libhamlib-pcr.la -dlopen ../alinco/libhamlib-alinco.la -dlopen ../uniden/libhamlib-uniden.la -dlopen ../tentec/libhamlib-tentec.la -dlopen ../winradio/libhamlib-winradio.la -dlopen ../kachina/libhamlib-kachina.la -dlopen ../rpc/libhamlib-rpcrig.la
BACKENDEPS = ../icom/libhamlib-icom.la ../yaesu/libhamlib-yaesu.la ../kenwood/libhamlib-kenwood.la ../aor/libhamlib-aor.la ../dummy/libhamlib-dummy.la ../pcr/libhamlib-pcr.la ../alinco/libhamlib-alinco.la ../uniden/libhamlib-uniden.la ../tentec/libhamlib-tentec.la ../winradio/libhamlib-winradio.la ../kachina/libhamlib-kachina.la ../rpc/libhamlib-rpcrig.la

## Linker options
listrigs_LDFLAGS = $(BACKENDLNK)
dumpcaps_LDFLAGS = $(BACKENDLNK)
dumpmem_LDFLAGS = $(BACKENDLNK)
testcaps_LDFLAGS = $(BACKENDLNK)
testrig_LDFLAGS = $(BACKENDLNK)
testtrn_LDFLAGS = $(BACKENDLNK)
testbcd_LDFLAGS = $(BACKENDLNK)
testfreq_LDFLAGS = $(BACKENDLNK)
rigctl_LDFLAGS = $(BACKENDLNK)
# rigmatrix needs also libgd
rigmatrix_LDFLAGS = -lgd $(BACKENDLNK)
testcpp_LDADD = ../c++/libhamlib++.la $(LDADD)
testcpp_LDFLAGS = $(BACKENDLNK)

## Dependencies

dumpcaps_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
dumpmem_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
testcaps_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
testrig_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
testtrn_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
listrigs_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
rigctl_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS) ../lib/libmisc.a
rigmatrix_DEPENDENCIES = $(DEPENDENCIES) $(BACKENDEPS)
testcpp_DEPENDENCIES = ../c++/libhamlib++.la $(BACKENDEPS)

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
testcpp_SOURCES = testcpp.cc

rigmatrix.html: rigmatrix_head.html rigmatrix listrigs dumpcaps
	mkdir -p html
	( cd html && cat ../rigmatrix_head.html && ../rigmatrix ) > html/rigmatrix.html
	for f in `./listrigs | tail +2 | cut -f1` ; do ./dumpcaps $$f > html/model$$f.txt ; done

EXTRA_DIST = rigmatrix_head.html $(man_MANS)

