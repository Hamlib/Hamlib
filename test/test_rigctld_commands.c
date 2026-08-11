/*
 *  Hamlib rigctld streaming command tests
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* Integration tests for rigctld streaming command handlers. */
/* Exercises stream_caps, stream_open, stream_close via rigctl_parse(). */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
/* Socket headers come from stream_proto.h (via rigctld_stream.h), which picks
 * the right set for the host; do not include them directly. */
#include "../tests/rigctl_parse.h"
#include "../tests/rigctld_stream.h"
#include "../tests/rigctld_client.h"
#include <hamlib/rig.h>
#include "../src/stream.h"
#include "../src/stream_proto.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>


/* True while fd refers to an open socket. SO_TYPE succeeds only on an
 * open socket, and unlike fcntl(F_GETFD) it is also valid on Winsock. */
static int socket_is_open(int fd)
{
    int so_type = 0;
    socklen_t so_len = sizeof(so_type);
    return getsockopt(fd, SOL_SOCKET, SO_TYPE,
                      (char *)&so_type, &so_len) == 0;
}


/* A data packet may carry the embedded time block (CTRL_TIME); any other
 * control bit marks a non-data frame. */
static int pkt_is_data(const struct rig_stream_packet_header *hdr)
{
    return (hdr->control & ~RIG_STREAM_CTRL_TIME) == 0;
}

/* Offset of sample data within the packet (skips the time block) */
static size_t pkt_data_offset(const struct rig_stream_packet_header *hdr)
{
    return RIG_STREAM_HEADER_SIZE
           + ((hdr->control & RIG_STREAM_CTRL_TIME)
              ? RIG_STREAM_TIME_BLOCK_SIZE : 0);
}

/* Bytes of sample data in the payload (excludes the time block) */
static size_t pkt_data_len(const struct rig_stream_packet_header *hdr)
{
    size_t skip = (hdr->control & RIG_STREAM_CTRL_TIME)
                  ? RIG_STREAM_TIME_BLOCK_SIZE : 0;
    return hdr->payload_len > skip ? hdr->payload_len - skip : 0;
}

/* Globals required by rigctl_parse.c */
int lock_mode = 0;
powerstat_t rig_powerstat = RIG_POWER_ON;
extern int is_rigctld;


/*
 * Expected stream_caps output lines from the dummy backend.
 * Format order follows all_formats[] in rigctld_stream.c.
 * These must match dummy.c stream_caps declarations exactly.
 */
/* The advertised rates are the frontend-derived EFFECTIVE list. With the
 * resampler built it is native ∪ curated standards ∪ integer divisions
 * (÷2..÷10, exact results) of each native rate, sorted ascending; without
 * it, exactly the native list. See HAMLIB_STREAMING_FORMAT_CONVERSION.md
 * §3.4 — these literals double as an integration test of that derivation
 * against the dummy backend's native rates. */
#define EXPECTED_AUDIO_NATIVE_RATES "8000,16000,24000,48000,96000"
#define EXPECTED_IQ_NATIVE_RATES    "24000,48000,96000,192000"

#ifdef HAVE_SAMPLERATE
#define EXPECTED_AUDIO_RATES \
    "800,1000,1600,2000,2400,3000,3200,4000,4800,6000,8000,9600,11025," \
    "12000,16000,19200,22050,24000,32000,44100,48000,96000"
#define EXPECTED_IQ_RATES \
    "2400,3000,4000,4800,6000,8000,9600,11025,12000,16000,19200,22050," \
    "24000,32000,38400,44100,48000,64000,96000,192000"
#else
/* Without the resampler the effective sets equal the native sets. */
#define EXPECTED_AUDIO_RATES EXPECTED_AUDIO_NATIVE_RATES
#define EXPECTED_IQ_RATES    EXPECTED_IQ_NATIVE_RATES
#endif

/* AUDIO_RX carries the dummy's fabricated OPUS test codec: it appears in
 * both views (declared native, passed through derivation untouched). */
#define EXPECTED_AUDIO_RX_LINE \
    "type=AUDIO_RX formats=PCM_S8,PCM_U8,PCM_S16,PCM_F32,OPUS " \
    "rates=" EXPECTED_AUDIO_RATES " channels=1-2 max=4 " \
    "native_formats=PCM_F32,OPUS " \
    "native_rates=" EXPECTED_AUDIO_NATIVE_RATES " native_channels=1-2"
#define EXPECTED_AUDIO_TX_LINE \
    "type=AUDIO_TX formats=PCM_S8,PCM_U8,PCM_S16,PCM_F32,OPUS " \
    "rates=" EXPECTED_AUDIO_RATES " channels=1-2 max=4 " \
    "native_formats=PCM_F32,OPUS " \
    "native_rates=" EXPECTED_AUDIO_NATIVE_RATES " native_channels=1-2"
#define EXPECTED_IQ_RX_LINE \
    "type=IQ_RX formats=IQ_CS8,IQ_CU8,IQ_CS16,IQ_CF32 " \
    "rates=" EXPECTED_IQ_RATES " channels=1-4 max=4 " \
    "native_formats=IQ_CF32 " \
    "native_rates=" EXPECTED_IQ_NATIVE_RATES " native_channels=1-4"
#define EXPECTED_IQ_TX_LINE \
    "type=IQ_TX formats=IQ_CS8,IQ_CU8,IQ_CS16,IQ_CF32 " \
    "rates=" EXPECTED_IQ_RATES " channels=1-4 max=4 " \
    "native_formats=IQ_CF32 " \
    "native_rates=" EXPECTED_IQ_NATIVE_RATES " native_channels=1-4"


/* --- Test helpers --- */

static RIG *test_rig_setup(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);

    if (!rig)
    {
        return NULL;
    }

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        return NULL;
    }

    /* Tests exercise rigctld stream commands */
    is_rigctld = 1;

    return rig;
}


static void test_rig_teardown(RIG *rig)
{
    if (rig)
    {
        rig_close(rig);
        rig_cleanup(rig);
    }
}


/*
 * Run a single rigctld command through the parse dispatch.
 * Writes cmd_str to a tmpfile as input, captures output to outbuf.
 * Returns the rigctl_parse return code.
 */
static int run_cmd(RIG *rig, const char *cmd_str, char *outbuf,
                   size_t outbuf_size)
{
    FILE *fin;
    FILE *fout;
    int vfo_opt = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int retval;
    long len;

    fin = tmpfile();
    fout = tmpfile();

    if (!fin || !fout)
    {
        if (fin) { fclose(fin); }

        if (fout) { fclose(fout); }

        return -1;
    }

    /* Streaming commands require ext_resp — auto-prepend '+' */
    if (strncmp(cmd_str, "\\stream_", 8) == 0)
    {
        fprintf(fin, "+%s\n", cmd_str);
    }
    else
    {
        fprintf(fin, "%s\n", cmd_str);
    }

    rewind(fin);

    /* Clear stale errno — scanfc in rigctl_parse checks errno unconditionally
     * and a leftover EINVAL from e.g. rig_set_conf causes false failures. */
    errno = 0;
    retval = rigctl_parse(rig, fin, fout, NULL, 0, NULL,
                          1, 0, &vfo_opt, '\n', &ext_resp, &resp_sep, 0);

    /* Capture output */
    fflush(fout);
    len = ftell(fout);
    rewind(fout);

    if (len > 0 && (size_t)len < outbuf_size)
    {
        fread(outbuf, 1, len, fout);
        outbuf[len] = '\0';
    }
    else if (len == 0)
    {
        outbuf[0] = '\0';
    }
    else
    {
        /* Output too large for buffer — truncate */
        fread(outbuf, 1, outbuf_size - 1, fout);
        outbuf[outbuf_size - 1] = '\0';
    }

    fclose(fin);
    fclose(fout);
    return retval;
}


/*
 * Run a command with ext_resp mode enabled.
 * Produces labeled output like "stream_id: 0\n".
 */
static int run_cmd_ext(RIG *rig, const char *cmd_str, char *outbuf,
                       size_t outbuf_size)
{
    FILE *fin;
    FILE *fout;
    int vfo_opt = 0;
    int ext_resp = 1;
    char resp_sep = '\n';
    int retval;
    long len;

    fin = tmpfile();
    fout = tmpfile();

    if (!fin || !fout)
    {
        if (fin) { fclose(fin); }

        if (fout) { fclose(fout); }

        return -1;
    }

    fprintf(fin, "%s\n", cmd_str);
    rewind(fin);

    errno = 0;
    retval = rigctl_parse(rig, fin, fout, NULL, 0, NULL,
                          1, 0, &vfo_opt, '\n', &ext_resp, &resp_sep, 0);

    fflush(fout);
    len = ftell(fout);
    rewind(fout);

    if (len > 0 && (size_t)len < outbuf_size)
    {
        fread(outbuf, 1, len, fout);
        outbuf[len] = '\0';
    }
    else
    {
        outbuf[0] = '\0';
    }

    fclose(fin);
    fclose(fout);
    return retval;
}


/* Parse "stream_id\nudp_port\n" from stream_open output */
static int parse_ext_int(const char *buf, const char *label, int *value);

/* --- Streaming fixtures --------------------------------------------------
 * acutest has no per-test fixture, so these are called explicitly. Pairing
 * begin() with end() keeps the stream registry's lifetime tied to the rig's
 * and keeps each test's intent, rather than its scaffolding, in view. */

static RIG *stream_test_begin(void)
{
    RIG *rig = test_rig_setup();
    rigctld_stream_registry_init(&g_stream_registry);
    return rig;
}

static void stream_test_end(RIG *rig)
{
    rigctld_stream_registry_destroy(&g_stream_registry);
    test_rig_teardown(rig);
}

static int parse_open_response(const char *buf, int *stream_id, int *udp_port)
{
    /* Try ext_resp labeled format first */
    if (parse_ext_int(buf, "stream_id", stream_id) == 0
            && parse_ext_int(buf, "udp_port", udp_port) == 0)
    {
        return 0;
    }

    /* Fall back to positional format */
    return (sscanf(buf, "%d\n%d\n", stream_id, udp_port) == 2) ? 0 : -1;
}


/* Count occurrences of a substring */
static int count_substr(const char *str, const char *sub)
{
    int count = 0;
    const char *p = str;
    size_t sublen = strlen(sub);

    while ((p = strstr(p, sub)) != NULL)
    {
        count++;
        p += sublen;
    }

    return count;
}


/* Parse "RPRT <code>" from output and extract numeric code.
 * Returns 0 on success, -1 if RPRT not found. */
static int parse_rprt_code(const char *buf, int *code)
{
    const char *p = strstr(buf, "RPRT ");

    if (!p)
    {
        return -1;
    }

    return (sscanf(p, "RPRT %d", code) == 1) ? 0 : -1;
}


/* Parse "label: value" from ext_resp output. Returns 0 on success. */
static int parse_ext_int(const char *buf, const char *label, int *value)
{
    char search[64];
    const char *p;

    snprintf(search, sizeof(search), "%s:", label);
    p = strstr(buf, search);

    if (!p)
    {
        return -1;
    }

    p += strlen(search);

    /* Skip spaces */
    while (*p == ' ')
    {
        p++;
    }

    return (sscanf(p, "%d", value) == 1) ? 0 : -1;
}


/* --- UDP helpers for feeder thread tests --- */

/* Poll `cond` every 5 ms for up to ~2 s so a feeder-thread effect is observed
 * deterministically, instead of relying on a fixed sleep that loses the race
 * under CI/parallel load.  Always falls through after the timeout so the
 * following assertion still reports the actual (possibly failed) state. */
#define WAIT_UNTIL(cond)                                  \
    do {                                                  \
        int _w = 0;                                       \
        while (!(cond) && _w < 400)                       \
        {                                                 \
            struct timespec _ts = { 0, 5000000L };        \
            nanosleep(&_ts, NULL);                        \
            _w++;                                         \
        }                                                 \
    } while (0)

/*
 * Build a 32-byte subscribe packet for the given stream type.
 */
/* Look up subscribe_token for a stream from the global registry. */
static uint32_t get_stream_token(uint16_t stream_id)
{
    struct rigctld_stream *stream = rigctld_stream_registry_find_by_id(
                                        &g_stream_registry, stream_id);
    return stream ? stream->subscribe_token : 0;
}


static void build_control_packet(unsigned char *buf, uint8_t stream_type,
                                 uint16_t stream_id, uint16_t control_flag)
{
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = 1;
    hdr.type = stream_type;
    hdr.stream_id = stream_id;
    hdr.subscribe_token = get_stream_token(stream_id);
    hdr.control = control_flag;
    stream_packet_header_pack(&hdr, buf);
}


static int send_control_pkt(int client_sock, int server_port,
                            uint8_t stream_type, uint16_t stream_id,
                            uint16_t control_flag)
{
    unsigned char pkt[32];
    build_control_packet(pkt, stream_type, stream_id, control_flag);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(server_port);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ssize_t sent = sendto(client_sock, pkt, 32, 0,
                          (struct sockaddr *)&dest, sizeof(dest));

    return (sent == 32) ? 0 : -1;
}


/*
 * Receive a UDP datagram with select() timeout.
 * Returns bytes received, or -1 on timeout/error.
 */
static ssize_t udp_recv_timeout(int sock, void *buf, size_t buflen,
                                int timeout_ms)
{
    struct timespec start;
    int remaining_ms = timeout_ms;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (;;)
    {
        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = remaining_ms / 1000;
        tv.tv_usec = (remaining_ms % 1000) * 1000;

        int ready = select(sock + 1, &fds, NULL, NULL, &tv);

        if (ready > 0)
        {
            return recvfrom(sock, buf, buflen, 0, NULL, NULL);
        }

        if (ready == 0)
        {
            return -1;
        }

        /* A signal must not be reported as an expired timeout: resume the
         * wait with whatever time is left. */
        if (errno != EINTR)
        {
            return -1;
        }

        struct timespec now;

        clock_gettime(CLOCK_MONOTONIC, &now);
        long spent_ms = (now.tv_sec - start.tv_sec) * 1000
                        + (now.tv_nsec - start.tv_nsec) / 1000000;

        if (spent_ms >= timeout_ms)
        {
            return -1;
        }

        remaining_ms = timeout_ms - (int)spent_ms;
    }
}


/*
 * Create a client UDP socket bound to loopback ephemeral port.
 * Returns socket fd, or -1 on error.
 */
static int create_client_udp_socket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        return -1;
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}


/* Convenience wrappers for common control packet types */
static int send_subscribe_pkt(int client_sock, int server_port,
                              uint8_t stream_type, uint16_t stream_id)
{
    return send_control_pkt(client_sock, server_port, stream_type, stream_id,
                            RIG_STREAM_CTRL_SUBSCRIBE);
}

/* Subscribe and consume the SUBSCRIBE_ACK. Either the request or its reply is
 * a single datagram that the host may drop under load, so the subscribe is
 * repeated until one is answered; anything else arriving first is discarded.
 * Returns the ACK length (the datagram is copied into ack), or -1 if no ACK
 * arrived. Repeating SUBSCRIBE is what the server expects — it treats a second
 * one as a re-subscribe. */
static ssize_t subscribe_and_await_ack(int client_sock, int server_port,
                                       uint8_t stream_type, uint16_t stream_id,
                                       unsigned char *ack, size_t ack_cap)
{
    int attempt;

    for (attempt = 0; attempt < 3; attempt++)
    {
        int tries;

        if (send_subscribe_pkt(client_sock, server_port, stream_type,
                               stream_id) != 0)
        {
            return -1;
        }

        for (tries = 0; tries < 4; tries++)
        {
            ssize_t n = udp_recv_timeout(client_sock, ack, ack_cap, 700);
            struct rig_stream_packet_header hdr;

            if (n < RIG_STREAM_HEADER_SIZE)
            {
                break;  /* nothing came back; send SUBSCRIBE again */
            }

            if (stream_packet_header_unpack(ack, (size_t)n, &hdr) == 0
                    && (hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK))
            {
                return n;
            }
        }
    }

    return -1;
}

/* Receive the first data frame, skipping ACK, metadata and time-only frames.
 * Returns the datagram length with *hdr filled, or -1 if none arrived within
 * max_tries receives. Loops that need a different policy for a short or
 * unparsable datagram — breaking out rather than skipping, or reporting it —
 * keep their own loop rather than bending this one. */
static ssize_t recv_data_packet(int sock, unsigned char *pkt, size_t cap,
                                struct rig_stream_packet_header *hdr,
                                int timeout_ms, int max_tries)
{
    int i;

    for (i = 0; i < max_tries; i++)
    {
        ssize_t n = udp_recv_timeout(sock, pkt, cap, timeout_ms);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            continue;
        }

        if (stream_packet_header_unpack(pkt, n, hdr) != 0)
        {
            continue;
        }

        if (!pkt_is_data(hdr) || pkt_data_len(hdr) == 0)
        {
            continue;   /* skip non-data and time-only frames */
        }

        return n;
    }

    return -1;
}


static int send_ping_pkt(int client_sock, int server_port,
                         uint8_t stream_type, uint16_t stream_id)
{
    return send_control_pkt(client_sock, server_port, stream_type, stream_id,
                            RIG_STREAM_CTRL_PING);
}


/* --- stream_caps tests --- */

void test_cmd_stream_caps_line_count(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_caps", buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("rigctl_parse returned %d", ret);

    /* Dummy backend declares exactly 4 stream types */
    int type_count = count_substr(buf, "type=");
    TEST_CHECK(type_count == 4);
    TEST_MSG("expected 4 type= entries, got %d in:\n%s", type_count, buf);

    test_rig_teardown(rig);
}


