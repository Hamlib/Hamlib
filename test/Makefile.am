# Unit tests for Hamlib using acutest (single-header test framework).
# Run via: make check

AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/hamlib -I$(top_srcdir)/src -I$(top_builddir)/include -I$(srcdir) -I$(top_srcdir)/lib

LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/lib/libmisc.la $(DL_LIBS) -lm

check_PROGRAMS = test_stream_ringbuf test_stream_convert test_stream_codec test_stream_time test_stream_account test_stream_api test_dummy_stream test_rigctld_stream test_rigstreamtest test_rigctld_commands test_netrigctl_stream

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
test_rigctld_commands_CPPFLAGS = $(AM_CPPFLAGS) -DIN_HAMLIB -I$(top_srcdir)/tests -I$(top_builddir)/src -I$(top_builddir)/security
test_rigctld_commands_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
test_rigctld_commands_LDADD = $(LDADD) $(PTHREAD_LIBS) $(READLINE_LIBS) $(NET_LIBS)

# Manual system test against a real Icom LAN radio; built but never run by
# `make check` (needs hardware and credentials).

TESTS = $(check_PROGRAMS)

EXTRA_DIST = acutest.h

$(top_builddir)/src/libhamlib.la:
	$(MAKE) -C $(top_builddir)/src/ libhamlib.la

$(top_builddir)/lib/libmisc.la:
	$(MAKE) -C $(top_builddir)/lib/ libmisc.la

$(top_builddir)/simulators/simflex$(EXEEXT):
	$(MAKE) -C $(top_builddir)/simulators simflex$(EXEEXT)
