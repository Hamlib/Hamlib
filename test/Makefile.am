# Unit tests and integration tests for Hamlib, run with the acutest
# single-header test framework. This directory is the home for ALL
# acutest-based suites, whatever subsystem they cover.
# Run via: make check
#
# Adding a new suite: create test_<name>.c (include "acutest.h" and
# "test_debug.h"), add it to check_PROGRAMS and its _SOURCES/_LDADD
# below — TESTS = $(check_PROGRAMS) and the CI workflows pick it up
# automatically from there.
#
# The (default) parallel automake harness captures each suite's full
# output — acutest per-case results plus library/daemon diagnostics —
# into <suite>.log, prints one PASS/FAIL line per suite on the console,
# and dumps the log of any failing suite. Suites run at RIG_DEBUG_WARN;
# set RIG_TEST_DEBUG=1 for TRACE-level logs (see test_debug.h).

# -DIN_HAMLIB: the suites exercise library internals directly (they
# include src/ headers and call internal helpers), so they compile as
# internal code — HL_PRIVATE fields carry no deprecation warnings here,
# matching the library's own AM_CPPFLAGS from configure.ac.
# -D__USE_MINGW_ANSI_STDIO=1: mingw-w64 substitutes its C99-compliant
# printf family, so %zu in test messages formats correctly on Windows
# (msvcrt's printf does not know %z). Inert on other platforms.
# -DWIN32_LEAN_AND_MEAN: keeps acutest.h's <windows.h> from dragging in
# winsock.h ahead of the winsock2.h the library headers use.
AM_CPPFLAGS = -DIN_HAMLIB -D__USE_MINGW_ANSI_STDIO=1 -DWIN32_LEAN_AND_MEAN -I$(top_srcdir)/include -I$(top_srcdir)/include/hamlib -I$(top_srcdir)/src -I$(top_builddir)/include -I$(srcdir) -I$(top_srcdir)/lib

LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS) -lm

check_PROGRAMS = test_stream_ringbuf test_stream_convert test_stream_codec test_stream_time test_stream_account test_stream_api test_dummy_stream test_rigctld_stream test_rigstreamtest test_rigctld_commands test_netrigctl_stream test_icom_network_proto test_icom_network_seqbuf test_icom_network_session test_icom_network_conf test_icom_network_stream test_icom_network_reconnect

test_stream_codec_SOURCES = test_stream_codec.c
test_stream_codec_LDADD = $(LDADD)

test_stream_ringbuf_SOURCES = test_stream_ringbuf.c
test_stream_ringbuf_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_stream_ringbuf_LDADD = $(LDADD) $(PTHREAD_LIBS)

test_stream_convert_SOURCES = test_stream_convert.c
test_stream_convert_LDADD = $(LDADD)

test_stream_time_SOURCES = test_stream_time.c
test_stream_time_LDADD = $(LDADD)

test_stream_account_SOURCES = test_stream_account.c
test_stream_account_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_stream_account_LDADD = $(LDADD) $(PTHREAD_LIBS)

test_stream_api_SOURCES = test_stream_api.c
test_stream_api_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_stream_api_LDADD = $(LDADD) $(PTHREAD_LIBS)

test_dummy_stream_SOURCES = test_dummy_stream.c
test_dummy_stream_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_dummy_stream_LDADD = $(LDADD) $(PTHREAD_LIBS)

test_rigctld_stream_SOURCES = test_rigctld_stream.c $(top_srcdir)/tests/rigctld_stream.c
test_rigctld_stream_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/tests
test_rigctld_stream_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_rigctld_stream_LDADD = $(LDADD) $(PTHREAD_LIBS) $(NET_LIBS)

test_rigstreamtest_SOURCES = test_rigstreamtest.c $(top_srcdir)/tests/rigstreamtest_util.c
test_rigstreamtest_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/tests
test_rigstreamtest_LDADD = $(LDADD)

test_netrigctl_stream_SOURCES = test_netrigctl_stream.c
test_netrigctl_stream_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_netrigctl_stream_LDADD = $(LDADD) $(PTHREAD_LIBS) $(NET_LIBS)

test_rigctld_commands_SOURCES = test_rigctld_commands.c \
    $(top_srcdir)/tests/rigctl_parse.c \
    $(top_srcdir)/tests/rigctld_stream.c \
    $(top_srcdir)/tests/dumpcaps.c \
    $(top_srcdir)/tests/dumpstate.c \
    $(top_srcdir)/tests/rig_tests.c
test_rigctld_commands_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/tests -I$(top_builddir)/src -I$(top_builddir)/security
test_rigctld_commands_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_rigctld_commands_LDADD = $(LDADD) $(PTHREAD_LIBS) $(READLINE_LIBS) $(NET_LIBS)

test_icom_network_proto_SOURCES = test_icom_network_proto.c
test_icom_network_proto_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/icom
test_icom_network_proto_LDADD = $(LDADD)

test_icom_network_seqbuf_SOURCES = test_icom_network_seqbuf.c
test_icom_network_seqbuf_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/icom
test_icom_network_seqbuf_LDADD = $(LDADD)

test_icom_network_session_SOURCES = test_icom_network_session.c \
	icom_network_mock.c icom_network_mock.h

test_icom_network_stream_SOURCES = test_icom_network_stream.c \
	icom_network_mock.c icom_network_mock.h
test_icom_network_stream_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/icom
test_icom_network_stream_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_icom_network_stream_LDADD = $(LDADD) $(PTHREAD_LIBS) $(NET_LIBS)
test_icom_network_session_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/icom
test_icom_network_session_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_icom_network_session_LDADD = $(LDADD) $(PTHREAD_LIBS) $(NET_LIBS)

test_icom_network_conf_SOURCES = test_icom_network_conf.c
test_icom_network_conf_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/icom
test_icom_network_conf_LDADD = $(LDADD) $(PTHREAD_LIBS) $(NET_LIBS)

test_icom_network_reconnect_SOURCES = test_icom_network_reconnect.c \
	icom_network_mock.c icom_network_mock.h
test_icom_network_reconnect_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/rigs/icom
test_icom_network_reconnect_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_icom_network_reconnect_LDADD = $(LDADD) $(PTHREAD_LIBS) $(NET_LIBS)

TESTS = $(check_PROGRAMS)

EXTRA_DIST = acutest.h test_debug.h

# Convenience target for CI cross builds: compile every suite without
# running it (the .exe files are executed under Wine in a separate step).
checkprogs: $(check_PROGRAMS)
.PHONY: checkprogs

$(top_builddir)/src/libhamlib.la:
	$(MAKE) -C $(top_builddir)/src/ libhamlib.la

$(top_builddir)/lib/libmisc.la:
	$(MAKE) -C $(top_builddir)/lib/ libmisc.la

$(top_builddir)/simulators/simflex$(EXEEXT):
	$(MAKE) -C $(top_builddir)/simulators simflex$(EXEEXT)
