bin_PROGRAMS = dumpcaps testrig testtrn testbcd listrigs rigctl @RIGMATRIX@
EXTRA_PROGRAMS = rigmatrix

# all the programs need this
LDADD = ../src/libhamlib.la

## Linker options
listrigs_LDADD = ../src/libhamlib.la "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../ft747/libhamlib-ft747.la "-dlopen" ../ft847/libhamlib-ft847.la "-dlopen" ../aor/libhamlib-aor.la
dumpcaps_LDADD = ../src/libhamlib.la "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../ft747/libhamlib-ft747.la "-dlopen" ../ft847/libhamlib-ft847.la "-dlopen" ../aor/libhamlib-aor.la
testrig_LDADD = ../src/libhamlib.la "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../ft747/libhamlib-ft747.la "-dlopen" ../ft847/libhamlib-ft847.la
testtrn_LDADD = ../src/libhamlib.la "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../ft747/libhamlib-ft747.la "-dlopen" ../ft847/libhamlib-ft847.la
rigctl_LDADD = ../src/libhamlib.la "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../ft747/libhamlib-ft747.la "-dlopen" ../ft847/libhamlib-ft847.la
# rigmatrix needs also libgd
rigmatrix_LDADD = -lgd ../src/libhamlib.la "-dlopen" ../icom/libhamlib-icom.la "-dlopen" ../ft747/libhamlib-ft747.la "-dlopen" ../ft847/libhamlib-ft847.la "-dlopen" ../aor/libhamlib-aor.la

## Dependencies
dumpcaps_DEPENDENCIES = ../icom/libhamlib-icom.la ../ft747/libhamlib-ft747.la ../ft847/libhamlib-ft847.la ../aor/libhamlib-aor.la
testrig_DEPENDENCIES = ../icom/libhamlib-icom.la ../ft747/libhamlib-ft747.la ../ft847/libhamlib-ft847.la
testtrn_DEPENDENCIES = ../icom/libhamlib-icom.la ../ft747/libhamlib-ft747.la ../ft847/libhamlib-ft847.la
listrigs_DEPENDENCIES = ../icom/libhamlib-icom.la ../ft747/libhamlib-ft747.la ../ft847/libhamlib-ft847.la ../aor/libhamlib-aor.la
rigctl_DEPENDENCIES = ../icom/libhamlib-icom.la ../ft747/libhamlib-ft747.la ../ft847/libhamlib-ft847.la
rigmatrix_DEPENDENCIES = ../icom/libhamlib-icom.la ../ft747/libhamlib-ft747.la ../ft847/libhamlib-ft847.la ../aor/libhamlib-aor.la

## Source list
testrig_SOURCES = testrig.c
testtrn_SOURCES = testtrn.c
testbcd_SOURCES = testbcd.c
dumpcaps_SOURCES = dumpcaps.c
listrigs_SOURCES = listrigs.c
rigctl_SOURCES = rigctl.c
rigmatrix_SOURCES = rigmatrix.c