void test_cmd_stream_caps_audio_rx_complete(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_caps", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    /* Verify the complete AUDIO_RX line matches exactly */
    TEST_CHECK(strstr(buf, EXPECTED_AUDIO_RX_LINE) != NULL);
    TEST_MSG("AUDIO_RX line mismatch.\nExpected substring:\n  %s\nActual output:\n%s",
             EXPECTED_AUDIO_RX_LINE, buf);

    test_rig_teardown(rig);
}


void test_cmd_stream_caps_audio_tx_complete(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_caps", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, EXPECTED_AUDIO_TX_LINE) != NULL);
    TEST_MSG("AUDIO_TX line mismatch.\nExpected substring:\n  %s\nActual output:\n%s",
             EXPECTED_AUDIO_TX_LINE, buf);

    test_rig_teardown(rig);
}


void test_cmd_stream_caps_iq_rx_complete(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_caps", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, EXPECTED_IQ_RX_LINE) != NULL);
    TEST_MSG("IQ_RX line mismatch.\nExpected substring:\n  %s\nActual output:\n%s",
             EXPECTED_IQ_RX_LINE, buf);

    test_rig_teardown(rig);
}


void test_cmd_stream_caps_iq_tx_complete(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_caps", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, EXPECTED_IQ_TX_LINE) != NULL);
    TEST_MSG("IQ_TX line mismatch.\nExpected substring:\n  %s\nActual output:\n%s",
             EXPECTED_IQ_TX_LINE, buf);

    test_rig_teardown(rig);
}


/* --- stream_open tests --- */

void test_cmd_stream_open_audio(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_open returned %d", ret);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);
    TEST_MSG("failed to parse response: '%s'", buf);

    TEST_CHECK(stream_id >= 1);
    TEST_MSG("stream_id: got %d, expected >= 1", stream_id);

    TEST_CHECK(udp_port > 1024);
    TEST_MSG("udp_port: got %d, expected > 1024 (ephemeral)", udp_port);

    /* Verify registry has the stream with correct config */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 1);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        TEST_CHECK(s->udp_port == udp_port);
        TEST_CHECK(s->type == RIG_STREAM_TYPE_AUDIO_RX);
        TEST_CHECK(s->config.sample_rate == 48000);
        TEST_CHECK(s->config.format == RIG_STREAM_FORMAT_PCM_S16);
        TEST_CHECK(s->config.channels == 1);

        /* Verify backend stream was actually opened (#8) */
        TEST_CHECK(s->backend_stream != NULL);
        TEST_MSG("backend_stream is NULL — rig_stream_open not called?");

        if (s->backend_stream)
        {
            TEST_CHECK(rig_stream_get_type(s->backend_stream) ==
                       RIG_STREAM_TYPE_AUDIO_RX);
            TEST_MSG("backend stream type mismatch");
        }

        /* Verify UDP socket is valid (#7 partial) */
        TEST_CHECK(s->udp_sock >= 0);
        TEST_CHECK(socket_is_open(s->udp_sock));
        TEST_MSG("UDP socket fd %d is not valid", s->udp_sock);
    }

    stream_test_end(rig);
}


void test_cmd_stream_open_iq(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 192000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_open returned %d", ret);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);
    TEST_MSG("failed to parse response: '%s'", buf);

    TEST_CHECK(stream_id >= 1);
    TEST_CHECK(udp_port > 1024);

    /* Verify IQ config has 2 channels */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        TEST_CHECK(s->config.channels == 1);
        TEST_MSG("IQ channels: got %d, expected 1", s->config.channels);

        TEST_CHECK(s->config.format == RIG_STREAM_FORMAT_IQ_CS16);
        TEST_CHECK(s->config.sample_rate == 192000);

        /* Verify backend stream was opened */
        TEST_CHECK(s->backend_stream != NULL);

        if (s->backend_stream)
        {
            TEST_CHECK(rig_stream_get_type(s->backend_stream) ==
                       RIG_STREAM_TYPE_IQ_RX);
        }
    }

    stream_test_end(rig);
}


void test_cmd_stream_open_sequential_ids(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Open first stream */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int id1 = -1, port1 = -1;
    TEST_CHECK(parse_open_response(buf, &id1, &port1) == 0);
    TEST_CHECK(id1 >= 1);

    /* Open second stream of same type */
    ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int id2 = -1, port2 = -1;
    TEST_CHECK(parse_open_response(buf, &id2, &port2) == 0);
    TEST_CHECK(id2 > id1);
    TEST_MSG("second stream_id: got %d, expected > %d", id2, id1);

    /* Ports must differ */
    TEST_CHECK(port1 != port2);
    TEST_MSG("ports should differ: %d vs %d", port1, port2);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 2);

    stream_test_end(rig);
}


void test_cmd_stream_open_bad_type(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_open BOGUS PCM_S16 48000", buf, sizeof(buf));

    /* Verify RPRT with specific negative error code */
    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected negative RPRT code, got %d", rprt_code);

    /* Registry must be empty — nothing was opened */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_stream_open_bad_format(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_open AUDIO_RX BOGUS 48000", buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected negative RPRT code, got %d", rprt_code);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_stream_open_bad_rate(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 0", buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected negative RPRT code, got %d", rprt_code);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_stream_open_unsupported_rate(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* 99999 Hz is not in dummy backend's rate list */
    run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 99999", buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected negative RPRT for unsupported rate, got %d", rprt_code);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


/* #5: Cross-type format mismatch — IQ format on audio type.
 * The frontend validates format against the backend's capability bitmask,
 * so this should fail because IQ_CS16 is not in AUDIO_RX's format list. */
void test_cmd_stream_open_iq_format_on_audio(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_open AUDIO_RX IQ_CS16 48000", buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected error for IQ format on audio type, got RPRT %d", rprt_code);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


/* Coherent multichannel I/Q over the text protocol: channels= accepts
 * the full caps range — the dummy declares 4 coherent I/Q channels —
 * with validation done by the frontend caps check. */
void test_cmd_stream_open_iq_multichannel(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CF32 96000 channels=4",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("4-channel IQ open failed: '%s'", buf);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);
    TEST_MSG("failed to parse response: '%s'", buf);

    /* Native format, rate and channel count: a native stream. */
    int val = -1;
    TEST_CHECK(parse_ext_int(buf, "conversions", &val) == 0);
    TEST_CHECK(val == RIG_STREAM_CONV_NONE);
    TEST_MSG("conversions: got %d, expected 0", val);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        TEST_CHECK(s->config.channels == 4);
        TEST_MSG("channels: got %d, expected 4", s->config.channels);
    }

    /* Above the caps maximum still fails cleanly in the frontend. */
    run_cmd(rig, "\\stream_open IQ_RX IQ_CF32 96000 channels=5",
            buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("channels=5: got RPRT %d, expected negative", rprt_code);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 1);

    stream_test_end(rig);
}


/* The open response reports the conversion stages — a converted
 * request (PCM_S16 against the PCM_F32-native dummy) shows CONV_FORMAT,
 * a native request shows CONV_NONE. Format conversion needs no resampler,
 * so this holds with and without HAVE_SAMPLERATE. */
void test_cmd_stream_open_reports_conversions(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];
    char cmd[64];
    int val = -1;

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(parse_ext_int(buf, "conversions", &val) == 0);
    TEST_MSG("no conversions line in: '%s'", buf);
    TEST_CHECK(val == RIG_STREAM_CONV_FORMAT);
    TEST_MSG("conversions: got %d, expected %d", val, RIG_STREAM_CONV_FORMAT);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_F32 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    val = -1;
    TEST_CHECK(parse_ext_int(buf, "conversions", &val) == 0);
    TEST_CHECK(val == RIG_STREAM_CONV_NONE);
    TEST_MSG("native open conversions: got %d, expected 0", val);

    stream_test_end(rig);
}


/* require_native=1 refuses a convertible-but-not-native request
 * with -RIG_ENAVAIL (distinct from -RIG_EINVAL for the impossible), and
 * accepts the native form of the same request. */
void test_cmd_stream_open_require_native(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000 require_native=1",
            buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code == -RIG_ENAVAIL);
    TEST_MSG("RPRT: got %d, expected %d", rprt_code, -RIG_ENAVAIL);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_F32 48000 require_native=1",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("native require_native open failed: '%s'", buf);

    int val = -1;
    TEST_CHECK(parse_ext_int(buf, "conversions", &val) == 0);
    TEST_CHECK(val == RIG_STREAM_CONV_NONE);

    stream_test_end(rig);
}


/* #6: Multiple stream types open simultaneously.
 * AUDIO_RX + IQ_RX + AUDIO_TX — verifies cross-type registry isolation. */
void test_cmd_stream_open_multiple_types(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];
    int id, port;

    /* Open AUDIO_RX */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(parse_open_response(buf, &id, &port) == 0);
    int audio_rx_id = id;

    /* Open IQ_RX */
    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 192000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(parse_open_response(buf, &id, &port) == 0);
    int iq_rx_id = id;

    /* Open AUDIO_TX */
    ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_F32 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(parse_open_response(buf, &id, &port) == 0);
    int audio_tx_id = id;

    /* IDs are globally unique */
    TEST_CHECK(audio_rx_id >= 1);
    TEST_MSG("AUDIO_RX id: got %d, expected >= 1", audio_rx_id);
    TEST_CHECK(iq_rx_id > audio_rx_id);
    TEST_MSG("IQ_RX id: got %d, expected > %d", iq_rx_id, audio_rx_id);
    TEST_CHECK(audio_tx_id > iq_rx_id);
    TEST_MSG("AUDIO_TX id: got %d, expected > %d", audio_tx_id, iq_rx_id);

    /* All 3 in registry */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 3);

    /* Each lookup returns the correct stream */
    struct rigctld_stream *s;
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, audio_rx_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        TEST_CHECK(s->type == RIG_STREAM_TYPE_AUDIO_RX);
        TEST_CHECK(s->config.format == RIG_STREAM_FORMAT_PCM_S16);
    }

    s = rigctld_stream_registry_find_by_id(&g_stream_registry, iq_rx_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        TEST_CHECK(s->type == RIG_STREAM_TYPE_IQ_RX);
        TEST_CHECK(s->config.format == RIG_STREAM_FORMAT_IQ_CS16);
        TEST_CHECK(s->config.channels == 1);
    }

    s = rigctld_stream_registry_find_by_id(&g_stream_registry, audio_tx_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        TEST_CHECK(s->type == RIG_STREAM_TYPE_AUDIO_TX);
        TEST_CHECK(s->config.format == RIG_STREAM_FORMAT_PCM_F32);
    }

    /* Close one, others must remain */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", audio_rx_id);
    ret = run_cmd(rig, close_cmd, buf, sizeof(buf));
    TEST_CHECK(strstr(buf, "RPRT 0") != NULL);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 2);

    stream_test_end(rig);
}


/* #10: ext_resp output must contain parseable numeric values */
void test_cmd_stream_open_ext_resp(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd_ext(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                          buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("ext stream_open returned %d", ret);

    /* Parse actual values from labeled output */
    int ext_stream_id = -1;
    int ext_udp_port = -1;

    TEST_CHECK(parse_ext_int(buf, "stream_id", &ext_stream_id) == 0);
    TEST_MSG("could not parse stream_id from: '%s'", buf);

    TEST_CHECK(parse_ext_int(buf, "udp_port", &ext_udp_port) == 0);
    TEST_MSG("could not parse udp_port from: '%s'", buf);

    TEST_CHECK(ext_stream_id >= 1);
    TEST_MSG("ext_resp stream_id: got %d, expected >= 1", ext_stream_id);

    TEST_CHECK(ext_udp_port > 1024);
    TEST_MSG("ext_resp udp_port: got %d, expected > 1024", ext_udp_port);

    /* Verify the values match what's in the registry */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, ext_stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        TEST_CHECK(s->udp_port == ext_udp_port);
        TEST_MSG("registry port %d != ext_resp port %d",
                 s->udp_port, ext_udp_port);
    }

    stream_test_end(rig);
}


/* --- stream_close tests --- */

/* #9: Verify cleanup — socket fd closed, backend stream closed */
void test_cmd_stream_close_happy_path(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Open a stream */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    /* Save the UDP socket fd before close for verification */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    int saved_fd = s ? s->udp_sock : -1;
    TEST_CHECK(saved_fd >= 0);

    /* Verify fd is valid before close */
    TEST_CHECK(socket_is_open(saved_fd));

    /* Close it */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    ret = run_cmd(rig, close_cmd, buf, sizeof(buf));

    TEST_CHECK(ret == 0);
    TEST_MSG("stream_close returned %d", ret);

    /* Verify RPRT 0 */
    int rprt_code = -1;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code == 0);
    TEST_MSG("expected RPRT 0, got RPRT %d", rprt_code);

    /* Registry must be empty */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    /* Verify socket fd was closed (#9) */
    TEST_CHECK(!socket_is_open(saved_fd));
    TEST_MSG("socket fd %d should be closed after stream_close", saved_fd);

    stream_test_end(rig);
}


void test_cmd_stream_close_not_found(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_close 99", buf, sizeof(buf));

    /* Verify specific error code */
    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected negative RPRT for not-found, got %d", rprt_code);

    stream_test_end(rig);
}


void test_cmd_stream_close_double_close(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];
    int rprt_code;

    /* Open a stream */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    /* First close — should succeed */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code == 0);
    TEST_MSG("first close should succeed, got RPRT %d", rprt_code);

    /* Second close — should fail */
    run_cmd(rig, close_cmd, buf, sizeof(buf));
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("double close should fail, got RPRT %d", rprt_code);

    stream_test_end(rig);
}


void test_cmd_stream_close_frees_slot(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Open stream */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int id1 = -1, port1 = -1;
    parse_open_response(buf, &id1, &port1);
    TEST_CHECK(id1 >= 1);

    /* Close it */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", id1);
    ret = run_cmd(rig, close_cmd, buf, sizeof(buf));
    int rprt_code;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code == 0);

    /* Reopen — slot freed, new ID allocated */
    ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int id2 = -1, port2 = -1;
    parse_open_response(buf, &id2, &port2);
    TEST_CHECK(id2 > id1);
    TEST_MSG("new id: got %d, expected > %d", id2, id1);

    stream_test_end(rig);
}


/* #3: Non-numeric stream_close argument */
void test_cmd_stream_close_non_numeric(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_close abc", buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected error for non-numeric arg, got RPRT %d", rprt_code);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


/* #4: Negative stream_id */
void test_cmd_stream_close_negative_id(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    run_cmd(rig, "\\stream_close -1", buf, sizeof(buf));

    int rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_MSG("could not parse RPRT from: '%s'", buf);
    TEST_CHECK(rprt_code < 0);
    TEST_MSG("expected error for negative id, got RPRT %d", rprt_code);

    stream_test_end(rig);
}


/* --- open+close interaction tests --- */

void test_cmd_open_close_reopen_cycle(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];
    int id, port;
    int i;
    int rprt_code;

    /* Open and close 3 times — IDs increase monotonically */
    int prev_id = 0;

    for (i = 0; i < 3; i++)
    {
        int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                          buf, sizeof(buf));
        TEST_CHECK(ret == 0);
        TEST_CHECK(parse_open_response(buf, &id, &port) == 0);
        TEST_CHECK(id > prev_id);
        TEST_MSG("cycle %d: id=%d, expected > %d", i, id, prev_id);

        char close_cmd[64];
        snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", id);
        ret = run_cmd(rig, close_cmd, buf, sizeof(buf));
        TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
        TEST_CHECK(rprt_code == 0);
        prev_id = id;
    }

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_multiple_streams_close_middle(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];
    int ids[3], ports[3];
    int i;
    int rprt_code;

    /* Open 3 streams */
    for (i = 0; i < 3; i++)
    {
        int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                          buf, sizeof(buf));
        TEST_CHECK(ret == 0);
        TEST_CHECK(parse_open_response(buf, &ids[i], &ports[i]) == 0);
        TEST_CHECK(ids[i] >= 1);
    }

    /* IDs are monotonically increasing */
    TEST_CHECK(ids[1] > ids[0]);
    TEST_CHECK(ids[2] > ids[1]);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 3);

    /* Close the middle one */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", ids[1]);
    run_cmd(rig, close_cmd, buf, sizeof(buf));
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code == 0);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 2);

    /* First and third should still exist */
    TEST_CHECK(rigctld_stream_registry_find_by_id(&g_stream_registry,
               ids[0]) != NULL);
    TEST_CHECK(rigctld_stream_registry_find_by_id(&g_stream_registry,
               ids[2]) != NULL);

    /* Middle should be gone */
    TEST_CHECK(rigctld_stream_registry_find_by_id(&g_stream_registry,
               ids[1]) == NULL);

    /* Opening a new stream gets a new ID (no reuse) */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int new_id = -1, new_port = -1;
    parse_open_response(buf, &new_id, &new_port);
    TEST_CHECK(new_id > ids[2]);
    TEST_MSG("new id: got %d, expected > %d", new_id, ids[2]);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 3);

    stream_test_end(rig);
}


/* #7: Verify the UDP socket is actually usable — send a datagram and
 * receive it on the server's socket. */
