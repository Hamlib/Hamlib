noinst_LTLIBRARIES += rigs/aor/libhamlib-aor.la
rigs_aor_libhamlib_aor_la_SOURCES = \
    rigs/aor/ar8200.c rigs/aor/ar8000.c rigs/aor/ar5000.c rigs/aor/ar3000.c \
    rigs/aor/ar7030.c rigs/aor/ar3030.c rigs/aor/ar2700.c rigs/aor/ar8600.c \
    rigs/aor/ar7030p.c rigs/aor/ar7030p.h rigs/aor/ar7030p_utils.c rigs/aor/sr2200.c \
    rigs/aor/aor.c rigs/aor/aor.h
rigs_aor_libhamlib_aor_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/aor -I$(top_srcdir)/rigs/aor $(AM_CPPFLAGS)

EXTRA_DIST += rigs/aor/README.aor rigs/aor/README.ar5000 rigs/aor/README.ar7030
