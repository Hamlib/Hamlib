rigs_icom_sources = \
    rigs/icom/icom.c rigs/icom/icom.h rigs/icom/icom_defs.h rigs/icom/icom_alt_agc.c \
    rigs/icom/icom_alt_agc.h rigs/icom/frame.c rigs/icom/frame.h rigs/icom/ic706.c \
    rigs/icom/icr8500.c rigs/icom/ic735.c rigs/icom/ic775.c rigs/icom/ic756.c \
    rigs/icom/ic275.c rigs/icom/ic475.c rigs/icom/ic1275.c rigs/icom/ic820h.c \
    rigs/icom/ic821h.c rigs/icom/icr7000.c rigs/icom/ic910.c rigs/icom/ic9100.c \
    rigs/icom/ic970.c rigs/icom/ic725.c rigs/icom/ic737.c rigs/icom/ic718.c \
    rigs/icom/os535.c rigs/icom/os456.c rigs/icom/omni.c rigs/icom/delta2.c \
    rigs/icom/ic92d.c rigs/icom/ic736.c rigs/icom/ic738.c rigs/icom/ic7410.c \
    rigs/icom/ic746.c rigs/icom/ic703.c rigs/icom/ic726.c rigs/icom/ic271.c \
    rigs/icom/ic765.c rigs/icom/ic781.c rigs/icom/ic471.c rigs/icom/icr9000.c \
    rigs/icom/icr9500.c rigs/icom/icr10.c rigs/icom/icr20.c rigs/icom/icr30.c \
    rigs/icom/icr6.c rigs/icom/icr71.c rigs/icom/icr72.c rigs/icom/icr75.c \
    rigs/icom/icrx7.c rigs/icom/icr8600.c rigs/icom/id1.c rigs/icom/id31.c \
    rigs/icom/id51.c rigs/icom/id52plus.c rigs/icom/id4100.c rigs/icom/id5100.c \
    rigs/icom/perseus.c rigs/icom/ic2730.c rigs/icom/ic707.c rigs/icom/ic728.c \
    rigs/icom/ic751.c rigs/icom/ic761.c rigs/icom/ic78.c rigs/icom/ic7800.c \
    rigs/icom/ic785x.c rigs/icom/ic7000.c rigs/icom/ic7100.c rigs/icom/ic7200.c \
    rigs/icom/ic7300.c rigs/icom/ic9700.c rigs/icom/ic705.c rigs/icom/ic905.c \
    rigs/icom/ic7600.c rigs/icom/ic7610.c rigs/icom/ic7700.c rigs/icom/ic7760.c \
    rigs/icom/icf8101.c rigs/icom/optoscan.c rigs/icom/optoscan.h rigs/icom/xiegu.c \
    rigs/icom/level_gran_icom.h rigs/icom/network_proto.c rigs/icom/network_proto.h \
    rigs/icom/network_seqbuf.c rigs/icom/network_seqbuf.h rigs/icom/network_session.c \
    rigs/icom/network_session.h rigs/icom/network_conf.c rigs/icom/network_conf.h \
    rigs/icom/network_utils.c rigs/icom/network_utils.h rigs/icom/icom_network.c \
    rigs/icom/icom_network.h rigs/icom/ic7610net.c rigs/icom/ic9700net.c \
    rigs/icom/ic705net.c rigs/icom/ic905net.c rigs/icom/ic7760net.c \
    rigs/icom/ic7300mk2net.c

noinst_LTLIBRARIES += rigs/icom/libhamlib-icom.la
rigs_icom_libhamlib_icom_la_SOURCES = $(rigs_icom_sources)

rigs_icom_libhamlib_icom_la_CPPFLAGS = \
    -I$(top_builddir)/rigs/icom -I$(top_srcdir)/rigs/icom $(AM_CPPFLAGS)

EXTRA_DIST += rigs/icom/ICOM.md