void test_cmd_stream_open_udp_socket_usable(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (!s)
    {
        stream_test_end(rig);
        return;
    }

    /* Verify socket fd is valid */
    TEST_CHECK(s->udp_sock >= 0);
    TEST_MSG("udp_sock=%d, expected >= 0", s->udp_sock);

    /* Verify socket is bound to the reported port. The socket is
     * dual-stack IPv6, so the buffer must fit a sockaddr_in6: Winsock
     * fails getsockname outright on a too-small buffer where POSIX
     * would truncate. */
    struct sockaddr_storage bound_addr;
    socklen_t addr_len = sizeof(bound_addr);
    int gret = getsockname(s->udp_sock, (struct sockaddr *)&bound_addr,
                           &addr_len);
    TEST_CHECK(gret == 0);
    TEST_MSG("getsockname returned %d", gret);

    int bound_port = 0;

    if (bound_addr.ss_family == AF_INET6)
    {
        struct sockaddr_in6 a6;
        memcpy(&a6, &bound_addr, sizeof(a6));
        bound_port = ntohs(a6.sin6_port);
    }
    else
    {
        struct sockaddr_in a4;
        memcpy(&a4, &bound_addr, sizeof(a4));
        bound_port = ntohs(a4.sin_port);
    }
    TEST_CHECK(bound_port == udp_port);
    TEST_MSG("bound port=%d, reported port=%d", bound_port, udp_port);

    /* Verify feeder thread is running */
    TEST_CHECK(s->running == 1);
    TEST_MSG("feeder thread running=%d, expected 1", s->running);

    /* Verify rig handle is set */
    TEST_CHECK(s->rig == rig);

    stream_test_end(rig);
}


/* --- RX feeder thread tests --- */

/* After stream_open, send subscribe packet, verify ACK arrives. */
void test_rx_subscribe_ack(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);


    /* Wait for ACK */
    unsigned char ack_buf[64];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        ack_buf, sizeof(ack_buf));
    TEST_CHECK(n >= 32);
    TEST_MSG("no subscribe ACK received within 2s (got %zd bytes)", n);

    if (n >= 32)
    {
        struct rig_stream_packet_header ack_hdr;
        TEST_CHECK(stream_packet_header_unpack(ack_buf, n, &ack_hdr) == 0);
        TEST_CHECK((ack_hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK) != 0);
        TEST_MSG("expected ACK control bit 0x%04x, got 0x%04x",
                 RIG_STREAM_CTRL_SUBSCRIBE_ACK, ack_hdr.control);
        TEST_CHECK(ack_hdr.version == 1);
        TEST_CHECK(ack_hdr.type == RIG_STREAM_TYPE_AUDIO_RX);
    }

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* After subscribe, verify data packets arrive with correct header fields. */
void test_rx_data_arrives(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe and consume ACK */

    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= 32);  /* ACK */

    /* Receive data packets, skipping metadata/control frames */
    struct rig_stream_packet_header hdr;

    n = recv_data_packet(client_sock, pkt, sizeof(pkt), &hdr, 2000, 10);
    int got_data = (n > 0);

    TEST_CHECK(got_data);
    TEST_MSG("no data packet received within timeout");

    if (got_data)
    {
        TEST_CHECK(hdr.version == 1);
        TEST_CHECK(hdr.type == RIG_STREAM_TYPE_AUDIO_RX);
        TEST_MSG("type: got %d, expected %d", hdr.type, RIG_STREAM_TYPE_AUDIO_RX);

        TEST_CHECK(hdr.sample_rate == 48000);
        TEST_MSG("sample_rate: got %u, expected 48000", hdr.sample_rate);

        TEST_CHECK(hdr.format == RIG_STREAM_FMT_ID_PCM_S16);
        TEST_MSG("format: got %d, expected %d (PCM_S16)",
                 hdr.format, RIG_STREAM_FMT_ID_PCM_S16);

        TEST_CHECK(hdr.channels == 1);
        TEST_MSG("channels: got %d, expected 1", hdr.channels);

        /* Data frame — only the TIME bit may be set */
        TEST_CHECK(pkt_is_data(&hdr));

        /* payload_len matches actual payload (incl. any time block) */
        TEST_CHECK(hdr.payload_len == (uint16_t)(n - 32));
        TEST_MSG("payload_len %u != actual %zd", hdr.payload_len, n - 32);

        /* payload_len <= max_payload (1420) */
        TEST_CHECK(hdr.payload_len <= 1420);

        /* sample data is a multiple of sample_size * channels (2 * 1 = 2) */
        TEST_CHECK(pkt_data_len(&hdr) % 2 == 0);
        TEST_MSG("payload_len %u not aligned to sample size", hdr.payload_len);
    }

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Receive multiple data packets, verify seq numbers increment and
 * timestamps advance. */
void test_rx_sequence_continuous(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe and consume ACK */

    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= 32);  /* ACK */

    /* Receive 10 data packets, verify seq and timestamp */
    uint32_t prev_seq = 0;
    uint64_t prev_timestamp = 0;
    int data_packets = 0;
    int i;

    for (i = 0; i < 15; i++)  /* try up to 15 to get 10 data packets */
    {
        n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < 32)
        {
            TEST_MSG("packet %d: receive timeout (n=%zd)", i, n);
            break;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            TEST_MSG("packet %d: header unpack failed", i);
            break;
        }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0)
        {
            continue;  /* skip non-data and time-only frames */
        }

        if (data_packets == 0)
        {
            /* Record starting values */
            prev_seq = hdr.seq;
            prev_timestamp = hdr.timestamp;
        }
        else
        {
            /* seq must be exactly prev + 1 */
            TEST_CHECK(hdr.seq == prev_seq + 1);
            TEST_MSG("data pkt %d: seq=%u, expected=%u",
                     data_packets, hdr.seq, prev_seq + 1);

            /* timestamp must advance */
            TEST_CHECK(hdr.timestamp > prev_timestamp);
            TEST_MSG("data pkt %d: timestamp=%llu, prev=%llu (not advancing)",
                     data_packets,
                     (unsigned long long)hdr.timestamp,
                     (unsigned long long)prev_timestamp);

            prev_seq = hdr.seq;
            prev_timestamp = hdr.timestamp;
        }

        data_packets++;

        if (data_packets >= 10)
        {
            break;
        }
    }

    TEST_CHECK(data_packets >= 10);
    TEST_MSG("only received %d data packets, expected at least 10", data_packets);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* --- TX data helpers --- */

/*
 * Build a data packet: 32-byte header + payload.
 * buf must be at least RIG_STREAM_HEADER_SIZE + payload_len bytes.
 * Returns total packet size (header + payload).
 */
static size_t build_data_packet(unsigned char *buf, uint8_t stream_type,
                                uint16_t stream_id, uint32_t seq,
                                uint64_t timestamp, uint32_t sample_rate,
                                uint8_t format_idx, uint8_t channels,
                                const void *payload, size_t payload_len)
{
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = 1;
    hdr.type = stream_type;
    hdr.stream_id = stream_id;
    hdr.subscribe_token = get_stream_token(stream_id);
    hdr.seq = seq;
    hdr.timestamp = timestamp;
    hdr.sample_rate = sample_rate;
    hdr.format = format_idx;
    hdr.channels = channels;
    hdr.control = 0;  /* data frame */
    hdr.payload_len = (uint16_t)payload_len;
    stream_packet_header_pack(&hdr, buf);

    if (payload && payload_len > 0)
    {
        memcpy(buf + RIG_STREAM_HEADER_SIZE, payload, payload_len);
    }

    return RIG_STREAM_HEADER_SIZE + payload_len;
}


/*
 * Send a pre-built packet to the server's UDP port.
 * Returns 0 on success, -1 on error.
 */
static int send_data_pkt(int client_sock, int server_port,
                         const unsigned char *pkt, size_t pkt_len)
{
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(server_port);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ssize_t sent = sendto(client_sock, pkt, pkt_len, 0,
                          (struct sockaddr *)&dest, sizeof(dest));

    return (sent == (ssize_t)pkt_len) ? 0 : -1;
}


/* --- IQ and payload verification tests --- */

/* Verify payload bytes match incrementing counter pattern.
 * Updates *expected for cross-packet continuity.
 * Returns 0 if all bytes match, -1 on first mismatch. */
static int verify_counter_payload(const unsigned char *payload, size_t len,
                                  uint8_t *expected)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (payload[i] != *expected)
        {
            return -1;
        }

        (*expected)++;  /* wraps naturally as uint8_t */
    }

    return 0;
}


/* Open IQ_RX stream, subscribe, receive a data packet, verify IQ header fields. */
void test_rx_iq_data_arrives(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 192000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);


    /* Consume ACK */
    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_IQ_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    /* Receive data packets until we get one with control==0 */
    struct rig_stream_packet_header hdr;

    n = recv_data_packet(client_sock, pkt, sizeof(pkt), &hdr, 2000, 10);
    int got_data = (n > 0);

    TEST_CHECK(got_data);
    TEST_MSG("no IQ data packet received within timeout");

    if (got_data)
    {
        TEST_CHECK(hdr.version == RIG_STREAM_PROTOCOL_VERSION);
        TEST_CHECK(hdr.type == RIG_STREAM_TYPE_IQ_RX);
        TEST_MSG("type=%d, expected IQ_RX=%d", hdr.type, RIG_STREAM_TYPE_IQ_RX);

        TEST_CHECK(hdr.sample_rate == 192000);
        TEST_MSG("sample_rate=%u, expected 192000", hdr.sample_rate);

        TEST_CHECK(hdr.format == RIG_STREAM_FMT_ID_IQ_CS16);
        TEST_MSG("format=%u, expected IQ_CS16 idx=%u",
                 hdr.format, RIG_STREAM_FMT_ID_IQ_CS16);

        TEST_CHECK(hdr.channels == 1);
        TEST_MSG("channels=%u, expected 1", hdr.channels);

        /* Payload must be aligned to IQ_CS16 sample size (4 bytes) */
        TEST_CHECK(hdr.payload_len > 0);
        TEST_CHECK(hdr.payload_len % 4 == 0);
        TEST_MSG("payload_len=%u, not aligned to 4", hdr.payload_len);

        /* Actual received size must match header + payload */
        TEST_CHECK(n == RIG_STREAM_HEADER_SIZE + hdr.payload_len);
    }

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Set counter mode, open AUDIO_RX, verify 3+ packets have continuous
 * incrementing byte pattern with no gaps across packet boundaries. */
void test_rx_counter_payload_audio(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);

    /* Set dummy backend to counter mode */
    token_t tok = rig_token_lookup(rig, "stream_mode");
    TEST_CHECK(tok != 0);
    TEST_CHECK(rig_set_conf(rig, tok, "counter") == RIG_OK);

    rigctld_stream_registry_init(&g_stream_registry);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_F32 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);


    /* Consume ACK */
    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    /* Receive 3+ data packets and verify continuous counter pattern */
    uint8_t expected_byte = 0;
    int data_count = 0;
    int first_data = 1;
    int i;

    for (i = 0; i < 20 && data_count < 3; i++)
    {
        n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            TEST_MSG("packet %d: timeout", i);
            break;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0)
        {
            continue;  /* skip non-data and time-only frames */
        }

        unsigned char *payload = pkt + pkt_data_offset(&hdr);
        size_t data_len = pkt_data_len(&hdr);

        if (first_data)
        {
            /* Seed expected counter from first byte of first data packet */
            expected_byte = payload[0];
            expected_byte++;  /* verify_counter_payload starts from expected */
            first_data = 0;

            /* Verify the rest of this first packet is continuous */
            uint8_t check = payload[0];

            if (data_len > 1)
            {
                check++;
                int vret = verify_counter_payload(payload + 1,
                                                  data_len - 1, &check);
                TEST_CHECK(vret == 0);
                TEST_MSG("first data packet: counter gap within packet");
            }

            expected_byte = check;
        }
        else
        {
            /* Cross-packet continuity: expected_byte should match payload[0] */
            int vret = verify_counter_payload(payload, data_len,
                                              &expected_byte);
            TEST_CHECK(vret == 0);
            TEST_MSG("data packet %d: counter gap (expected %u, got %u)",
                     data_count, (unsigned)(expected_byte - data_len),
                     payload[0]);
        }

        data_count++;
    }

    TEST_CHECK(data_count >= 3);
    TEST_MSG("only got %d data packets, expected >= 3", data_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* System-level codec passthrough: open the dummy's fabricated OPUS codec
 * (counter mode) and verify the packet stream arrives byte-identical —
 * continuous counter across 3+ UDP packets, wire format id OPUS, and a
 * conversions: 0 open response. No conversion stage may ever touch a
 * codec stream. */
void test_rx_codec_passthrough_audio(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);

    token_t tok = rig_token_lookup(rig, "stream_mode");
    TEST_CHECK(tok != 0);
    TEST_CHECK(rig_set_conf(rig, tok, "counter") == RIG_OK);

    rigctld_stream_registry_init(&g_stream_registry);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX OPUS 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("codec open failed: '%s'", buf);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    /* A codec stream is native by definition. */
    int val = -1;
    TEST_CHECK(parse_ext_int(buf, "conversions", &val) == 0);
    TEST_CHECK(val == RIG_STREAM_CONV_NONE);
    TEST_MSG("codec conversions: got %d, expected 0", val);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    uint8_t expected_byte = 0;
    int data_count = 0;
    int first_data = 1;
    uint64_t prev_ts = 0;
    int i;

    for (i = 0; i < 20 && data_count < 3; i++)
    {
        n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            TEST_MSG("packet %d: timeout", i);
            break;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0)
        {
            continue;
        }

        /* The wire must label the payload as the codec, untouched. */
        TEST_CHECK(hdr.format == RIG_STREAM_FMT_ID_OPUS);
        TEST_MSG("packet %d: wire format id %u, expected %u (OPUS)",
                 i, (unsigned)hdr.format, (unsigned)RIG_STREAM_FMT_ID_OPUS);

        /* One datagram = one codec frame; the timestamp advances by the
         * fabricated 480-sample duration per frame. */
        if (data_count > 0)
        {
            TEST_CHECK(hdr.timestamp == prev_ts + 480);
            TEST_MSG("packet %d: ts %llu, expected %llu", i,
                     (unsigned long long)hdr.timestamp,
                     (unsigned long long)(prev_ts + 480));
        }

        prev_ts = hdr.timestamp;

        unsigned char *payload = pkt + pkt_data_offset(&hdr);
        size_t data_len = pkt_data_len(&hdr);

        if (first_data)
        {
            uint8_t check = payload[0];
            first_data = 0;

            if (data_len > 1)
            {
                check++;
                int vret = verify_counter_payload(payload + 1,
                                                  data_len - 1, &check);
                TEST_CHECK(vret == 0);
                TEST_MSG("first codec packet: byte altered within packet");
            }

            expected_byte = check;
        }
        else
        {
            int vret = verify_counter_payload(payload, data_len,
                                              &expected_byte);
            TEST_CHECK(vret == 0);
            TEST_MSG("codec packet %d: bytes altered or lost", data_count);
        }

        data_count++;
    }

    TEST_CHECK(data_count >= 3);
    TEST_MSG("only got %d codec data packets, expected >= 3", data_count);

    /* Observability: the status reports the produced codec-frame count. */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);

    if (run_cmd_ext(rig, cmd, buf, sizeof(buf)) == 0)
    {
        int cf = -1;
        TEST_CHECK(parse_ext_int(buf, "codec_frames", &cf) == 0);
        TEST_CHECK(cf >= data_count);
        TEST_MSG("codec_frames %d, expected >= %d", cf, data_count);
    }

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* A codec request that is not exactly native must be refused outright:
 * rates a raw format would reach through resampling (44100 curated,
 * 12000 = 48000/4) are -RIG_EINVAL for a codec. */
void test_cmd_stream_open_codec_non_native(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];
    int rprt_code;

    run_cmd(rig, "\\stream_open AUDIO_RX OPUS 44100", buf, sizeof(buf));
    rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code == -RIG_EINVAL);
    TEST_MSG("OPUS@44100: got RPRT %d, expected %d", rprt_code, -RIG_EINVAL);

    run_cmd(rig, "\\stream_open AUDIO_RX OPUS 12000", buf, sizeof(buf));
    rprt_code = 0;
    TEST_CHECK(parse_rprt_code(buf, &rprt_code) == 0);
    TEST_CHECK(rprt_code == -RIG_EINVAL);
    TEST_MSG("OPUS@12000: got RPRT %d, expected %d", rprt_code, -RIG_EINVAL);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


/* Set counter mode, open IQ_RX, verify 3+ packets have continuous
 * incrementing byte pattern with no gaps across packet boundaries. */
