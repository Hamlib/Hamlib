noinst_LTLIBRARIES += rigs/dummy/libhamlib-dummy.la
rigs_dummy_libhamlib_dummy_la_SOURCES = \
    rigs/dummy/dummy_common.c rigs/dummy/dummy_common.h rigs/dummy/dummy.c \
    rigs/dummy/dummy.h rigs/dummy/dummy_stream.c rigs/dummy/dummy_stream.h \
    rigs/dummy/rot_dummy.c rigs/dummy/rot_dummy.h rigs/dummy/rot_pstrotator.c \
    rigs/dummy/rot_pstrotator.h rigs/dummy/netrigctl.c rigs/dummy/netrotctl.c \
    rigs/dummy/flrig.c rigs/dummy/flrig.h rigs/dummy/trxmanager.c rigs/dummy/trxmanager.h \
    rigs/dummy/amp_dummy.c rigs/dummy/amp_dummy.h rigs/dummy/netampctl.c \
    rigs/dummy/tci1x.c rigs/dummy/aclog.c rigs/dummy/sdrsharp.c rigs/dummy/quisk.c \
    rigs/dummy/gqrx.c
rigs_dummy_libhamlib_dummy_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/dummy -I$(top_srcdir)/rigs/dummy $(AM_CPPFLAGS)
