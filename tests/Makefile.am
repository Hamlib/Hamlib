bin_PROGRAMS = dumpcaps testrig testtrn testbcd listrigs rigctl @RIGMATRIX@
EXTRA_PROGRAMS = rigmatrix

# all the programs need this
LDADD = ../src/libhamlib.la

BACKENDLNK = "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../yaesu/libhamlib-ft747.la "-dlopen" ../yaesu/libhamlib-ft847.la "-dlopen" ../kenwood/libhamlib-kenwood.la "-dlopen" ../aor/libhamlib-aor.la
BACKENDEPS = ../icom/libhamlib-icom.la ../yaesu/libhamlib-ft747.la ../yaesu/libhamlib-ft847.la ../kenwood/libhamlib-kenwood.la ../aor/libhamlib-aor.la

## Linker options
listrigs_LDADD = ../src/libhamlib.la $(BACKENDLNK)
dumpcaps_LDADD = ../src/libhamlib.la $(BACKENDLNK)
testrig_LDADD = ../src/libhamlib.la $(BACKENDLNK)
testtrn_LDADD = ../src/libhamlib.la $(BACKENDLNK)
rigctl_LDADD = ../src/libhamlib.la $(BACKENDLNK)
# rigmatrix needs also libgd
rigmatrix_LDADD = -lgd ../src/libhamlib.la $(BACKENDLNK)

## Dependencies

dumpcaps_DEPENDENCIES = $(BACKENDEPS)
testrig_DEPENDENCIES = $(BACKENDEPS)
testtrn_DEPENDENCIES = $(BACKENDEPS)
listrigs_DEPENDENCIES = $(BACKENDEPS)
rigctl_DEPENDENCIES = $(BACKENDEPS)
rigmatrix_DEPENDENCIES = $(BACKENDEPS)

## Source list
testrig_SOURCES = testrig.c
testtrn_SOURCES = testtrn.c
testbcd_SOURCES = testbcd.c
dumpcaps_SOURCES = dumpcaps.c
listrigs_SOURCES = listrigs.c
rigctl_SOURCES = rigctl.c
rigmatrix_SOURCES = rigmatrix.c