void test_rx_counter_payload_iq(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);

    /* Set dummy backend to counter mode */
    token_t tok = rig_token_lookup(rig, "stream_mode");
    TEST_CHECK(tok != 0);
    TEST_CHECK(rig_set_conf(rig, tok, "counter") == RIG_OK);

    rigctld_stream_registry_init(&g_stream_registry);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CF32 192000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);


    /* Consume ACK */
    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_IQ_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    /* Receive 3+ data packets and verify continuous counter pattern */
    uint8_t expected_byte = 0;
    int data_count = 0;
    int first_data = 1;
    int i;

    for (i = 0; i < 20 && data_count < 3; i++)
    {
        n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            TEST_MSG("packet %d: timeout", i);
            break;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0)
        {
            continue;  /* skip non-data and time-only frames */
        }

        unsigned char *payload = pkt + pkt_data_offset(&hdr);
        size_t data_len = pkt_data_len(&hdr);

        if (first_data)
        {
            expected_byte = payload[0];
            expected_byte++;
            first_data = 0;

            if (data_len > 1)
            {
                uint8_t check = expected_byte;
                int vret = verify_counter_payload(payload + 1,
                                                  data_len - 1, &check);
                TEST_CHECK(vret == 0);
                TEST_MSG("first IQ data packet: counter gap within packet");
                expected_byte = check;
            }
        }
        else
        {
            int vret = verify_counter_payload(payload, data_len,
                                              &expected_byte);
            TEST_CHECK(vret == 0);
            TEST_MSG("IQ data packet %d: counter gap (got %u, expected %u)",
                     data_count, payload[0],
                     (unsigned)(expected_byte - data_len));
        }

        data_count++;
    }

    TEST_CHECK(data_count >= 3);
    TEST_MSG("only got %d IQ data packets, expected >= 3", data_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Server frames carry the registry stream source ID: the stream_open
 * response reports it between stream_id and udp_port, and the subscribe ACK
 * plus every subsequent frame are stamped with it. */
void test_stream_source_id_stamping(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    g_stream_registry.source_id = 0x1234;
    char buf[1024];

    int ret = run_cmd_ext(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                          buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_open returned %d", ret);

    int stream_id = -1, udp_port = -1, resp_source_id = -1;
    TEST_CHECK(parse_ext_int(buf, "stream_id", &stream_id) == 0);
    TEST_CHECK(parse_ext_int(buf, "udp_port", &udp_port) == 0);
    TEST_CHECK(parse_ext_int(buf, "source_id", &resp_source_id) == 0);
    TEST_MSG("no source_id in response: '%s'", buf);
    TEST_CHECK(resp_source_id == 0x1234);
    TEST_MSG("resp_source_id=%d, expected %d", resp_source_id, 0x1234);

    /* source_id: sits between stream_id: and udp_port: */
    const char *p_stream = strstr(buf, "stream_id:");
    const char *p_source = strstr(buf, "source_id:");
    const char *p_port = strstr(buf, "udp_port:");
    TEST_CHECK(p_stream != NULL && p_source != NULL && p_port != NULL
               && p_stream < p_source && p_source < p_port);
    TEST_MSG("field order wrong in: '%s'", buf);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    struct rig_stream_packet_header hdr;
    TEST_CHECK(stream_packet_header_unpack(pkt, n, &hdr) == 0);
    TEST_CHECK(hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK);
    TEST_CHECK(hdr.source_id == 0x1234);
    TEST_MSG("ACK source_id=0x%04x, expected 0x1234", hdr.source_id);

    /* Every subsequent frame (metadata/data/time-only) carries the ID */
    int i;
    int frames = 0;

    for (i = 0; i < 3; i++)
    {
        n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            continue;
        }

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        frames++;
        TEST_CHECK(hdr.source_id == 0x1234);
        TEST_MSG("frame %d source_id=0x%04x, expected 0x1234",
                 i, hdr.source_id);
    }

    TEST_CHECK(frames > 0);
    TEST_MSG("no frames received after ACK");

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* A client frame carrying a non-zero source_id is dropped: a tampered
 * subscribe gets no ACK, while a compliant one still succeeds. */
void test_stream_source_id_inbound_reject(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    g_stream_registry.source_id = 0x1234;
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe carrying a non-zero source_id — must be dropped */
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_AUDIO_RX;
    hdr.stream_id = stream_id;
    hdr.source_id = 0x0005;
    hdr.subscribe_token = get_stream_token(stream_id);
    hdr.control = RIG_STREAM_CTRL_SUBSCRIBE;

    unsigned char pkt[RIG_STREAM_HEADER_SIZE];
    stream_packet_header_pack(&hdr, pkt);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(udp_port);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_CHECK(sendto(client_sock, pkt, sizeof(pkt), 0,
                      (struct sockaddr *)&dest, sizeof(dest))
               == (ssize_t)sizeof(pkt));

    unsigned char rbuf[2048];
    ssize_t n = udp_recv_timeout(client_sock, rbuf, sizeof(rbuf), 500);
    TEST_CHECK(n < 0);
    TEST_MSG("tampered subscribe was answered (n=%zd)", n);

    /* Compliant subscribe (source_id 0) still succeeds */
    n = subscribe_and_await_ack(client_sock, udp_port,
                                RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                rbuf, sizeof(rbuf));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    struct rig_stream_packet_header ack_hdr;
    TEST_CHECK(stream_packet_header_unpack(rbuf, n, &ack_hdr) == 0);
    TEST_CHECK(ack_hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Server-side anti-hijack: a SUBSCRIBE bearing the wrong subscribe_token must
 * be dropped (no ACK), while a correct-token SUBSCRIBE still succeeds. */
void test_stream_wrong_token_reject(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe carrying a wrong token — must be dropped. */
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_AUDIO_RX;
    hdr.stream_id = stream_id;
    hdr.subscribe_token = get_stream_token(stream_id) ^ 0x1u;
    hdr.control = RIG_STREAM_CTRL_SUBSCRIBE;

    unsigned char pkt[RIG_STREAM_HEADER_SIZE];
    stream_packet_header_pack(&hdr, pkt);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(udp_port);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_CHECK(sendto(client_sock, pkt, sizeof(pkt), 0,
                      (struct sockaddr *)&dest, sizeof(dest))
               == (ssize_t)sizeof(pkt));

    unsigned char rbuf[2048];
    ssize_t n = udp_recv_timeout(client_sock, rbuf, sizeof(rbuf), 500);
    TEST_CHECK(n < 0);
    TEST_MSG("wrong-token subscribe was answered (n=%zd)", n);

    /* Correct-token subscribe still succeeds. */
    n = subscribe_and_await_ack(client_sock, udp_port,
                                RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                rbuf, sizeof(rbuf));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    struct rig_stream_packet_header ack_hdr;
    TEST_CHECK(stream_packet_header_unpack(rbuf, n, &ack_hdr) == 0);
    TEST_CHECK(ack_hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* After subscribe, the first non-ACK packet should be a metadata frame. */
void test_rx_metadata_initial(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);


    /* Consume subscribe ACK */
    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    struct rig_stream_packet_header ack_hdr;
    TEST_CHECK(stream_packet_header_unpack(pkt, n, &ack_hdr) == 0);
    TEST_CHECK(ack_hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK);

    /* Next packet should be metadata (RIG_STREAM_CTRL_METADATA) */
    int got_metadata = 0;
    int i;

    for (i = 0; i < 5; i++)
    {
        n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            continue;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        if (hdr.control & RIG_STREAM_CTRL_METADATA)
        {
            got_metadata = 1;

            /* Verify metadata packet structure */
            TEST_CHECK(hdr.payload_len == RIG_STREAM_METADATA_WIRE_SIZE);
            TEST_MSG("metadata payload_len=%u, expected %u",
                     hdr.payload_len, RIG_STREAM_METADATA_WIRE_SIZE);

            TEST_CHECK(n == RIG_STREAM_HEADER_SIZE
                       + RIG_STREAM_METADATA_WIRE_SIZE);

            /* Unpack and verify metadata content */
            struct rig_stream_metadata meta;
            unsigned char *payload = pkt + pkt_data_offset(&hdr);
            size_t data_len = pkt_data_len(&hdr);
            int mret = stream_metadata_unpack(payload, data_len, &meta);
            TEST_CHECK(mret == 0);
            TEST_MSG("metadata unpack failed");

            if (mret == 0)
            {
                /* field_mask should have freq, vfo, ptt bits */
                TEST_CHECK(meta.field_mask & RIG_STREAM_META_VFO_FREQ);
                TEST_CHECK(meta.field_mask & RIG_STREAM_META_VFO_ID);
                TEST_CHECK(meta.field_mask & RIG_STREAM_META_PTT);

                /* Dummy backend defaults to freq > 0 */
                TEST_CHECK(meta.vfo_freq > 0);
                TEST_MSG("vfo_freq=%llu, expected > 0",
                         (unsigned long long)meta.vfo_freq);

                /* Default PTT is off */
                TEST_CHECK(meta.ptt == 0);
            }

            break;
        }
    }

    TEST_CHECK(got_metadata);
    TEST_MSG("no metadata packet received within 5 attempts");

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* --- TX feeder thread tests --- */

/* Open AUDIO_TX, send data packets, verify data reaches backend ringbuf. */
void test_tx_audio_data_accepted(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);
    TEST_CHECK(udp_port > 0);

    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_TX, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->backend_stream != NULL);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Build and send 3 data packets with small payload */
    unsigned char payload[960];
    memset(payload, 0xAB, sizeof(payload));

    int i;

    for (i = 0; i < 3; i++)
    {
        unsigned char pkt[2048];
        size_t pkt_len = build_data_packet(pkt, RIG_STREAM_TYPE_AUDIO_TX,
                                           (uint16_t)stream_id, (uint32_t)i,
                                           (uint64_t)(i * 480), 48000,
                                           RIG_STREAM_FMT_ID_PCM_S16, 1,
                                           payload, sizeof(payload));
        TEST_CHECK(send_data_pkt(client_sock, udp_port, pkt, pkt_len) == 0);
        TEST_MSG("failed to send TX data packet %d", i);
    }

    /* Verify data arrived in backend ring buffer */
    /* The dummy TX scheduler consumes the ring concurrently, so verify
     * arrival via the producer position: 3 packets x 480 frames. */
    WAIT_UNTIL(rig_stream_get_samples_written(s->backend_stream) >= 3 * 480);
    TEST_CHECK(rig_stream_get_samples_written(s->backend_stream) >= 3 * 480);
    TEST_MSG("TX feeder wrote %llu frames to backend ringbuf",
             (unsigned long long)
             rig_stream_get_samples_written(s->backend_stream));

    /* Verify feeder processed all 3 packets */
    TEST_CHECK(s->packet_count >= 3);
    TEST_MSG("packet_count: got %d, expected >= 3", s->packet_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Stereo TX timestamp must advance by whole frames, not whole samples.
 * For PCM_S16 (sample_size=2) channels=2, frame_bytes = 4. The feeder
 * advances stream->timestamp by data_len / frame_bytes. Sending B bytes
 * must yield timestamp == B/4, NOT B/2. Regression test for the channels
 * fix in the frame-size calculation. */
void test_tx_stereo_timestamp_frames(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* channels=2 is passed in the open command so the feeder caches the
     * correct frame size (sample_size * channels = 4) at thread start. */
    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000 channels=2",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);
    TEST_CHECK(udp_port > 0);

    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_TX, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->config.channels == 2);
    TEST_MSG("channels: got %d, expected 2", s->config.channels);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Send 3 packets of 960 bytes each. 960 is a multiple of 4 (frame size).
     * Total B = 2880 bytes; expected timestamp = B/4 = 720 frames. */
    const size_t payload_bytes = 960;
    const int num_packets = 3;
    const size_t total_bytes = payload_bytes * num_packets;
    unsigned char payload[960];
    memset(payload, 0xCD, sizeof(payload));

    int i;

    for (i = 0; i < num_packets; i++)
    {
        unsigned char pkt[2048];
        size_t pkt_len = build_data_packet(pkt, RIG_STREAM_TYPE_AUDIO_TX,
                                           (uint16_t)stream_id, (uint32_t)i,
                                           0, 48000,
                                           RIG_STREAM_FMT_ID_PCM_S16, 2,
                                           payload, payload_bytes);
        TEST_CHECK(send_data_pkt(client_sock, udp_port, pkt, pkt_len) == 0);
        TEST_MSG("failed to send stereo TX data packet %d", i);
    }

    /* Wait for the feeder to consume all packets. */
    WAIT_UNTIL(s->packet_count >= num_packets);

    uint64_t expected_ts = (uint64_t)total_bytes / 4;  /* frame size = 4 */
    TEST_CHECK(s->timestamp == expected_ts);
    TEST_MSG("timestamp: got %llu, expected %llu (B/4, not B/2=%llu)",
             (unsigned long long)s->timestamp,
             (unsigned long long)expected_ts,
             (unsigned long long)(total_bytes / 2));

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Open IQ_TX, send data packets, verify data reaches backend ringbuf. */
void test_tx_iq_data_accepted(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open IQ_TX IQ_CS16 192000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);
    TEST_CHECK(udp_port > 0);

    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_IQ_TX, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->backend_stream != NULL);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Build and send 3 IQ data packets (payload must be 4-byte aligned) */
    unsigned char payload[960];
    memset(payload, 0xCD, sizeof(payload));

    int i;

    for (i = 0; i < 3; i++)
    {
        unsigned char pkt[2048];
        size_t pkt_len = build_data_packet(pkt, RIG_STREAM_TYPE_IQ_TX,
                                           (uint16_t)stream_id, (uint32_t)i,
                                           (uint64_t)(i * 240), 192000,
                                           RIG_STREAM_FMT_ID_IQ_CS16, 2,
                                           payload, sizeof(payload));
        TEST_CHECK(send_data_pkt(client_sock, udp_port, pkt, pkt_len) == 0);
        TEST_MSG("failed to send TX IQ packet %d", i);
    }

    /* The dummy TX scheduler consumes the ring concurrently, so verify
     * arrival via the producer position: 3 packets x 240 IQ pairs. */
    WAIT_UNTIL(rig_stream_get_samples_written(s->backend_stream) >= 3 * 240);
    TEST_CHECK(rig_stream_get_samples_written(s->backend_stream) >= 3 * 240);
    TEST_MSG("TX feeder wrote %llu frames to backend ringbuf",
             (unsigned long long)
             rig_stream_get_samples_written(s->backend_stream));

    /* Verify feeder processed all 3 packets */
    TEST_CHECK(s->packet_count >= 3);
    TEST_MSG("packet_count: got %d, expected >= 3", s->packet_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Loopback: send counter pattern via TX, receive on RX, verify continuity. */
void test_tx_loopback_counter_audio(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);

    /* Set dummy backend to loopback mode */
    token_t tok = rig_token_lookup(rig, "stream_mode");
    TEST_CHECK(tok != 0);
    TEST_CHECK(rig_set_conf(rig, tok, "loopback") == RIG_OK);

    rigctld_stream_registry_init(&g_stream_registry);
    char buf[1024];

    /* Open TX first so loopback thread can find it as peer */
    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_F32 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int tx_id = -1, tx_port = -1;
    TEST_CHECK(parse_open_response(buf, &tx_id, &tx_port) == 0);

    /* Open RX — loopback thread starts reading from TX ringbuf */
    ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_F32 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int rx_id = -1, rx_port = -1;
    TEST_CHECK(parse_open_response(buf, &rx_id, &rx_port) == 0);

    /* Subscribe on RX and consume ACK */
    int rx_sock = create_client_udp_socket();
    TEST_CHECK(rx_sock >= 0);

    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(rx_sock, rx_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, rx_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);  /* ACK */

    /* Create TX client socket and send 5 counter-pattern packets */
    int tx_sock = create_client_udp_socket();
    TEST_CHECK(tx_sock >= 0);

    unsigned char payload[960];
    uint8_t counter = 0;
    int i;

    for (i = 0; i < 5; i++)
    {
        size_t j;

        for (j = 0; j < sizeof(payload); j++)
        {
            payload[j] = counter++;
        }

        unsigned char tx_pkt[2048];
        size_t pkt_len = build_data_packet(tx_pkt, RIG_STREAM_TYPE_AUDIO_TX,
                                           (uint16_t)tx_id, (uint32_t)i,
                                           (uint64_t)(i * 480), 48000,
                                           RIG_STREAM_FMT_ID_PCM_S16, 1,
                                           payload, sizeof(payload));
        TEST_CHECK(send_data_pkt(tx_sock, tx_port, tx_pkt, pkt_len) == 0);
    }

    /* Receive on RX and verify counter pattern continuity */
    uint8_t expected_byte = 0;
    int data_count = 0;
    int first_data = 1;

    for (i = 0; i < 30 && data_count < 3; i++)
    {
        n = udp_recv_timeout(rx_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            break;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0)
        {
            continue;  /* skip non-data and time-only frames */
        }

        unsigned char *rx_payload = pkt + pkt_data_offset(&hdr);
        size_t data_len = pkt_data_len(&hdr);

        if (first_data)
        {
            expected_byte = rx_payload[0];
            expected_byte++;
            first_data = 0;

            if (data_len > 1)
            {
                uint8_t check = rx_payload[0];
                check++;
                int vret = verify_counter_payload(rx_payload + 1,
                                                  data_len - 1,
                                                  &check);
                TEST_CHECK(vret == 0);
                TEST_MSG("loopback: counter gap in first packet");
                expected_byte = check;
            }
        }
        else
        {
            int vret = verify_counter_payload(rx_payload, data_len,
                                              &expected_byte);
            TEST_CHECK(vret == 0);
            TEST_MSG("loopback: counter gap in packet %d", data_count);
        }

        data_count++;
    }

    TEST_CHECK(data_count >= 3);
    TEST_MSG("loopback: only got %d data packets, expected >= 3", data_count);

    close(tx_sock);
    close(rx_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", rx_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", tx_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Loopback: send counter pattern via IQ_TX, receive on IQ_RX, verify. */
void test_tx_loopback_counter_iq(void)
{
    RIG *rig = test_rig_setup();
    TEST_CHECK(rig != NULL);

    /* Set dummy backend to loopback mode */
    token_t tok = rig_token_lookup(rig, "stream_mode");
    TEST_CHECK(tok != 0);
    TEST_CHECK(rig_set_conf(rig, tok, "loopback") == RIG_OK);

    rigctld_stream_registry_init(&g_stream_registry);
    char buf[1024];

    /* Open IQ_TX first so loopback thread can find it as peer */
    int ret = run_cmd(rig, "\\stream_open IQ_TX IQ_CF32 192000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int tx_id = -1, tx_port = -1;
    TEST_CHECK(parse_open_response(buf, &tx_id, &tx_port) == 0);

    /* Open IQ_RX — loopback thread starts reading from TX ringbuf */
    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CF32 192000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int rx_id = -1, rx_port = -1;
    TEST_CHECK(parse_open_response(buf, &rx_id, &rx_port) == 0);

    /* Subscribe on RX and consume ACK */
    int rx_sock = create_client_udp_socket();
    TEST_CHECK(rx_sock >= 0);

    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(rx_sock, rx_port,
                                        RIG_STREAM_TYPE_IQ_RX, rx_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);  /* ACK */

    /* Create TX client socket and send 5 counter-pattern IQ packets */
    int tx_sock = create_client_udp_socket();
    TEST_CHECK(tx_sock >= 0);

    /* IQ_CS16: 4 bytes per sample, keep payload 4-byte aligned */
    unsigned char payload[960];
    uint8_t counter = 0;
    int i;

    for (i = 0; i < 5; i++)
    {
        size_t j;

        for (j = 0; j < sizeof(payload); j++)
        {
            payload[j] = counter++;
        }

        unsigned char tx_pkt[2048];
        size_t pkt_len = build_data_packet(tx_pkt, RIG_STREAM_TYPE_IQ_TX,
                                           (uint16_t)tx_id, (uint32_t)i,
                                           (uint64_t)(i * 240), 192000,
                                           RIG_STREAM_FMT_ID_IQ_CS16, 2,
                                           payload, sizeof(payload));
        TEST_CHECK(send_data_pkt(tx_sock, tx_port, tx_pkt, pkt_len) == 0);
    }

    /* Receive on RX and verify counter pattern continuity */
    uint8_t expected_byte = 0;
    int data_count = 0;
    int first_data = 1;

    for (i = 0; i < 30 && data_count < 3; i++)
    {
        n = udp_recv_timeout(rx_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            break;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0)
        {
            continue;  /* skip non-data and time-only frames */
        }

        unsigned char *rx_payload = pkt + pkt_data_offset(&hdr);
        size_t data_len = pkt_data_len(&hdr);

        if (first_data)
        {
            expected_byte = rx_payload[0];
            expected_byte++;
            first_data = 0;

            if (data_len > 1)
            {
                uint8_t check = rx_payload[0];
                check++;
                int vret = verify_counter_payload(rx_payload + 1,
                                                  data_len - 1,
                                                  &check);
                TEST_CHECK(vret == 0);
                TEST_MSG("IQ loopback: counter gap in first packet");
                expected_byte = check;
            }
        }
        else
        {
            int vret = verify_counter_payload(rx_payload, data_len,
                                              &expected_byte);
            TEST_CHECK(vret == 0);
            TEST_MSG("IQ loopback: counter gap in packet %d", data_count);
        }

        data_count++;
    }

    TEST_CHECK(data_count >= 3);
    TEST_MSG("IQ loopback: only got %d data packets, expected >= 3",
             data_count);

    close(tx_sock);
    close(rx_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", rx_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", tx_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Send packets with a sequence gap, verify gap_count is tracked. */
void test_tx_sequence_gap(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_TX, stream_id);
    TEST_CHECK(s != NULL);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    unsigned char payload[480];
    memset(payload, 0, sizeof(payload));

    /* Send seq 0, 1, 5 — gap of 3 (missing 2, 3, 4) */
    uint32_t seqs[] = { 0, 1, 5 };
    int i;

    for (i = 0; i < 3; i++)
    {
        unsigned char pkt[2048];
        size_t pkt_len = build_data_packet(pkt, RIG_STREAM_TYPE_AUDIO_TX,
                                           (uint16_t)stream_id, seqs[i],
                                           (uint64_t)(seqs[i] * 240), 48000,
                                           RIG_STREAM_FMT_ID_PCM_S16, 1,
                                           payload, sizeof(payload));
        TEST_CHECK(send_data_pkt(client_sock, udp_port, pkt, pkt_len) == 0);
    }

    WAIT_UNTIL(s->packet_count >= 3);

    TEST_CHECK(s->packet_count == 3);
    TEST_MSG("packet_count: got %d, expected 3", s->packet_count);

    TEST_CHECK(s->gap_count == 3);
    TEST_MSG("gap_count: got %d, expected 3 (missed seq 2,3,4)", s->gap_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* An implausibly large forward seq jump (> RIGCTLD_MAX_PLAUSIBLE_SEQ_GAP)
 * counts as a SINGLE discontinuity, not the literal (huge) gap. Regression
 * test for the gap-clamp fix. */
void test_tx_sequence_gap_implausible(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_TX, stream_id);
    TEST_CHECK(s != NULL);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    unsigned char payload[480];
    memset(payload, 0, sizeof(payload));

    /* seq 0 establishes the baseline (first data packet skips gap check),
     * then a forward jump to 2<<20. The gap is (2<<20)-1 ~= 2.1M, which is
     * > RIGCTLD_MAX_PLAUSIBLE_SEQ_GAP (1<<20) but < 2^31 (forward), so it is
     * counted as exactly one gap rather than ~2 million. */
    uint32_t seqs[] = { 0, (2u << 20) };
    int i;

    for (i = 0; i < 2; i++)
    {
        unsigned char pkt[2048];
        size_t pkt_len = build_data_packet(pkt, RIG_STREAM_TYPE_AUDIO_TX,
                                           (uint16_t)stream_id, seqs[i],
                                           0, 48000,
                                           RIG_STREAM_FMT_ID_PCM_S16, 1,
                                           payload, sizeof(payload));
        TEST_CHECK(send_data_pkt(client_sock, udp_port, pkt, pkt_len) == 0);
    }

    WAIT_UNTIL(s->packet_count >= 2);

    TEST_CHECK(s->packet_count == 2);
    TEST_MSG("packet_count: got %d, expected 2", s->packet_count);

    /* The whole point: a single implausible jump is one gap, not 2097151. */
    TEST_CHECK(s->gap_count == 1);
    TEST_MSG("gap_count: got %u, expected 1 (implausible jump clamped)",
             s->gap_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Send a metadata frame via TX, verify rig frequency changes. */
void test_tx_metadata_applied(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Build metadata packet: set freq to 7.050 MHz */
    struct rig_stream_metadata meta;
    memset(&meta, 0, sizeof(meta));
    meta.field_mask = RIG_STREAM_META_VFO_FREQ;
    meta.vfo_freq = 7050000;

    unsigned char pkt[RIG_STREAM_HEADER_SIZE + RIG_STREAM_METADATA_WIRE_SIZE];
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = 1;
    hdr.type = RIG_STREAM_TYPE_AUDIO_TX;
    hdr.stream_id = (uint16_t)stream_id;
    hdr.subscribe_token = get_stream_token(stream_id);
    hdr.control = RIG_STREAM_CTRL_METADATA;
    hdr.payload_len = RIG_STREAM_METADATA_WIRE_SIZE;
    hdr.sample_rate = 48000;
    hdr.format = RIG_STREAM_FMT_ID_PCM_S16;
    hdr.channels = 1;
    stream_packet_header_pack(&hdr, pkt);
    stream_metadata_pack(&meta, pkt + RIG_STREAM_HEADER_SIZE);

    TEST_CHECK(send_data_pkt(client_sock, udp_port, pkt,
                             RIG_STREAM_HEADER_SIZE
                             + RIG_STREAM_METADATA_WIRE_SIZE) == 0);

    /* The metadata handler applies the frequency and bumps packet_count
     * together, so wait for the packet to be counted. */
    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_TX, stream_id);
    TEST_CHECK(s != NULL);
    WAIT_UNTIL(s->packet_count >= 1);

    /* Verify rig frequency changed */
    freq_t freq = 0;
    ret = rig_get_freq(rig, RIG_VFO_CURR, &freq);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(freq == 7050000);
    TEST_MSG("freq: got %.0f, expected 7050000", (double)freq);

    /* Verify metadata packet was counted */
    TEST_CHECK(s->packet_count >= 1);
    TEST_MSG("packet_count: got %d, expected >= 1", s->packet_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* A metadata frame whose header claims a 20-byte payload but carries no body
 * must be dropped, not acted on (no out-of-datagram read, no rig_set_freq). */
void test_tx_metadata_truncated_rejected(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    /* Establish a known baseline frequency to detect any spurious change. */
    rig_set_freq(rig, RIG_VFO_CURR, 14000000);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Header advertises a metadata payload but we send only the header. */
    unsigned char pkt[RIG_STREAM_HEADER_SIZE];
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = 1;
    hdr.type = RIG_STREAM_TYPE_AUDIO_TX;
    hdr.stream_id = (uint16_t)stream_id;
    hdr.subscribe_token = get_stream_token(stream_id);
    hdr.control = RIG_STREAM_CTRL_METADATA;
    hdr.payload_len = RIG_STREAM_METADATA_WIRE_SIZE;
    hdr.sample_rate = 48000;
    hdr.format = RIG_STREAM_FMT_ID_PCM_S16;
    hdr.channels = 1;
    stream_packet_header_pack(&hdr, pkt);

    TEST_CHECK(send_data_pkt(client_sock, udp_port, pkt,
                             RIG_STREAM_HEADER_SIZE) == 0);

    usleep(100000);

    /* Frequency must be unchanged by the malformed frame. */
    freq_t freq = 0;
    ret = rig_get_freq(rig, RIG_VFO_CURR, &freq);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(freq == 14000000);
    TEST_MSG("freq: got %.0f, expected 14000000 (malformed frame ignored)",
             (double)freq);

    /* The frame must be dropped before the packet counter is bumped. */
    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_TX, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->packet_count == 0);
    TEST_MSG("packet_count: got %d, expected 0", s->packet_count);

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* --- Stream control command tests --- */

void test_cmd_stream_pause_resume(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_RX, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->backend_stream != NULL);
    TEST_CHECK(s->backend_stream->paused == 0);

    /* Pause */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_pause %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(s->backend_stream->paused == 1);
    TEST_MSG("paused: got %d, expected 1", s->backend_stream->paused);

    /* Resume */
    snprintf(cmd, sizeof(cmd), "\\stream_resume %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(s->backend_stream->paused == 0);
    TEST_MSG("paused: got %d, expected 0", s->backend_stream->paused);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_mute_unmute(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    struct rigctld_stream *s = rigctld_stream_registry_lookup(
                                   &g_stream_registry, RIG_STREAM_TYPE_AUDIO_RX, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->backend_stream != NULL);
    TEST_CHECK(s->backend_stream->muted == 0);

    /* Mute */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_mute %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(s->backend_stream->muted == 1);
    TEST_MSG("muted: got %d, expected 1", s->backend_stream->muted);

    /* Unmute */
    snprintf(cmd, sizeof(cmd), "\\stream_unmute %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(s->backend_stream->muted == 0);
    TEST_MSG("muted: got %d, expected 0", s->backend_stream->muted);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_pause_not_found(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_pause 99", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero", ret);

    stream_test_end(rig);
}


void test_cmd_stream_status(void)
{
    RIG *rig = stream_test_begin();
    char buf[2048];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);
    ret = run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, "AUDIO_RX") != NULL);
    TEST_MSG("status output: %s", buf);
    TEST_CHECK(strstr(buf, "48000") != NULL);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_status_ext_resp(void)
{
    RIG *rig = stream_test_begin();
    char buf[2048];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);
    ret = run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int paused_val = -1, muted_val = -1, rate_val = -1;
    TEST_CHECK(parse_ext_int(buf, "paused", &paused_val) == 0);
    TEST_CHECK(paused_val == 0);
    TEST_MSG("paused: got %d, expected 0", paused_val);
    TEST_CHECK(parse_ext_int(buf, "muted", &muted_val) == 0);
    TEST_CHECK(muted_val == 0);
    TEST_CHECK(parse_ext_int(buf, "sample_rate", &rate_val) == 0);
    TEST_CHECK(rate_val == 48000);
    TEST_MSG("sample_rate: got %d, expected 48000", rate_val);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_status_not_found(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_status 99", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero", ret);

    stream_test_end(rig);
}


void test_cmd_stream_metadata_read(void)
{
    RIG *rig = stream_test_begin();
    char buf[2048];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    /* Set frequency so metadata has something to report */
    rig_set_freq(rig, RIG_VFO_CURR, 14200000.0);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_metadata_read %d", stream_id);
    ret = run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, "vfo_freq: 14200000") != NULL);
    TEST_MSG("metadata_get output: %s", buf);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_drain(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_ASSERT(s != NULL && s->backend_stream != NULL);

    /* Drain has to be given something to drain: on an empty ring it returns
     * immediately and would pass even if it never waited. ~85 ms of audio at
     * this rate fits the 250 ms ring and cannot be consumed by the paced
     * backend before the check below, yet drains well inside its 1 s timeout. */
    static int16_t pcm[4096];
    size_t written = 0;
    TEST_CHECK(rig_stream_write(rig, s->backend_stream, pcm, sizeof(pcm),
                                &written, 1000, NULL) == RIG_OK);
    TEST_CHECK(written > 0);

    struct rig_stream *bs = s->backend_stream;
    TEST_CHECK(stream_ringbuf_available(&bs->ringbuf) > 0);
    TEST_MSG("ring already empty before drain; this test cannot show draining");

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_drain %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(stream_ringbuf_available(&bs->ringbuf) == 0);
    TEST_MSG("drain returned but left %zu bytes in the ring",
             stream_ringbuf_available(&bs->ringbuf));

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_drain_not_found(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_drain 99", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero", ret);

    stream_test_end(rig);
}


void test_cmd_stream_list(void)
{
    RIG *rig = stream_test_begin();
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id1 = -1, port1 = -1;
    parse_open_response(buf, &id1, &port1);

    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 192000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id2 = -1, port2 = -1;
    parse_open_response(buf, &id2, &port2);

    ret = run_cmd_ext(rig, "\\stream_list", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, "AUDIO_RX") != NULL);
    TEST_CHECK(strstr(buf, "IQ_RX") != NULL);
    TEST_MSG("stream_list output:\n%s", buf);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", id1);
    run_cmd(rig, cmd, buf, sizeof(buf));
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", id2);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_list_empty(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd_ext(rig, "\\stream_list", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, "AUDIO_RX") == NULL);
    TEST_CHECK(strstr(buf, "IQ_RX") == NULL);

    stream_test_end(rig);
}


void test_cmd_stream_list_after_close(void)
{
    RIG *rig = stream_test_begin();
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int audio_id = -1, audio_port = -1;
    parse_open_response(buf, &audio_id, &audio_port);

    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 192000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    /* Close the audio stream */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", audio_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    /* List — should only show IQ_RX */
    ret = run_cmd_ext(rig, "\\stream_list", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    TEST_CHECK(strstr(buf, "AUDIO_RX") == NULL);
    TEST_CHECK(strstr(buf, "IQ_RX") != NULL);
    TEST_MSG("stream_list output:\n%s", buf);

    stream_test_end(rig);
}


/* Verify mute produces zero payloads on the UDP data path */
void test_rx_mute_zeros(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);
    unsigned char pkt[2048];
    TEST_CHECK(subscribe_and_await_ack(client_sock, udp_port,
                                       RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                       pkt, sizeof(pkt)) >= 0);

    /* Skip any remaining control frames to reach a data packet */
    struct rig_stream_packet_header hdr;
    int got_nonzero = 0;

    for (int i = 0; i < 20; i++)
    {
        ssize_t n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE) { continue; }

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0) { continue; }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0) { continue; }

        /* Check for any non-zero byte in payload */
        size_t payload_len = (size_t)n - RIG_STREAM_HEADER_SIZE;

        for (size_t j = 0; j < payload_len; j++)
        {
            if (pkt[RIG_STREAM_HEADER_SIZE + j] != 0)
            {
                got_nonzero = 1;
                break;
            }
        }

        if (got_nonzero) { break; }
    }

    TEST_CHECK(got_nonzero == 1);
    TEST_MSG("Pre-mute: expected non-zero audio data");

    /* Mute the stream */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_mute %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    /* Allow time for mute to take effect in feeder thread */
    usleep(50000);

    /* Receive data packets — payload should be all zeros */
    int zero_packets = 0;

    for (int i = 0; i < 20; i++)
    {
        ssize_t n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE) { continue; }

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0) { continue; }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0) { continue; }

        size_t data_off = pkt_data_offset(&hdr);
        size_t data_len = pkt_data_len(&hdr);
        int all_zero = 1;

        for (size_t j = 0; j < data_len; j++)
        {
            if (pkt[data_off + j] != 0)
            {
                all_zero = 0;
                break;
            }
        }

        if (all_zero && data_len > 0)
        {
            zero_packets++;
        }

        if (zero_packets >= 3) { break; }
    }

    TEST_CHECK(zero_packets >= 3);
    TEST_MSG("Muted: got %d zero-payload packets, expected >= 3", zero_packets);

    /* Unmute and verify non-zero data resumes */
    snprintf(cmd, sizeof(cmd), "\\stream_unmute %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    usleep(50000);

    got_nonzero = 0;

    for (int i = 0; i < 20; i++)
    {
        ssize_t n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);

        if (n < RIG_STREAM_HEADER_SIZE) { continue; }

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0) { continue; }

        if (!pkt_is_data(&hdr) || pkt_data_len(&hdr) == 0) { continue; }

        size_t payload_len = (size_t)n - RIG_STREAM_HEADER_SIZE;

        for (size_t j = 0; j < payload_len; j++)
        {
            if (pkt[RIG_STREAM_HEADER_SIZE + j] != 0)
            {
                got_nonzero = 1;
                break;
            }
        }

        if (got_nonzero) { break; }
    }

    TEST_CHECK(got_nonzero == 1);
    TEST_MSG("Post-unmute: expected non-zero audio data");

    close(client_sock);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


/* #9: Verify ALL stream_status fields */
void test_cmd_stream_status_all_fields(void)
{
    RIG *rig = stream_test_begin();
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);
    ret = run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int val;
    TEST_CHECK(parse_ext_int(buf, "stream_id", &val) == 0);
    TEST_CHECK(val == stream_id);
    TEST_MSG("stream_id: got %d, expected %d", val, stream_id);

    TEST_CHECK(strstr(buf, "type: AUDIO_RX") != NULL);
    TEST_MSG("missing type: AUDIO_RX");

    TEST_CHECK(parse_ext_int(buf, "sample_rate", &val) == 0);
    TEST_CHECK(val == 48000);

    TEST_CHECK(strstr(buf, "format: PCM_S16") != NULL);
    TEST_MSG("missing format: PCM_S16");

    TEST_CHECK(parse_ext_int(buf, "channels", &val) == 0);
    TEST_CHECK(val == 1);
    TEST_MSG("channels: got %d, expected 1", val);

    TEST_CHECK(parse_ext_int(buf, "udp_port", &val) == 0);
    TEST_CHECK(val == udp_port);
    TEST_MSG("udp_port: got %d, expected %d", val, udp_port);

    TEST_CHECK(parse_ext_int(buf, "paused", &val) == 0);
    TEST_CHECK(val == 0);

    TEST_CHECK(parse_ext_int(buf, "muted", &val) == 0);
    TEST_CHECK(val == 0);

    /* PCM_S16 against the PCM_F32-native dummy is a converted stream. */
    TEST_CHECK(parse_ext_int(buf, "conversions", &val) == 0);
    TEST_CHECK(val == RIG_STREAM_CONV_FORMAT);
    TEST_MSG("status conversions: got %d, expected %d", val,
             RIG_STREAM_CONV_FORMAT);

    TEST_CHECK(parse_ext_int(buf, "packet_count", &val) == 0);
    TEST_CHECK(val >= 0);

    TEST_CHECK(parse_ext_int(buf, "gap_count", &val) == 0);
    TEST_CHECK(val == 0);

    TEST_CHECK(parse_ext_int(buf, "overruns", &val) == 0);
    TEST_CHECK(val >= 0);

    TEST_CHECK(parse_ext_int(buf, "underruns", &val) == 0);
    TEST_CHECK(val >= 0);

    TEST_CHECK(parse_ext_int(buf, "backend_gaps", &val) == 0);
    TEST_CHECK(val >= 0);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


/* #10: Status reflects pause/mute state changes */
void test_cmd_stream_status_after_pause_mute(void)
{
    RIG *rig = stream_test_begin();
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    int val;

    /* Pause and verify status */
    snprintf(cmd, sizeof(cmd), "\\stream_pause %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);
    run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(parse_ext_int(buf, "paused", &val) == 0);
    TEST_CHECK(val == 1);
    TEST_MSG("after pause, paused: got %d, expected 1", val);

    /* Mute and verify status */
    snprintf(cmd, sizeof(cmd), "\\stream_mute %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);
    run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(parse_ext_int(buf, "muted", &val) == 0);
    TEST_CHECK(val == 1);
    TEST_MSG("after mute, muted: got %d, expected 1", val);
    TEST_CHECK(parse_ext_int(buf, "paused", &val) == 0);
    TEST_CHECK(val == 1);
    TEST_MSG("after mute, paused still: got %d, expected 1", val);

    /* Resume and verify */
    snprintf(cmd, sizeof(cmd), "\\stream_resume %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);
    run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(parse_ext_int(buf, "paused", &val) == 0);
    TEST_CHECK(val == 0);
    TEST_MSG("after resume, paused: got %d, expected 0", val);
    TEST_CHECK(parse_ext_int(buf, "muted", &val) == 0);
    TEST_CHECK(val == 1);
    TEST_MSG("after resume, muted still: got %d, expected 1", val);

    /* Unmute and verify clean state */
    snprintf(cmd, sizeof(cmd), "\\stream_unmute %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    snprintf(cmd, sizeof(cmd), "\\stream_status %d", stream_id);
    run_cmd_ext(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(parse_ext_int(buf, "paused", &val) == 0);
    TEST_CHECK(val == 0);
    TEST_CHECK(parse_ext_int(buf, "muted", &val) == 0);
    TEST_CHECK(val == 0);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


/* #11: Verify ALL stream_list fields precisely */
void test_cmd_stream_list_all_fields(void)
{
    RIG *rig = stream_test_begin();
    g_stream_registry.source_id = 0x0777;
    char buf[4096];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    ret = run_cmd_ext(rig, "\\stream_list", buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int val;
    TEST_CHECK(parse_ext_int(buf, "stream_id", &val) == 0);
    TEST_CHECK(val == stream_id);
    TEST_MSG("list stream_id: got %d, expected %d", val, stream_id);

    TEST_CHECK(parse_ext_int(buf, "source_id", &val) == 0);
    TEST_CHECK(val == 0x0777);
    TEST_MSG("list source_id: got %d, expected %d", val, 0x0777);

    TEST_CHECK(strstr(buf, "type: AUDIO_RX") != NULL);
    TEST_CHECK(strstr(buf, "format: PCM_S16") != NULL);

    TEST_CHECK(parse_ext_int(buf, "sample_rate", &val) == 0);
    TEST_CHECK(val == 48000);

    TEST_CHECK(parse_ext_int(buf, "channels", &val) == 0);
    TEST_CHECK(val == 1);

    TEST_CHECK(parse_ext_int(buf, "udp_port", &val) == 0);
    TEST_CHECK(val == udp_port);

    TEST_CHECK(parse_ext_int(buf, "paused", &val) == 0);
    TEST_CHECK(val == 0);

    TEST_CHECK(parse_ext_int(buf, "muted", &val) == 0);
    TEST_CHECK(val == 0);

    /* PCM_S16 against the PCM_F32-native dummy is a converted stream. */
    TEST_CHECK(parse_ext_int(buf, "conversions", &val) == 0);
    TEST_CHECK(val == RIG_STREAM_CONV_FORMAT);
    TEST_MSG("list conversions: got %d, expected %d", val,
             RIG_STREAM_CONV_FORMAT);

    snprintf(buf, sizeof(buf), "\\stream_close %d", stream_id);
    run_cmd(rig, buf, buf, sizeof(buf));
    stream_test_end(rig);
}


/* #12: Missing error-path tests */
void test_cmd_stream_mute_not_found(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_mute 99", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero", ret);

    stream_test_end(rig);
}


void test_cmd_stream_unmute_not_found(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_unmute 99", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero", ret);

    stream_test_end(rig);
}


void test_cmd_stream_resume_not_found(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_resume 99", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero", ret);

    stream_test_end(rig);
}


void test_cmd_stream_metadata_read_not_found(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_metadata_read 99", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero", ret);

    stream_test_end(rig);
}


/* #13: Non-ext_resp streaming commands are rejected */
void test_cmd_stream_requires_ext_resp(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    /* Send stream_open WITHOUT ext_resp (no + prefix).
     * Use rigctl_parse directly since run_cmd auto-adds +. */
    FILE *fin = tmpfile();
    FILE *fout = tmpfile();
    int vfo_opt = 0;
    int ext_resp = 0;
    char resp_sep = '\n';

    fprintf(fin, "\\stream_open AUDIO_RX PCM_S16 48000\n");
    rewind(fin);

    errno = 0;
    int retval = rigctl_parse(rig, fin, fout, NULL, 0, NULL,
                              1, 0, &vfo_opt, '\n', &ext_resp, &resp_sep, 0);

    fflush(fout);
    long len = ftell(fout);
    rewind(fout);

    if (len > 0 && (size_t)len < sizeof(buf))
    {
        fread(buf, 1, len, fout);
        buf[len] = '\0';
    }
    else
    {
        buf[0] = '\0';
    }

    fclose(fin);
    fclose(fout);

    /* Should be rejected with EPROTO error */
    TEST_CHECK(retval != 0);
    TEST_MSG("non-ext_resp retval: got %d, expected nonzero", retval);
    TEST_CHECK(strstr(buf, "RPRT") != NULL);
    TEST_MSG("output: %s", buf);

    /* No stream should have been created */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


/* #14: Idempotent toggle tests */
void test_cmd_stream_pause_already_paused(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_pause %d", stream_id);

    /* Pause twice — second should succeed without error */
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("double pause: got %d, expected 0", ret);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_resume_not_paused(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_resume %d", stream_id);

    /* Resume without prior pause — should succeed */
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("resume non-paused: got %d, expected 0", ret);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_mute_already_muted(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_mute %d", stream_id);

    /* Mute twice — second should succeed */
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("double mute: got %d, expected 0", ret);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


void test_cmd_stream_unmute_not_muted(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_unmute %d", stream_id);

    /* Unmute without prior mute — should succeed */
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("unmute non-muted: got %d, expected 0", ret);

    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    stream_test_end(rig);
}


/* --- Client disconnect cleanup tests --- */

void test_cmd_disconnect_cleans_streams(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Open two streams */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id1 = -1, port1 = -1;
    parse_open_response(buf, &id1, &port1);

    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id2 = -1, port2 = -1;
    parse_open_response(buf, &id2, &port2);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 2);

    /* Set client_id on both streams (simulating what rigctld.c will do) */
    int fake_client_id = 42;
    struct rigctld_stream *s1 = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id1);
    struct rigctld_stream *s2 = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id2);
    TEST_CHECK(s1 != NULL);
    TEST_CHECK(s2 != NULL);

    if (!s1 || !s2)
    {
        stream_test_end(rig);
        return;
    }

    s1->client_id = fake_client_id;
    s2->client_id = fake_client_id;

    /* Verify feeder threads are running */
    TEST_CHECK(s1->running == 1);
    TEST_CHECK(s2->running == 1);

    /* Simulate client disconnect */
    int closed = rigctld_stream_registry_close_by_client(
                     &g_stream_registry, fake_client_id);
    TEST_CHECK(closed == 2);
    TEST_MSG("close_by_client returned %d, expected 2", closed);

    /* Registry should be empty */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_disconnect_leaves_other_clients(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Open two streams */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id1 = -1, port1 = -1;
    parse_open_response(buf, &id1, &port1);

    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id2 = -1, port2 = -1;
    parse_open_response(buf, &id2, &port2);

    /* Assign different client IDs */
    struct rigctld_stream *s1 = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id1);
    struct rigctld_stream *s2 = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id2);
    TEST_CHECK(s1 != NULL);
    TEST_CHECK(s2 != NULL);

    if (!s1 || !s2)
    {
        stream_test_end(rig);
        return;
    }

    s1->client_id = 10;
    s2->client_id = 20;

    /* Close only client 10 */
    int closed = rigctld_stream_registry_close_by_client(
                     &g_stream_registry, 10);
    TEST_CHECK(closed == 1);
    TEST_MSG("close_by_client returned %d, expected 1", closed);

    /* Client 20's stream should remain */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 1);
    TEST_CHECK(rigctld_stream_registry_find_by_id(
                   &g_stream_registry, id2) != NULL);

    /* Clean up remaining stream properly */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", id2);
    run_cmd(rig, cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


void test_cmd_stream_metadata_interval_from_registry(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Set custom metadata interval on registry */
    g_stream_registry.metadata_interval_ms = 50;

    /* Open a stream */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (!s)
    {
        stream_test_end(rig);
        return;
    }

    /* Verify stream inherited registry's metadata interval */
    TEST_CHECK(s->metadata_interval_ms == 50);
    TEST_MSG("metadata_interval_ms=%d, expected 50", s->metadata_interval_ms);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


void test_client_id_from_pthread_key(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Set up pthread key and assign client_id=99 for this thread */
    rigctld_client_id_init();
    rigctld_client_id_set(99);

    /* Open a stream — handler should pick up client_id from pthread key */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (!s)
    {
        stream_test_end(rig);
        return;
    }

    /* Verify stream_open read client_id from pthread key, not default 0 */
    TEST_CHECK(s->client_id == 99);
    TEST_MSG("client_id=%d, expected 99", s->client_id);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


void test_disconnect_cleanup_via_pthread_key(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Set client_id=77 via pthread key */
    rigctld_client_id_init();
    rigctld_client_id_set(77);

    /* Open two streams — both should inherit client_id=77 */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id1 = -1, port1 = -1;
    parse_open_response(buf, &id1, &port1);

    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id2 = -1, port2 = -1;
    parse_open_response(buf, &id2, &port2);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 2);

    /* Verify both streams got client_id from pthread key */
    struct rigctld_stream *s1 = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id1);
    struct rigctld_stream *s2 = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id2);
    TEST_CHECK(s1 != NULL && s1->client_id == 77);
    TEST_CHECK(s2 != NULL && s2->client_id == 77);

    /* Simulate disconnect — close_by_client using the key-assigned ID */
    int closed = rigctld_stream_registry_close_by_client(
                     &g_stream_registry, 77);
    TEST_CHECK(closed == 2);
    TEST_MSG("close_by_client returned %d, expected 2", closed);
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_stream_status_non_numeric_id(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_status abc", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero for non-numeric id", ret);

    stream_test_end(rig);
}


void test_cmd_stream_pause_non_numeric_id(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_pause xyz", buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("ret: got %d, expected nonzero for non-numeric id", ret);

    stream_test_end(rig);
}


void test_cmd_feeder_exits_without_subscribe(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    /* Open an RX stream — feeder starts and waits for subscribe */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);
    TEST_CHECK(stream_id > 0);

    /* Close immediately without ever sending a subscribe packet.
     * This verifies the feeder thread exits cleanly via its select()
     * timeout + running flag check, without hanging. */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    ret = run_cmd(rig, cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_close ret=%d, expected 0 (no hang)", ret);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_close_by_client_idempotent(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    /* Set client_id via pthread key */
    rigctld_client_id_init();
    rigctld_client_id_set(88);

    /* Open a stream — inherits client_id=88 */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    /* First close_by_client should clean up 1 stream */
    int closed = rigctld_stream_registry_close_by_client(
                     &g_stream_registry, 88);
    TEST_CHECK(closed == 1);
    TEST_MSG("first close_by_client returned %d, expected 1", closed);

    /* Second call should be idempotent — returns 0, no crash */
    closed = rigctld_stream_registry_close_by_client(
                 &g_stream_registry, 88);
    TEST_CHECK(closed == 0);
    TEST_MSG("second close_by_client returned %d, expected 0", closed);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


/* --- Multicast streaming tests --- */


/* Parse multicast address from ext_resp output (e.g. "multicast: 239.1.2.3\n") */
static int parse_ext_str(const char *buf, const char *key,
                         char *out, size_t out_size)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s: ", key);
    const char *p = strstr(buf, pattern);

    if (!p)
    {
        return -1;
    }

    p += strlen(pattern);
    size_t i = 0;

    while (*p && *p != '\n' && i < out_size - 1)
    {
        out[i++] = *p++;
    }

    out[i] = '\0';
    return 0;
}


void test_cmd_stream_open_multicast_ipv4(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 multicast=239.1.2.3:5000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_open returned %d", ret);

    /* Parse response — should include stream_id, udp_port, multicast */
    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);
    TEST_MSG("response: '%s'", buf);
    TEST_CHECK(stream_id > 0);
    TEST_CHECK(udp_port == 5000);
    TEST_MSG("udp_port: got %d, expected 5000", udp_port);

    /* Verify multicast address in response */
    char mcast_addr[64] = "";
    TEST_CHECK(parse_ext_str(buf, "multicast", mcast_addr,
                             sizeof(mcast_addr)) == 0);
    TEST_CHECK(strcmp(mcast_addr, "239.1.2.3") == 0);
    TEST_MSG("multicast: got '%s', expected '239.1.2.3'", mcast_addr);

    /* Verify stream is registered as multicast */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->multicast == 1);
    TEST_MSG("multicast flag: got %d, expected 1", s->multicast);

    /* Clean up */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


void test_cmd_stream_open_multicast_tx_rejected(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    /* TX multicast should be rejected */
    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_TX PCM_S16 48000 multicast=239.1.2.3:5000",
                      buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("TX multicast should be rejected, got ret=%d", ret);

    /* No streams should be registered */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_stream_open_multicast_bad_addr(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    /* Non-multicast address should be rejected */
    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 multicast=192.168.1.1:5000",
                      buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("non-multicast address should be rejected, got ret=%d", ret);

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


void test_cmd_stream_open_multicast_duplicate(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    /* First open should succeed */
    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 multicast=239.1.2.3:5000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    /* Second open with same group:port should fail */
    ret = run_cmd(rig,
                  "\\stream_open AUDIO_RX PCM_S16 48000 multicast=239.1.2.3:5000",
                  buf, sizeof(buf));
    TEST_CHECK(ret != 0);
    TEST_MSG("duplicate group:port should be rejected, got ret=%d", ret);

    /* Only 1 stream should be registered */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 1);

    /* Clean up */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


void test_cmd_stream_open_multicast_ttl_override(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 multicast=239.1.2.3:5002 ttl=4",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_open returned %d", ret);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s->multicast == 1);
    TEST_CHECK(s->multicast_ttl == 4);
    TEST_MSG("ttl: got %d, expected 4", s->multicast_ttl);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Every \stream_open key=value parameter that lands in the stream config or on
 * the stream itself. A parameter that is silently dropped (a typo in its key
 * string, say) leaves the field at its default, so each assertion uses a value
 * no default would produce. */
void test_cmd_stream_open_kv_config_params(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 channels=2 mtu=2000 "
                      "metadata_interval=250 metadata_refresh=750 "
                      "time_stale_coarse=1500 time_stale_invalidate=7000 "
                      "keepalive_timeout=90",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_open returned %d", ret);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(s->config.channels == 2);
    TEST_MSG("channels: got %d, expected 2", s->config.channels);

    TEST_CHECK(s->config.mtu == 2000);
    TEST_MSG("mtu: got %u, expected 2000", s->config.mtu);

    TEST_CHECK(s->metadata_interval_ms == 250);
    TEST_MSG("metadata_interval: got %d, expected 250", s->metadata_interval_ms);

    TEST_CHECK(s->metadata_refresh_ms == 750);
    TEST_MSG("metadata_refresh: got %d, expected 750", s->metadata_refresh_ms);

    TEST_CHECK(s->config.time_stale_coarse_ms == 1500);
    TEST_MSG("time_stale_coarse: got %u, expected 1500",
             s->config.time_stale_coarse_ms);

    TEST_CHECK(s->config.time_stale_invalidate_ms == 7000);
    TEST_MSG("time_stale_invalidate: got %u, expected 7000",
             s->config.time_stale_invalidate_ms);

    TEST_CHECK(s->subscribe_timeout_s == 90);
    TEST_MSG("keepalive_timeout: got %d, expected 90", s->subscribe_timeout_s);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* SO_SNDBUF actually granted by the kernel for a request of `bytes` on a
 * scratch UDP socket, or -1 if the probe fails. Linux silently clamps
 * requests to net.core.wmem_max (and reports doubled values), so the
 * granted size is the only reliable signal of the OS ceiling. */
static int probe_sndbuf(size_t bytes)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    int val = (int)bytes;
    int granted = -1;

    if (fd < 0)
    {
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                   (const void *)&val, sizeof(val)) == 0)
    {
        socklen_t len = sizeof(granted);

        if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                       (void *)&granted, &len) != 0)
        {
            granted = -1;
        }
    }

    close(fd);
    return granted;
}


/* Socket buffer of the stream's UDP socket. RX streams are served by the
 * daemon, so the send buffer is the one sized from transport_buffer_*. */
static int stream_sndbuf(const struct rigctld_stream *s)
{
    int val = 0;
    socklen_t len = sizeof(val);

    if (getsockopt(s->udp_sock, SOL_SOCKET, SO_SNDBUF, (void *)&val, &len) != 0)
    {
        return -1;
    }

    return val;
}


/* transport_buffer_ms / _bytes do not reach the stream struct — they size the
 * UDP socket buffer. Compare against a default-sized stream rather than an
 * absolute value, because the kernel is free to round or double what it is
 * given. Both requests are far above the 250 ms default at this rate. */
void test_cmd_stream_open_kv_transport_buffer(void)
{
    /* On stock Linux net.core.wmem_max (212992) sits below even the
     * default request (the 256 KB floor), so every open is clamped to the
     * same ceiling and the ms/bytes comparisons below can never hold.
     * Probe before any TEST_CHECK (TEST_SKIP requires it) and skip on
     * such hosts; raising net.core.wmem_max re-enables the test. */
    {
        size_t def_req = stream_transport_buffer_bytes(
                             48000, 2, RIG_STREAM_TRANSPORT_BUFFER_DURATION_MS,
                             0);
        int def_granted = probe_sndbuf(def_req);
        int big_granted = probe_sndbuf(4000000);

        if (def_granted > 0 && big_granted > 0
                && big_granted <= def_granted)
        {
            TEST_SKIP("kernel grants SO_SNDBUF %d for a 4 MB request vs %d "
                      "for the default; raise net.core.wmem_max",
                      big_granted, def_granted);
            return;
        }
    }

    RIG *rig = stream_test_begin();
    char buf[1024];
    int id_default = -1, id_ms = -1, id_bytes = -1, udp_port = -1;

    TEST_CHECK(run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                       buf, sizeof(buf)) == 0);
    TEST_CHECK(parse_open_response(buf, &id_default, &udp_port) == 0);

    TEST_CHECK(run_cmd(rig,
                       "\\stream_open AUDIO_RX PCM_S16 48000 transport_buffer_ms=5000",
                       buf, sizeof(buf)) == 0);
    TEST_CHECK(parse_open_response(buf, &id_ms, &udp_port) == 0);

    TEST_CHECK(run_cmd(rig,
                       "\\stream_open AUDIO_RX PCM_S16 48000 transport_buffer_bytes=4000000",
                       buf, sizeof(buf)) == 0);
    TEST_CHECK(parse_open_response(buf, &id_bytes, &udp_port) == 0);

    struct rigctld_stream *sd = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id_default);
    struct rigctld_stream *sm = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id_ms);
    struct rigctld_stream *sb = rigctld_stream_registry_find_by_id(
                                    &g_stream_registry, id_bytes);
    TEST_ASSERT(sd != NULL && sm != NULL && sb != NULL);

    int bd = stream_sndbuf(sd), bm = stream_sndbuf(sm), bb = stream_sndbuf(sb);
    TEST_ASSERT(bd > 0 && bm > 0 && bb > 0);

    TEST_CHECK(bm > bd);
    TEST_MSG("transport_buffer_ms=5000 gave %d, default gave %d", bm, bd);

    TEST_CHECK(bb > bd);
    TEST_MSG("transport_buffer_bytes=4000000 gave %d, default gave %d", bb, bd);

    char close_cmd[64];
    int ids[3] = { id_default, id_ms, id_bytes };

    for (int i = 0; i < 3; i++)
    {
        snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", ids[i]);
        run_cmd(rig, close_cmd, buf, sizeof(buf));
    }

    stream_test_end(rig);
}


void test_cmd_stream_status_multicast(void)
{
    RIG *rig = stream_test_begin();
    char buf[1024];

    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 multicast=239.1.2.3:5003 ttl=3",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    parse_open_response(buf, &stream_id, &udp_port);

    /* Get stream status */
    char status_cmd[64];
    snprintf(status_cmd, sizeof(status_cmd), "\\stream_status %d", stream_id);
    ret = run_cmd(rig, status_cmd, buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    /* Verify multicast fields in status */
    char mcast_addr[64] = "";
    TEST_CHECK(parse_ext_str(buf, "multicast", mcast_addr,
                             sizeof(mcast_addr)) == 0);
    TEST_CHECK(strcmp(mcast_addr, "239.1.2.3") == 0);
    TEST_MSG("status multicast: got '%s'", mcast_addr);

    int ttl = -1;
    TEST_CHECK(parse_ext_int(buf, "ttl", &ttl) == 0);
    TEST_CHECK(ttl == 3);
    TEST_MSG("status ttl: got %d, expected 3", ttl);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Verify RX feeder replies PONG to a PING packet. */
void test_rx_ping_pong_reply(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe and consume ACK */

    unsigned char pkt[2048];
    ssize_t n = subscribe_and_await_ack(client_sock, udp_port,
                                        RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                        pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);  /* ACK */

    /* Drain a couple of data/metadata packets */
    int i;

    for (i = 0; i < 3; i++)
    {
        udp_recv_timeout(client_sock, pkt, sizeof(pkt), 500);
    }

    /* Send PING */
    TEST_CHECK(send_ping_pkt(client_sock, udp_port,
                             RIG_STREAM_TYPE_AUDIO_RX, stream_id) == 0);

    /* Receive packets until we get a PONG (skip data/metadata) */
    int got_pong = 0;

    for (i = 0; i < 20; i++)
    {
        n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 1000);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            continue;
        }

        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
        {
            continue;
        }

        if (hdr.control & RIG_STREAM_CTRL_PONG)
        {
            got_pong = 1;
            TEST_CHECK(hdr.stream_id == stream_id);
            TEST_MSG("pong stream_id: got %d, expected %d",
                     hdr.stream_id, stream_id);
            TEST_CHECK(hdr.payload_len == 0);
            TEST_MSG("pong payload_len: got %d, expected 0",
                     hdr.payload_len);
            break;
        }
    }

    TEST_CHECK(got_pong);
    TEST_MSG("no PONG received after sending PING");

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Verify stream auto-closes if no subscribe arrives within timeout. */
void test_rx_subscribe_timeout_auto_close(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    /* Shorten timeout for testing */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (!s)
    {
        stream_test_end(rig);
        return;
    }

    s->subscribe_timeout_s = 2;

    /* Don't subscribe — wait for timeout + margin */
    sleep(3);

    /* Stream should have been auto-closed (stays in registry as zombie) */
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s != NULL && s->auto_closed == 1);
    TEST_MSG("stream should have auto_closed=1 after subscribe timeout");

    stream_test_end(rig);
}


/* Verify RX stream auto-closes when no PING arrives after subscribe. */
void test_rx_keepalive_timeout_auto_close(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe */
    unsigned char pkt[2048];
    TEST_CHECK(subscribe_and_await_ack(client_sock, udp_port,
                                       RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                       pkt, sizeof(pkt)) >= 0);

    /* Shorten keepalive timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 2;
    }

    /* Don't send any PINGs — wait for timeout */
    sleep(4);

    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s != NULL && s->auto_closed == 1);
    TEST_MSG("stream should have auto_closed=1 after keepalive timeout");

    close(client_sock);
    stream_test_end(rig);
}


/* Verify PING resets the keepalive timeout, keeping the stream alive. */
void test_rx_ping_resets_timeout(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe */
    unsigned char pkt[2048];
    TEST_CHECK(subscribe_and_await_ack(client_sock, udp_port,
                                       RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                       pkt, sizeof(pkt)) >= 0);

    /* Shorten keepalive timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 3;
    }

    /* Send PINGs at 1s and 2s to keep alive past the initial 3s window */
    sleep(1);
    send_ping_pkt(client_sock, udp_port, RIG_STREAM_TYPE_AUDIO_RX, stream_id);

    sleep(1);
    send_ping_pkt(client_sock, udp_port, RIG_STREAM_TYPE_AUDIO_RX, stream_id);

    /* Wait 1 more second (total 3s, but only 1s since last PING) */
    sleep(1);

    /* Stream should still be alive — last PING was 1s ago, timeout is 3s */
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_MSG("stream should still be alive — PINGs reset timeout");

    close(client_sock);

    if (s)
    {
        char close_cmd[64];
        snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
        run_cmd(rig, close_cmd, buf, sizeof(buf));
    }

    stream_test_end(rig);
}


/* Verify re-subscribe resets keepalive timeout. */
void test_rx_resubscribe_resets_timeout(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe */
    unsigned char pkt[2048];
    TEST_CHECK(subscribe_and_await_ack(client_sock, udp_port,
                                       RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                       pkt, sizeof(pkt)) >= 0);

    /* Shorten keepalive timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 3;
    }

    /* Re-subscribe at 2s to reset timeout */
    sleep(2);
    send_subscribe_pkt(client_sock, udp_port, RIG_STREAM_TYPE_AUDIO_RX, stream_id);

    /* Wait another 2s — total 4s from start but only 2s from re-subscribe */
    sleep(2);

    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_MSG("stream should still be alive — re-subscribe reset timeout");

    close(client_sock);

    if (s)
    {
        char close_cmd[64];
        snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
        run_cmd(rig, close_cmd, buf, sizeof(buf));
    }

    stream_test_end(rig);
}


/* Verify multicast RX streams are exempt from keepalive timeout. */
void test_rx_multicast_no_timeout(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 multicast=239.1.2.3:5010",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    /* Shorten keepalive timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 2;
    }

    /* Wait past timeout — multicast should stay alive */
    sleep(3);

    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_MSG("multicast stream should be exempt from keepalive timeout");

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Verify TX feeder replies PONG to a PING packet. */
void test_tx_ping_pong_reply(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Send PING to TX feeder */
    TEST_CHECK(send_ping_pkt(client_sock, udp_port,
                             RIG_STREAM_TYPE_AUDIO_TX, stream_id) == 0);

    /* Receive PONG */
    unsigned char pkt[64];
    ssize_t n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);
    TEST_MSG("no PONG received from TX feeder (got %zd bytes)", n);

    if (n >= RIG_STREAM_HEADER_SIZE)
    {
        struct rig_stream_packet_header hdr;
        TEST_CHECK(stream_packet_header_unpack(pkt, n, &hdr) == 0);
        TEST_CHECK((hdr.control & RIG_STREAM_CTRL_PONG) != 0);
        TEST_MSG("expected PONG control bit, got 0x%04x", hdr.control);
        TEST_CHECK(hdr.stream_id == stream_id);
        TEST_CHECK(hdr.payload_len == 0);
    }

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Verify TX stream auto-closes when no data arrives within timeout. */
void test_tx_timeout_auto_close(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    /* Shorten timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 2;
    }

    /* Don't send any data — wait for timeout */
    sleep(4);

    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s != NULL && s->auto_closed == 1);
    TEST_MSG("TX stream should have auto_closed=1 after inactivity timeout");

    stream_test_end(rig);
}


/* Verify sending data resets the TX inactivity timeout. */
void test_tx_data_resets_timeout(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Shorten timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 3;
    }

    /* Send data packets at 1s intervals to keep alive */
    int i;

    for (i = 0; i < 3; i++)
    {
        sleep(1);
        /* Send a data packet */
        unsigned char pkt[RIG_STREAM_HEADER_SIZE + 64];
        struct rig_stream_packet_header hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.version = 1;
        hdr.type = RIG_STREAM_TYPE_AUDIO_TX;
        hdr.stream_id = stream_id;
        hdr.subscribe_token = get_stream_token(stream_id);
        hdr.seq = i;
        hdr.payload_len = 64;
        stream_packet_header_pack(&hdr, pkt);
        memset(pkt + RIG_STREAM_HEADER_SIZE, 0, 64);

        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(udp_port);
        dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        sendto(client_sock, pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dest, sizeof(dest));
    }

    /* Total elapsed ~3s, but only 1s since last data → should be alive */
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_MSG("TX stream should still be alive — data resets timeout");

    close(client_sock);

    if (s)
    {
        char close_cmd[64];
        snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
        run_cmd(rig, close_cmd, buf, sizeof(buf));
    }

    stream_test_end(rig);
}


/* Verify stream_close after auto_close returns error, no crash. */
void test_auto_close_then_stream_close(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    /* Shorten subscribe timeout to trigger auto_close */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 2;
    }

    /* Don't subscribe — wait for auto_close */
    sleep(3);

    /* Confirm stream was auto-closed (stays in registry as zombie) */
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s != NULL && s->auto_closed == 1);

    /* stream_close on a zombie should succeed (join + cleanup) */
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    ret = run_cmd(rig, close_cmd, buf, sizeof(buf));

    /* Verify RPRT 0 (success) */
    char *rprt = strstr(buf, "RPRT ");
    TEST_CHECK(rprt != NULL);

    if (rprt)
    {
        int code = atoi(rprt + 5);
        TEST_CHECK(code == 0);
        TEST_MSG("expected RPRT 0 for zombie close, got %d", code);
    }

    /* Now confirm it's gone from registry */
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s == NULL);

    stream_test_end(rig);
}


/* Regression: kv args parsed correctly via stream_open command. */
void test_kv_parse_via_stream_open(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Registry default is 0; override via kv arg */
    g_stream_registry.metadata_interval_ms = 0;

    int ret = run_cmd(rig,
                      "\\stream_open AUDIO_RX PCM_S16 48000 metadata_interval=100",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("stream_open returned %d", ret);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (!s)
    {
        stream_test_end(rig);
        return;
    }

    /* Verify kv arg overrode registry default */
    TEST_CHECK(s->metadata_interval_ms == 100);
    TEST_MSG("metadata_interval_ms=%d, expected 100", s->metadata_interval_ms);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* Gap 1: Verify PING resets the TX inactivity timeout, keeping stream alive. */
void test_tx_ping_resets_timeout(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_TX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Shorten timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 3;
    }

    /* Send PINGs (no data) at 1s intervals to keep alive */
    int i;

    for (i = 0; i < 3; i++)
    {
        sleep(1);
        send_ping_pkt(client_sock, udp_port, RIG_STREAM_TYPE_AUDIO_TX, stream_id);
    }

    /* Total ~3s elapsed, but only 1s since last PING → should be alive */
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_MSG("TX stream should still be alive — PINGs reset timeout");

    close(client_sock);

    if (s)
    {
        char close_cmd[64];
        snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
        run_cmd(rig, close_cmd, buf, sizeof(buf));
    }

    stream_test_end(rig);
}


/* Gap 2: Verify concurrent auto_close and stream_close don't crash.
 * Sets timeout right at the boundary so both paths race. */
void test_concurrent_auto_close_stream_close(void)
{
    int iter;

    for (iter = 0; iter < 3; iter++)
    {
        RIG *rig = test_rig_setup();
        TEST_CHECK(rig != NULL);
        rigctld_stream_registry_init(&g_stream_registry);
        char buf[1024];

        int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                          buf, sizeof(buf));
        TEST_CHECK(ret == 0);

        int stream_id = -1, udp_port = -1;
        TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

        int client_sock = create_client_udp_socket();
        TEST_CHECK(client_sock >= 0);

        /* Subscribe so feeder enters data loop */
        unsigned char pkt[2048];
        TEST_CHECK(subscribe_and_await_ack(client_sock, udp_port,
                                           RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                           pkt, sizeof(pkt)) >= 0);

        /* Set tight timeout — feeder will auto_close around t=2 */
        struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                       &g_stream_registry, stream_id);

        if (s)
        {
            s->subscribe_timeout_s = 2;
        }

        /* Sleep right up to the timeout boundary, then race stream_close */
        sleep(2);

        char close_cmd[64];
        snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
        run_cmd(rig, close_cmd, buf, sizeof(buf));

        /* Either stream_close won (success) or auto_close won (error).
         * Either way: no crash, stream must be gone. */
        usleep(200000);  /* let any in-flight thread finish */

        s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
        TEST_CHECK(s == NULL);
        TEST_MSG("iter %d: stream should be gone after close/auto_close race",
                 iter);

        close(client_sock);
        stream_test_end(rig);
    }
}


/* Verify registry_destroy properly stops running feeder threads.
 * Before the fix, registry_destroy just freed streams without stopping
 * feeders, causing use-after-free. */
void test_registry_destroy_stops_feeders(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Open a stream so a feeder is actively running */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int id1 = -1, port1 = -1;
    TEST_CHECK(parse_open_response(buf, &id1, &port1) == 0);

    /* Verify it's in the registry with feeder running */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 1);

    /* Destroy registry — must stop feeders cleanly, no crash */
    rigctld_stream_registry_destroy(&g_stream_registry);

    test_rig_teardown(rig);
}


/* Verify AUDIO_RX and IQ_RX can be open simultaneously. */
void test_dual_stream_audio_and_iq(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    /* Open audio stream */
    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    int audio_id = -1, audio_port = -1;
    TEST_CHECK(parse_open_response(buf, &audio_id, &audio_port) == 0);
    TEST_MSG("audio stream_id=%d udp_port=%d", audio_id, audio_port);

    /* Open IQ stream (must use IQ format, not PCM) */
    ret = run_cmd(rig, "\\stream_open IQ_RX IQ_CS16 48000",
                  buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_MSG("IQ stream_open response: %s", buf);
    int iq_id = -1, iq_port = -1;
    TEST_CHECK(parse_open_response(buf, &iq_id, &iq_port) == 0);
    TEST_MSG("iq stream_id=%d udp_port=%d", iq_id, iq_port);

    /* Both should be in registry */
    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 2);

    /* Different stream IDs */
    TEST_CHECK(audio_id != iq_id);
    TEST_MSG("audio_id=%d iq_id=%d should differ", audio_id, iq_id);

    /* Different UDP ports */
    TEST_CHECK(audio_port != iq_port);

    /* Both should be findable */
    struct rigctld_stream *s_audio = rigctld_stream_registry_find_by_id(
                                         &g_stream_registry, audio_id);
    struct rigctld_stream *s_iq = rigctld_stream_registry_find_by_id(
                                      &g_stream_registry, iq_id);
    TEST_CHECK(s_audio != NULL);
    TEST_CHECK(s_iq != NULL);

    if (s_audio && s_iq)
    {
        TEST_CHECK(s_audio->type == RIG_STREAM_TYPE_AUDIO_RX);
        TEST_CHECK(s_iq->type == RIG_STREAM_TYPE_IQ_RX);
    }

    /* Close both */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", audio_id);
    run_cmd(rig, cmd, buf, sizeof(buf));
    snprintf(cmd, sizeof(cmd), "\\stream_close %d", iq_id);
    run_cmd(rig, cmd, buf, sizeof(buf));

    TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);

    stream_test_end(rig);
}


/* Race close_by_client against auto_close timeout.
 * Verifies no crash when close_by_client and the feeder's auto_close
 * both try to clean up the same stream concurrently. */
void test_close_by_client_concurrent_with_auto_close(void)
{
    int iter;

    for (iter = 0; iter < 3; iter++)
    {
        RIG *rig = test_rig_setup();
        TEST_CHECK(rig != NULL);
        rigctld_stream_registry_init(&g_stream_registry);
        char buf[1024];

        int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                          buf, sizeof(buf));
        TEST_CHECK(ret == 0);

        int stream_id = -1, udp_port = -1;
        TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

        int client_sock = create_client_udp_socket();
        TEST_CHECK(client_sock >= 0);

        /* Subscribe so feeder enters data loop */
        unsigned char pkt[2048];
        TEST_CHECK(subscribe_and_await_ack(client_sock, udp_port,
                                           RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                           pkt, sizeof(pkt)) >= 0);

        /* Set tight timeout — feeder will auto_close around t=2 */
        struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                       &g_stream_registry, stream_id);

        if (s)
        {
            s->subscribe_timeout_s = 2;
        }

        /* Sleep right up to the boundary, then race close_by_client */
        sleep(2);

        int closed = rigctld_stream_registry_close_by_client(
                         &g_stream_registry, s ? s->client_id : 0);

        /* Either close_by_client found the stream or auto_close got there first.
         * Either way: no crash, and the registry must be empty. */
        usleep(200000);  /* let any in-flight thread finish */

        TEST_CHECK(rigctld_stream_registry_count(&g_stream_registry) == 0);
        TEST_MSG("iter %d: close_by_client returned %d, registry should be empty",
                 iter, closed);

        close(client_sock);
        stream_test_end(rig);
    }
}


/* Gap 3: Verify PING with wrong stream_id is ignored (no timeout reset). */
void test_rx_ping_wrong_stream_id_ignored(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Subscribe */
    unsigned char pkt[2048];
    TEST_CHECK(subscribe_and_await_ack(client_sock, udp_port,
                                       RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                       pkt, sizeof(pkt)) >= 0);

    /* Shorten keepalive timeout */
    struct rigctld_stream *s = rigctld_stream_registry_find_by_id(
                                   &g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);

    if (s)
    {
        s->subscribe_timeout_s = 3;
    }

    /* Send PINGs with WRONG stream_id — should not reset timeout */
    int wrong_id = stream_id + 99;
    sleep(1);
    send_ping_pkt(client_sock, udp_port, RIG_STREAM_TYPE_AUDIO_RX, wrong_id);
    sleep(1);
    send_ping_pkt(client_sock, udp_port, RIG_STREAM_TYPE_AUDIO_RX, wrong_id);

    /* Wait for timeout to fire (3s from subscribe, we're at ~2s + margin) */
    sleep(2);

    /* Stream should have auto-closed — wrong-id PINGs didn't save it */
    s = rigctld_stream_registry_find_by_id(&g_stream_registry, stream_id);
    TEST_CHECK(s != NULL);
    TEST_CHECK(s != NULL && s->auto_closed == 1);
    TEST_MSG("wrong stream_id PINGs should not reset keepalive timeout");

    close(client_sock);
    stream_test_end(rig);
}


/* Gap 6: Verify PING before subscribe gets PONG reply. */
void test_rx_ping_before_subscribe(void)
{
    RIG *rig = stream_test_begin();
    TEST_CHECK(rig != NULL);
    char buf[1024];

    int ret = run_cmd(rig, "\\stream_open AUDIO_RX PCM_S16 48000",
                      buf, sizeof(buf));
    TEST_CHECK(ret == 0);

    int stream_id = -1, udp_port = -1;
    TEST_CHECK(parse_open_response(buf, &stream_id, &udp_port) == 0);

    int client_sock = create_client_udp_socket();
    TEST_CHECK(client_sock >= 0);

    /* Send PING before subscribing */
    TEST_CHECK(send_ping_pkt(client_sock, udp_port,
                             RIG_STREAM_TYPE_AUDIO_RX, stream_id) == 0);

    /* Should get PONG back */
    unsigned char pkt[256];
    ssize_t n = udp_recv_timeout(client_sock, pkt, sizeof(pkt), 2000);
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);
    TEST_MSG("no PONG received before subscribe (got %zd bytes)", n);

    if (n >= RIG_STREAM_HEADER_SIZE)
    {
        struct rig_stream_packet_header hdr;
        TEST_CHECK(stream_packet_header_unpack(pkt, n, &hdr) == 0);
        TEST_CHECK((hdr.control & RIG_STREAM_CTRL_PONG) != 0);
        TEST_MSG("expected PONG, got control 0x%04x", hdr.control);
        TEST_CHECK(hdr.stream_id == stream_id);
    }

    /* Now subscribe normally — should still work */

    n = subscribe_and_await_ack(client_sock, udp_port,
                                RIG_STREAM_TYPE_AUDIO_RX, stream_id,
                                pkt, sizeof(pkt));
    TEST_CHECK(n >= RIG_STREAM_HEADER_SIZE);

    if (n >= RIG_STREAM_HEADER_SIZE)
    {
        struct rig_stream_packet_header hdr;
        stream_packet_header_unpack(pkt, n, &hdr);
        TEST_CHECK((hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK) != 0);
        TEST_MSG("expected SUBSCRIBE_ACK after PING, got 0x%04x", hdr.control);
    }

    close(client_sock);

    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "\\stream_close %d", stream_id);
    run_cmd(rig, close_cmd, buf, sizeof(buf));

    stream_test_end(rig);
}


/* --- Test list --- */

TEST_LIST =
{
    /* stream_caps — exact output verification */
    { "cmd_stream_caps_line_count",         test_cmd_stream_caps_line_count },
    { "cmd_stream_caps_audio_rx_complete",  test_cmd_stream_caps_audio_rx_complete },
    { "cmd_stream_caps_audio_tx_complete",  test_cmd_stream_caps_audio_tx_complete },
    { "cmd_stream_caps_iq_rx_complete",     test_cmd_stream_caps_iq_rx_complete },
    { "cmd_stream_caps_iq_tx_complete",     test_cmd_stream_caps_iq_tx_complete },
    { "cmd_stream_open_iq_multichannel",    test_cmd_stream_open_iq_multichannel },
    { "cmd_stream_open_reports_conversions", test_cmd_stream_open_reports_conversions },
    { "cmd_stream_open_require_native",     test_cmd_stream_open_require_native },

    /* stream_open — happy paths with backend verification */
    { "cmd_stream_open_kv_config_params",   test_cmd_stream_open_kv_config_params },
    { "cmd_stream_open_kv_transport_buffer", test_cmd_stream_open_kv_transport_buffer },
    { "cmd_stream_open_audio",              test_cmd_stream_open_audio },
    { "cmd_stream_open_iq",                 test_cmd_stream_open_iq },
    { "cmd_stream_open_sequential_ids",     test_cmd_stream_open_sequential_ids },
    { "cmd_stream_open_ext_resp",           test_cmd_stream_open_ext_resp },
    { "cmd_stream_open_multiple_types",     test_cmd_stream_open_multiple_types },
    { "stream_source_id_stamping",          test_stream_source_id_stamping },
    { "stream_source_id_inbound_reject",    test_stream_source_id_inbound_reject },
    { "stream_wrong_token_reject",          test_stream_wrong_token_reject },

    /* stream_open — error paths with RPRT code verification */
    { "cmd_stream_open_bad_type",           test_cmd_stream_open_bad_type },
    { "cmd_stream_open_bad_format",         test_cmd_stream_open_bad_format },
    { "cmd_stream_open_bad_rate",           test_cmd_stream_open_bad_rate },
    { "cmd_stream_open_unsupported_rate",   test_cmd_stream_open_unsupported_rate },
    { "cmd_stream_open_iq_format_on_audio", test_cmd_stream_open_iq_format_on_audio },

    /* stream_close — with cleanup verification */
    { "cmd_stream_close_happy_path",        test_cmd_stream_close_happy_path },
    { "cmd_stream_close_not_found",         test_cmd_stream_close_not_found },
    { "cmd_stream_close_double_close",      test_cmd_stream_close_double_close },
    { "cmd_stream_close_frees_slot",        test_cmd_stream_close_frees_slot },
    { "cmd_stream_close_non_numeric",       test_cmd_stream_close_non_numeric },
    { "cmd_stream_close_negative_id",       test_cmd_stream_close_negative_id },

    /* open+close interaction */
    { "cmd_open_close_reopen_cycle",        test_cmd_open_close_reopen_cycle },
    { "cmd_multiple_streams_close_middle",  test_cmd_multiple_streams_close_middle },

    /* UDP data path verification */
    { "cmd_stream_open_udp_socket_usable",  test_cmd_stream_open_udp_socket_usable },

    /* RX feeder thread */
    { "rx_subscribe_ack",                   test_rx_subscribe_ack },
    { "rx_data_arrives",                    test_rx_data_arrives },
    { "rx_sequence_continuous",             test_rx_sequence_continuous },

    /* IQ and payload verification */
    { "rx_iq_data_arrives",                 test_rx_iq_data_arrives },
    { "rx_counter_payload_audio",           test_rx_counter_payload_audio },
    { "rx_codec_passthrough_audio",         test_rx_codec_passthrough_audio },
    { "cmd_stream_open_codec_non_native",   test_cmd_stream_open_codec_non_native },
    { "rx_counter_payload_iq",              test_rx_counter_payload_iq },
    { "rx_metadata_initial",               test_rx_metadata_initial },

    /* TX feeder thread */
    { "tx_audio_data_accepted",             test_tx_audio_data_accepted },
    { "tx_stereo_timestamp_frames",         test_tx_stereo_timestamp_frames },
    { "tx_iq_data_accepted",                test_tx_iq_data_accepted },
    { "tx_loopback_counter_audio",          test_tx_loopback_counter_audio },
    { "tx_loopback_counter_iq",             test_tx_loopback_counter_iq },
    { "tx_sequence_gap",                    test_tx_sequence_gap },
    { "tx_sequence_gap_implausible",        test_tx_sequence_gap_implausible },
    { "tx_metadata_applied",                test_tx_metadata_applied },
    { "tx_metadata_truncated_rejected",     test_tx_metadata_truncated_rejected },

    /* Stream control commands */
    { "cmd_stream_pause_resume",            test_cmd_stream_pause_resume },
    { "cmd_stream_mute_unmute",             test_cmd_stream_mute_unmute },
    { "cmd_stream_pause_not_found",         test_cmd_stream_pause_not_found },
    { "cmd_stream_status",                  test_cmd_stream_status },
    { "cmd_stream_status_ext_resp",         test_cmd_stream_status_ext_resp },
    { "cmd_stream_status_not_found",        test_cmd_stream_status_not_found },
    { "cmd_stream_metadata_read",            test_cmd_stream_metadata_read },
    { "cmd_stream_drain",                   test_cmd_stream_drain },
    { "cmd_stream_drain_not_found",         test_cmd_stream_drain_not_found },
    { "cmd_stream_list",                    test_cmd_stream_list },
    { "cmd_stream_list_empty",              test_cmd_stream_list_empty },
    { "cmd_stream_list_after_close",        test_cmd_stream_list_after_close },

    /* Data-flow verification */
    { "rx_mute_zeros",                      test_rx_mute_zeros },

    /* Comprehensive field verification */
    { "cmd_stream_status_all_fields",       test_cmd_stream_status_all_fields },
    { "cmd_stream_status_after_pause_mute", test_cmd_stream_status_after_pause_mute },
    { "cmd_stream_list_all_fields",         test_cmd_stream_list_all_fields },

    /* Error-path tests */
    { "cmd_stream_mute_not_found",          test_cmd_stream_mute_not_found },
    { "cmd_stream_unmute_not_found",        test_cmd_stream_unmute_not_found },
    { "cmd_stream_resume_not_found",        test_cmd_stream_resume_not_found },
    { "cmd_stream_metadata_read_not_found",  test_cmd_stream_metadata_read_not_found },

    /* ext_resp enforcement */
    { "cmd_stream_requires_ext_resp",       test_cmd_stream_requires_ext_resp },

    /* Idempotent toggle tests */
    { "cmd_stream_pause_already_paused",    test_cmd_stream_pause_already_paused },
    { "cmd_stream_resume_not_paused",       test_cmd_stream_resume_not_paused },
    { "cmd_stream_mute_already_muted",      test_cmd_stream_mute_already_muted },
    { "cmd_stream_unmute_not_muted",        test_cmd_stream_unmute_not_muted },

    /* Client disconnect cleanup */
    { "cmd_disconnect_cleans_streams",      test_cmd_disconnect_cleans_streams },
    { "cmd_disconnect_leaves_other_clients", test_cmd_disconnect_leaves_other_clients },

    /* Metadata interval propagation */
    { "cmd_stream_metadata_interval_from_registry", test_cmd_stream_metadata_interval_from_registry },

    /* pthread_key client_id propagation */
    { "client_id_from_pthread_key",         test_client_id_from_pthread_key },
    { "disconnect_cleanup_via_pthread_key", test_disconnect_cleanup_via_pthread_key },

    /* Edge cases and hardening */
    { "cmd_stream_status_non_numeric_id",   test_cmd_stream_status_non_numeric_id },
    { "cmd_stream_pause_non_numeric_id",    test_cmd_stream_pause_non_numeric_id },
    { "cmd_feeder_exits_without_subscribe", test_cmd_feeder_exits_without_subscribe },
    { "cmd_close_by_client_idempotent",     test_cmd_close_by_client_idempotent },

    /* PING/PONG keepalive */
    { "rx_ping_pong_reply",                  test_rx_ping_pong_reply },
    { "tx_ping_pong_reply",                  test_tx_ping_pong_reply },
    { "rx_subscribe_timeout_auto_close",     test_rx_subscribe_timeout_auto_close },
    { "rx_keepalive_timeout_auto_close",     test_rx_keepalive_timeout_auto_close },
    { "rx_ping_resets_timeout",              test_rx_ping_resets_timeout },
    { "rx_resubscribe_resets_timeout",       test_rx_resubscribe_resets_timeout },
    { "rx_multicast_no_timeout",             test_rx_multicast_no_timeout },
    { "tx_timeout_auto_close",               test_tx_timeout_auto_close },
    { "tx_data_resets_timeout",              test_tx_data_resets_timeout },
    { "auto_close_then_stream_close",        test_auto_close_then_stream_close },
    { "concurrent_auto_close_stream_close", test_concurrent_auto_close_stream_close },
    { "registry_destroy_stops_feeders",     test_registry_destroy_stops_feeders },
    { "dual_stream_audio_and_iq",           test_dual_stream_audio_and_iq },
    { "close_by_client_concurrent_auto_close", test_close_by_client_concurrent_with_auto_close },
    { "rx_ping_wrong_stream_id_ignored",    test_rx_ping_wrong_stream_id_ignored },
    { "tx_ping_resets_timeout",             test_tx_ping_resets_timeout },
    { "rx_ping_before_subscribe",           test_rx_ping_before_subscribe },

    /* Key=value argument parsing regression */
    { "kv_parse_via_stream_open",            test_kv_parse_via_stream_open },

    /* Multicast streaming */
    { "cmd_stream_open_multicast_ipv4",     test_cmd_stream_open_multicast_ipv4 },
    { "cmd_stream_open_multicast_tx_rejected", test_cmd_stream_open_multicast_tx_rejected },
    { "cmd_stream_open_multicast_bad_addr", test_cmd_stream_open_multicast_bad_addr },
    { "cmd_stream_open_multicast_duplicate", test_cmd_stream_open_multicast_duplicate },
    { "cmd_stream_open_multicast_ttl_override", test_cmd_stream_open_multicast_ttl_override },
    { "cmd_stream_status_multicast",        test_cmd_stream_status_multicast },

    { NULL, NULL }
};
