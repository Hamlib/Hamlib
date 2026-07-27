/*
 *  Hamlib netrigctl streaming tests
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

/* Integration tests for netrigctl streaming over network to rigctld. */
/* Spawns a subprocess rigctld with dummy backend for end-to-end testing. */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"

#ifdef _WIN32

/* These tests drive a real rigctld child through fork()/execlp(), which
 * Windows does not provide, so the suite does not run there. The netrigctl
 * client code it exercises is also covered by test_rigctld_stream, which is
 * portable. */
static void test_requires_posix_host(void)
{
    TEST_MSG("netrigctl streaming tests need fork(); skipped on this host");
    TEST_CHECK(1);
}

TEST_LIST =
{
    { "requires_posix_host", test_requires_posix_host },
    { NULL, NULL }
};

#else

#include <hamlib/rig.h>
/* Socket headers come from stream_proto.h, which picks the right set for the
 * host; do not include them directly. */
#include "../src/stream.h"
#include "../src/stream_net.h"
#include "../src/stream_proto.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>


/* --- Subprocess rigctld management --- */

struct rigctld_proc
{
    pid_t pid;
    int port;
};


/* Find an available TCP port by binding to port 0. */
static int find_free_port(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    if (sock < 0)
    {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }

    if (getsockname(sock, (struct sockaddr *)&addr, &addrlen) < 0)
    {
        close(sock);
        return -1;
    }

    int port = ntohs(addr.sin_port);
    close(sock);
    return port;
}


/* Wait until rigctld is accepting TCP connections. */
static int wait_for_rigctld(int port, int timeout_ms)
{
    int elapsed = 0;

    while (elapsed < timeout_ms)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;

        if (sock < 0)
        {
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            close(sock);
            return 0;
        }

        close(sock);
        usleep(50000);  /* 50ms */
        elapsed += 50;
    }

    return -1;
}


/* Find the rigctld binary — check build tree first, then PATH. */
static const char *find_rigctld(void)
{
    if (access("../tests/rigctld", X_OK) == 0)
    {
        return "../tests/rigctld";
    }

    if (access("./tests/rigctld", X_OK) == 0)
    {
        return "./tests/rigctld";
    }

    /* Try absolute path from build directory */
    if (access("tests/rigctld", X_OK) == 0)
    {
        return "tests/rigctld";
    }

    return "rigctld";  /* Fallback to PATH */
}


/* Start rigctld; source_id_opt, when non-NULL, is passed as
 * --stream-source-id. A proc->port already > 0 is reused (daemon restart on
 * the same port), otherwise a free port is picked. */
static int start_rigctld_opt(struct rigctld_proc *proc,
                             const char *source_id_opt)
{
    const char *rigctld_path;
    char port_str[16];

    if (proc->port <= 0)
    {
        proc->port = find_free_port();
    }

    if (proc->port < 0)
    {
        return -1;
    }

    rigctld_path = find_rigctld();
    snprintf(port_str, sizeof(port_str), "%d", proc->port);

    proc->pid = fork();

    if (proc->pid < 0)
    {
        return -1;
    }

    if (proc->pid == 0)
    {
        /* Child: redirect stdout/stderr to /dev/null */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        if (source_id_opt)
        {
            execlp(rigctld_path, "rigctld", "-m", "1", "-t", port_str,
                   "--stream-source-id", source_id_opt, NULL);
        }
        else
        {
            execlp(rigctld_path, "rigctld", "-m", "1", "-t", port_str, NULL);
        }

        _exit(127);
    }

    /* Parent: wait for rigctld to start */
    if (wait_for_rigctld(proc->port, 5000) < 0)
    {
        kill(proc->pid, SIGTERM);
        waitpid(proc->pid, NULL, 0);
        return -1;
    }

    return 0;
}


static int start_rigctld(struct rigctld_proc *proc)
{
    return start_rigctld_opt(proc, NULL);
}


static void stop_rigctld(struct rigctld_proc *proc)
{
    if (proc->pid > 0)
    {
        kill(proc->pid, SIGTERM);
        waitpid(proc->pid, NULL, 0);
        proc->pid = 0;
    }
}


/* Open a netrigctl RIG connected to the subprocess rigctld. */
static RIG *open_netrigctl(int port)
{
    RIG *rig;
    char pathname[64];

    rig = rig_init(RIG_MODEL_NETRIGCTL);

    if (!rig)
    {
        return NULL;
    }

    snprintf(pathname, sizeof(pathname), "127.0.0.1:%d", port);
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), pathname);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        return NULL;
    }

    return rig;
}


/* Helper: find a stream_caps entry by type, return pointer or NULL. */
static const struct rig_stream_caps *find_caps_by_type(
    const struct rig_caps *caps,
    rig_stream_type_t type)
{
    if (!caps || !caps->stream_caps)
    {
        return NULL;
    }

    for (int i = 0; i < HAMLIB_MAX_STREAM_CAPS; i++)
    {
        if (caps->stream_caps[i].type == type
                && caps->stream_caps[i].formats != 0)
        {
            return &caps->stream_caps[i];
        }

        if (caps->stream_caps[i].type == 0
                && caps->stream_caps[i].formats == 0)
        {
            break;
        }
    }

    return NULL;
}


/* Helper: check if a specific rate appears in the sample_rates array. */
static int has_rate(const int *rates, int rate)
{
    for (int i = 0; i < HAMLIB_MAX_STREAM_RATES && rates[i] != 0; i++)
    {
        if (rates[i] == rate)
        {
            return 1;
        }
    }

    return 0;
}


/* --- Tests --- */


/* Verify all 4 stream types are discovered with correct format bitmasks,
 * sample rates, channel ranges, and max_streams values matching the
 * dummy backend's rig_caps. */
void test_caps_discovery_all_types(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    /* Count total discovered caps */
    int total_caps = 0;

    for (int i = 0; rig->caps->stream_caps && i < HAMLIB_MAX_STREAM_CAPS; i++)
    {
        if (rig->caps->stream_caps[i].type == 0
                && rig->caps->stream_caps[i].formats == 0)
        {
            break;
        }

        total_caps++;
    }

    TEST_CHECK(total_caps == 4);
    TEST_MSG("Expected 4 stream caps, got %d", total_caps);

    /* --- AUDIO_RX --- */
    const struct rig_stream_caps *arx = find_caps_by_type(rig->caps,
                                        RIG_STREAM_TYPE_AUDIO_RX);
    TEST_CHECK(arx != NULL);
    TEST_MSG("AUDIO_RX caps must be present");

    if (arx)
    {
        /* Documented dummy AUDIO format bitmask:
         * S8(1<<0) | U8(1<<1) | S16(1<<2) | F32(1<<3)
         * = 1+2+4+8 = 0x0F */
        const rig_stream_format_t expected_audio_fmts = 0x0F;

        TEST_CHECK(arx->formats == expected_audio_fmts);
        TEST_MSG("AUDIO_RX formats: got 0x%x, expected 0x%x",
                 arx->formats, expected_audio_fmts);

        /* Exact sample rates: 8000, 16000, 24000, 48000, 96000 */
        TEST_CHECK(has_rate(arx->sample_rates, 8000));
        TEST_MSG("AUDIO_RX must support 8000 Hz");
        TEST_CHECK(has_rate(arx->sample_rates, 16000));
        TEST_MSG("AUDIO_RX must support 16000 Hz");
        TEST_CHECK(has_rate(arx->sample_rates, 24000));
        TEST_MSG("AUDIO_RX must support 24000 Hz");
        TEST_CHECK(has_rate(arx->sample_rates, 48000));
        TEST_MSG("AUDIO_RX must support 48000 Hz");
        TEST_CHECK(has_rate(arx->sample_rates, 96000));
        TEST_MSG("AUDIO_RX must support 96000 Hz");

        /* Channel range */
        TEST_CHECK(arx->channels_min == 1);
        TEST_MSG("AUDIO_RX channels_min: got %d, expected 1",
                 arx->channels_min);
        TEST_CHECK(arx->channels_max == 2);
        TEST_MSG("AUDIO_RX channels_max: got %d, expected 2",
                 arx->channels_max);

        /* Max concurrent streams */
        TEST_CHECK(arx->max_streams == 4);
        TEST_MSG("AUDIO_RX max_streams: got %d, expected 4",
                 arx->max_streams);
    }

    /* --- AUDIO_TX --- */
    const struct rig_stream_caps *atx = find_caps_by_type(rig->caps,
                                        RIG_STREAM_TYPE_AUDIO_TX);
    TEST_CHECK(atx != NULL);
    TEST_MSG("AUDIO_TX caps must be present");

    if (atx)
    {
        /* Same documented dummy AUDIO bitmask as AUDIO_RX: 0x0F */
        const rig_stream_format_t expected_audio_fmts = 0x0F;

        TEST_CHECK(atx->formats == expected_audio_fmts);
        TEST_MSG("AUDIO_TX formats: got 0x%x, expected 0x%x",
                 atx->formats, expected_audio_fmts);

        TEST_CHECK(has_rate(atx->sample_rates, 8000));
        TEST_CHECK(has_rate(atx->sample_rates, 48000));
        TEST_CHECK(has_rate(atx->sample_rates, 96000));

        TEST_CHECK(atx->channels_min == 1);
        TEST_CHECK(atx->channels_max == 2);
        TEST_CHECK(atx->max_streams == 4);
    }

    /* --- IQ_RX --- */
    const struct rig_stream_caps *iqrx = find_caps_by_type(rig->caps,
                                         RIG_STREAM_TYPE_IQ_RX);
    TEST_CHECK(iqrx != NULL);
    TEST_MSG("IQ_RX caps must be present");

    if (iqrx)
    {
        /* Documented dummy IQ format bitmask:
         * CS8(1<<16) | CU8(1<<17) | CS16(1<<18) | CF32(1<<19)
         * = 0x10000+0x20000+0x40000+0x80000 = 0xF0000 */
        const rig_stream_format_t expected_iq_fmts = 0xF0000;

        TEST_CHECK(iqrx->formats == expected_iq_fmts);
        TEST_MSG("IQ_RX formats: got 0x%x, expected 0x%x",
                 iqrx->formats, expected_iq_fmts);

        TEST_CHECK(has_rate(iqrx->sample_rates, 24000));
        TEST_CHECK(has_rate(iqrx->sample_rates, 48000));
        TEST_CHECK(has_rate(iqrx->sample_rates, 96000));
        TEST_CHECK(has_rate(iqrx->sample_rates, 192000));
        TEST_MSG("IQ_RX must support 192000 Hz");

        TEST_CHECK(iqrx->channels_min == 1);
        TEST_CHECK(iqrx->channels_max == 4);
        TEST_CHECK(iqrx->max_streams == 4);
    }

    /* --- IQ_TX --- */
    const struct rig_stream_caps *iqtx = find_caps_by_type(rig->caps,
                                         RIG_STREAM_TYPE_IQ_TX);
    TEST_CHECK(iqtx != NULL);
    TEST_MSG("IQ_TX caps must be present");

    if (iqtx)
    {
        /* Same documented dummy IQ bitmask as IQ_RX: 0xF0000 */
        const rig_stream_format_t expected_iq_fmts = 0xF0000;

        TEST_CHECK(iqtx->formats == expected_iq_fmts);
        TEST_MSG("IQ_TX formats: got 0x%x, expected 0x%x",
                 iqtx->formats, expected_iq_fmts);

        TEST_CHECK(has_rate(iqtx->sample_rates, 24000));
        TEST_CHECK(has_rate(iqtx->sample_rates, 192000));

        TEST_CHECK(iqtx->channels_min == 1);
        TEST_CHECK(iqtx->channels_max == 4);
        TEST_CHECK(iqtx->max_streams == 4);
    }

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* Open and close a TX stream.
 * Verify session fields and that backend_priv is NULL after close. */
void test_open_close_tx(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_open TX returned %d", ret);
    TEST_CHECK(stream != NULL);

    if (ret == RIG_OK && stream)
    {
        /* Verify session was allocated and populated */
        TEST_CHECK(stream->backend_priv != NULL);
        TEST_MSG("backend_priv must be set after open");

        struct rig_stream_net_session *sess =
            (struct rig_stream_net_session *)stream->backend_priv;
        TEST_CHECK(sess->udp_sock >= 0);
        TEST_MSG("udp_sock=%d, must be >= 0", sess->udp_sock);
        TEST_CHECK(sess->remote_stream_id >= 0);
        TEST_MSG("remote_stream_id=%d, must be >= 0", sess->remote_stream_id);
        TEST_CHECK(sess->remote_udp_port > 0);
        TEST_MSG("remote_udp_port=%d, must be > 0", sess->remote_udp_port);
        TEST_CHECK(sess->remote_udp_port <= 65535);
        TEST_MSG("remote_udp_port=%d, must be <= 65535",
                 sess->remote_udp_port);

        /* TX streams run the receive thread too, to collect PONG and async
         * write-status frames from the server. */
        TEST_CHECK(sess->rx_running == 1);
        TEST_MSG("TX stream must run the receive thread for write-status frames");

        /* Initial TX state */
        TEST_CHECK(sess->tx_seq == 0);
        TEST_MSG("tx_seq must be 0 before any writes");
        TEST_CHECK(sess->tx_timestamp == 0);
        TEST_MSG("tx_timestamp must be 0 before any writes");

        /* Close and verify cleanup */
        ret = rig_stream_close(rig, stream);
        TEST_CHECK(ret == RIG_OK);
        TEST_MSG("rig_stream_close returned %d", ret);
    }

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* Open and close an RX stream.
 * Verify the SUBSCRIBE handshake completed and RX thread is running. */
void test_open_close_rx(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_open RX returned %d", ret);
    TEST_CHECK(stream != NULL);

    if (ret == RIG_OK && stream)
    {
        TEST_CHECK(stream->backend_priv != NULL);

        struct rig_stream_net_session *sess =
            (struct rig_stream_net_session *)stream->backend_priv;

        /* RX stream must have receiver thread running */
        TEST_CHECK(sess->rx_running == 1);
        TEST_MSG("RX stream must have rx_running == 1");

        TEST_CHECK(sess->udp_sock >= 0);
        TEST_CHECK(sess->remote_stream_id >= 0);
        TEST_CHECK(sess->remote_udp_port > 0);

        /* Close and verify cleanup */
        ret = rig_stream_close(rig, stream);
        TEST_CHECK(ret == RIG_OK);
        TEST_MSG("rig_stream_close RX returned %d", ret);
    }

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* Write multiple TX frames and verify seq/timestamp advance correctly.
 * 480 samples of S16LE mono = 960 bytes per frame. */
void test_write_tx_multi(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    struct rig_stream_net_session *sess =
        (struct rig_stream_net_session *)stream->backend_priv;
    TEST_ASSERT(sess != NULL);

    /* Verify initial state */
    TEST_CHECK(sess->tx_seq == 0);
    TEST_CHECK(sess->tx_timestamp == 0);

    /* Prepare 480 samples of S16LE mono = 960 bytes.
     * Each sample is 2 bytes, so 960 / 2 / 1 channel = 480 samples. */
    int16_t samples[480];

    for (int i = 0; i < 480; i++)
    {
        samples[i] = (int16_t)(i & 0x7FFF);
    }

    /* Write 3 frames */
    for (int frame = 0; frame < 3; frame++)
    {
        size_t written = 0;
        ret = rig_stream_write(rig, stream, samples, sizeof(samples),
                               &written, 1000, NULL);
        TEST_CHECK(ret == RIG_OK);
        TEST_MSG("frame %d: rig_stream_write returned %d", frame, ret);
        TEST_CHECK(written == sizeof(samples));
        TEST_MSG("frame %d: written=%zu, expected %zu",
                 frame, written, sizeof(samples));
    }

    /* After 3 writes: seq=3, timestamp=3*480=1440 */
    TEST_CHECK(sess->tx_seq == 3);
    TEST_MSG("tx_seq: got %u, expected 3", sess->tx_seq);
    TEST_CHECK(sess->tx_timestamp == 1440);
    TEST_MSG("tx_timestamp: got %llu, expected 1440",
             (unsigned long long)sess->tx_timestamp);

    /* Write one more with different size: 240 samples = 480 bytes */
    int16_t half_frame[240];

    for (int i = 0; i < 240; i++)
    {
        half_frame[i] = (int16_t)(i & 0x7FFF);
    }

    size_t written = 0;
    ret = rig_stream_write(rig, stream, half_frame, sizeof(half_frame),
                           &written, 1000, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(written == sizeof(half_frame));
    TEST_MSG("half frame: written=%zu, expected %zu",
             written, sizeof(half_frame));

    /* After 4th write: seq=4, timestamp=1440+240=1680 */
    TEST_CHECK(sess->tx_seq == 4);
    TEST_MSG("tx_seq: got %u, expected 4", sess->tx_seq);
    TEST_CHECK(sess->tx_timestamp == 1680);
    TEST_MSG("tx_timestamp: got %llu, expected 1680",
             (unsigned long long)sess->tx_timestamp);

    ret = rig_stream_close(rig, stream);
    TEST_CHECK(ret == RIG_OK);

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* End-to-end write-status: a past-scheduled TX burst makes the server's dummy
 * backend detect a late burst, which must travel back as a WRITE_STATUS frame
 * and surface at the client's rig_stream_wait_write_status(). */
void test_tx_write_status_e2e(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, &cfg, &stream) == RIG_OK);
    TEST_ASSERT(stream != NULL);

    /* Schedule the burst 2 s in the past so the server's dummy scheduler flags
     * it late (CLOCK_REALTIME seconds, matching the dummy's clock source). */
    struct rig_stream_write_info winfo;
    memset(&winfo, 0, sizeof(winfo));
    winfo.time_valid = 1;
    winfo.seconds = (int64_t)time(NULL) - 2;
    winfo.picoseconds = 0;
    winfo.flags = RIG_STREAM_TIME_FLAG_SOB | RIG_STREAM_TIME_FLAG_EOB;

    int16_t samples[480];
    memset(samples, 0, sizeof(samples));
    size_t written = 0;
    TEST_CHECK(rig_stream_write(rig, stream, samples, sizeof(samples),
                                &written, 1000, &winfo) == RIG_OK);

    /* Block until the late-burst event arrives over the wire (or time out). */
    struct rig_stream_write_status ev;
    memset(&ev, 0, sizeof(ev));
    int rc = rig_stream_wait_write_status(rig, stream, &ev, 3000);

    TEST_CHECK_(rc == RIG_OK, "wait_write_status rc=%d (no event over the wire)",
                rc);

    if (rc == RIG_OK)
    {
        TEST_CHECK_(ev.event == RIG_STREAM_WRITE_EVENT_LATE,
                    "event=%u (expected LATE)", ev.event);
        TEST_CHECK_(ev.flags & RIG_STREAM_WRITE_STATUS_REMOTE,
                    "REMOTE flag not set on a server-reported event");
    }

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* End-to-end UNDERRUN: a single plain burst is consumed by the server's dummy
 * backend, which then starves — the resulting UNDERRUN must travel back as a
 * WRITE_STATUS frame and surface (marked REMOTE) at the client. Proves a second
 * event type flows over the full wire path. */
void test_tx_underrun_e2e(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, &cfg, &stream) == RIG_OK);

    int16_t samples[480];
    memset(samples, 0, sizeof(samples));
    size_t written = 0;
    TEST_CHECK(rig_stream_write(rig, stream, samples, sizeof(samples),
                                &written, 1000, NULL) == RIG_OK);

    int got_underrun = 0;

    for (int i = 0; i < 6 && !got_underrun; i++)
    {
        struct rig_stream_write_status ev;
        memset(&ev, 0, sizeof(ev));

        if (rig_stream_wait_write_status(rig, stream, &ev, 700) != RIG_OK)
        {
            continue;
        }

        if (ev.event == RIG_STREAM_WRITE_EVENT_UNDERRUN)
        {
            got_underrun = 1;
            TEST_CHECK_(ev.flags & RIG_STREAM_WRITE_STATUS_REMOTE,
                        "REMOTE flag not set on a server-reported event");
        }
    }

    TEST_CHECK_(got_underrun, "no UNDERRUN event arrived over the wire");

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* RX data path: open AUDIO_RX stream and verify actual audio data arrives.
 * The dummy backend generates a sine wave, so we must receive non-zero
 * samples within a reasonable timeout. */
void test_rx_data_received(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_open RX returned %d", ret);
    TEST_ASSERT(stream != NULL);

    /* Retry loop: allow up to 3 seconds for data to arrive.
     * The dummy backend generates data at real-time rate. The path is:
     * dummy generator → ringbuf → rigctld feeder → UDP → client rx_thread
     * → ringbuf → rig_stream_read. */
    int16_t buf[480];  /* 10ms at 48kHz mono */
    size_t bytes_read = 0;
    int got_data = 0;
    int attempts = 0;
    int max_attempts = 15;  /* 15 * 200ms = 3 seconds */

    while (attempts < max_attempts)
    {
        memset(buf, 0, sizeof(buf));
        bytes_read = 0;
        ret = rig_stream_read(rig, stream, buf, sizeof(buf),
                              &bytes_read, 200, NULL);

        if (ret == RIG_OK && bytes_read > 0)
        {
            got_data = 1;
            break;
        }

        attempts++;
    }

    TEST_CHECK(got_data == 1);
    TEST_MSG("Must receive RX data within 3 seconds (got %d bytes after %d attempts)",
             (int)bytes_read, attempts + 1);

    if (got_data)
    {
        TEST_CHECK(bytes_read > 0);
        TEST_MSG("bytes_read=%zu, must be > 0", bytes_read);

        /* Dummy sine wave should produce non-zero samples.
         * Check that not all samples are zero. */
        int nonzero_count = 0;
        int sample_count = (int)(bytes_read / sizeof(int16_t));

        for (int i = 0; i < sample_count; i++)
        {
            if (buf[i] != 0)
            {
                nonzero_count++;
            }
        }

        TEST_CHECK(nonzero_count > 0);
        TEST_MSG("Received %d samples, %d non-zero (sine wave data expected)",
                 sample_count, nonzero_count);
    }

    ret = rig_stream_close(rig, stream);
    TEST_CHECK(ret == RIG_OK);

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* RX data path: read multiple frames and verify ongoing data flow. */
void test_rx_continuous_data(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    /* Wait for data to start flowing */
    usleep(500000);  /* 500ms */

    /* Read 5 consecutive frames */
    int successful_reads = 0;
    size_t total_bytes = 0;
    int16_t buf[480];

    for (int i = 0; i < 5; i++)
    {
        size_t bytes_read = 0;
        ret = rig_stream_read(rig, stream, buf, sizeof(buf),
                              &bytes_read, 500, NULL);

        if (ret == RIG_OK && bytes_read > 0)
        {
            successful_reads++;
            total_bytes += bytes_read;
        }
    }

    TEST_CHECK(successful_reads >= 3);
    TEST_MSG("Got %d/5 successful reads (expect >= 3)", successful_reads);
    TEST_CHECK(total_bytes > 0);
    TEST_MSG("total_bytes=%zu across %d reads", total_bytes, successful_reads);

    ret = rig_stream_close(rig, stream);
    TEST_CHECK(ret == RIG_OK);

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* Opening a stream with a sample rate not in the caps must fail. */
void test_open_unsupported_rate(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 44100;  /* 44.1kHz not in dummy caps */
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret != RIG_OK);
    TEST_MSG("rig_stream_open with unsupported rate returned %d (expected error)",
             ret);
    TEST_CHECK(stream == NULL);
    TEST_MSG("stream must be NULL on failure");

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* Opening a stream with a format not in the caps must fail. */
void test_open_unsupported_format(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    /* Try opening AUDIO_TX with an IQ format — that shouldn't work */
    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_IQ_CS16;  /* IQ format for audio type */
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret != RIG_OK);
    TEST_MSG("rig_stream_open with IQ format on audio type returned %d "
             "(expected error)", ret);
    TEST_CHECK(stream == NULL);

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* Verify that re-opening a stream after close works (no resource leak). */
void test_open_close_reopen(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    /* First open/close cycle */
    rig_stream_t *stream1 = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream1);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("First open returned %d", ret);
    TEST_ASSERT(stream1 != NULL);

    int first_stream_id = -1;

    {
        struct rig_stream_net_session *sess =
            (struct rig_stream_net_session *)stream1->backend_priv;
        TEST_ASSERT(sess != NULL);
        first_stream_id = sess->remote_stream_id;
    }

    ret = rig_stream_close(rig, stream1);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("First close returned %d", ret);

    /* Second open/close cycle */
    rig_stream_t *stream2 = NULL;
    ret = rig_stream_open(rig, &cfg, &stream2);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("Second open returned %d", ret);
    TEST_ASSERT(stream2 != NULL);

    {
        struct rig_stream_net_session *sess =
            (struct rig_stream_net_session *)stream2->backend_priv;
        TEST_ASSERT(sess != NULL);

        /* Second stream should get a different stream_id from rigctld */
        TEST_CHECK(sess->remote_stream_id != first_stream_id);
        TEST_MSG("Second stream_id=%d must differ from first=%d",
                 sess->remote_stream_id, first_stream_id);

        TEST_CHECK(sess->udp_sock >= 0);
        TEST_CHECK(sess->remote_udp_port > 0);

        /* Fresh session: tx_seq must be 0 */
        TEST_CHECK(sess->tx_seq == 0);
    }

    ret = rig_stream_close(rig, stream2);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("Second close returned %d", ret);

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* Pause an RX stream, verify reads return timeout, then resume and
 * verify data flows again. */
void test_rx_pause_resume(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    /* Wait for data to start flowing */
    int16_t buf[480];
    size_t bytes_read = 0;
    int got_data = 0;

    for (int i = 0; i < 15; i++)
    {
        ret = rig_stream_read(rig, stream, buf, sizeof(buf),
                              &bytes_read, 200, NULL);

        if (ret == RIG_OK && bytes_read > 0)
        {
            got_data = 1;
            break;
        }
    }

    TEST_CHECK(got_data == 1);
    TEST_MSG("Must receive data before testing pause");

    /* Pause the stream */
    ret = rig_stream_pause(rig, stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_pause returned %d", ret);
    TEST_CHECK(stream->paused == 1);

    /* When paused, reads must return timeout */
    bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf),
                          &bytes_read, 200, NULL);
    TEST_CHECK(ret == -RIG_ETIMEOUT);
    TEST_MSG("Paused read returned %d (expected -%d)", ret, RIG_ETIMEOUT);
    TEST_CHECK(bytes_read == 0);
    TEST_MSG("Paused read bytes_read=%zu (expected 0)", bytes_read);

    /* Resume the stream */
    ret = rig_stream_resume(rig, stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_resume returned %d", ret);
    TEST_CHECK(stream->paused == 0);

    /* After resume, data should flow again */
    got_data = 0;

    for (int i = 0; i < 15; i++)
    {
        bytes_read = 0;
        ret = rig_stream_read(rig, stream, buf, sizeof(buf),
                              &bytes_read, 200, NULL);

        if (ret == RIG_OK && bytes_read > 0)
        {
            got_data = 1;
            break;
        }
    }

    TEST_CHECK(got_data == 1);
    TEST_MSG("Must receive data after resume");

    ret = rig_stream_close(rig, stream);
    TEST_CHECK(ret == RIG_OK);

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* --- Test list --- */

/* e2e: server-side dummy anchors travel as embedded TIME blocks and feed
 * the client's anchor ring — the enriched read reports capture time. */
void test_rx_capture_time_propagates(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, &cfg, &stream) == RIG_OK);
    TEST_ASSERT(stream != NULL);

    struct rig_stream_read_info info;
    int16_t buf[960];
    size_t got = 0;
    int valid = 0;

    for (int i = 0; i < 25 && !valid; i++)
    {
        if (rig_stream_read(rig, stream, buf, sizeof(buf), &got, 200,
                            &info) == RIG_OK && got > 0)
        {
            valid = info.time_valid;
        }
    }

    TEST_CHECK_(valid, "capture time never propagated to the client");

    if (valid)
    {
        TEST_CHECK_(info.time_source == RIG_STREAM_TIME_SRC_HOST,
                    "source=%u", info.time_source);
        TEST_CHECK_(info.seconds > 1577836800LL
                    && info.seconds < 4102444800LL,
                    "seconds=%lld", (long long)info.seconds);
    }

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* e2e: flush on a netrigctl TX stream must forward \stream_drain to the
 * daemon, which drains the real backend's TX path. The frontend default only
 * polls the local ring buffer — a no-op for netrigctl TX, which bypasses the
 * ring buffer via stream_write — so netrigctl must override stream_drain. */
void test_tx_flush_forwards(void)
{
    struct rigctld_proc proc = {0};

    if (start_rigctld(&proc) < 0)
    {
        TEST_CHECK_(0, "could not start rigctld");
        return;
    }

    RIG *rig = open_netrigctl(proc.port);
    TEST_ASSERT(rig != NULL);

    /* netrigctl must wire a flush override, not fall back to the local
     * ring-buffer poll (which never drains the remote radio's TX FIFO). */
    TEST_CHECK(rig->caps->stream_drain != NULL);
    TEST_MSG("netrigctl should implement stream_drain to forward to the daemon");

    struct rig_stream_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, &cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    /* Queue a frame of TX samples */
    int16_t samples[480];

    for (int i = 0; i < 480; i++)
    {
        samples[i] = (int16_t)(i & 0x7FFF);
    }

    size_t written = 0;
    ret = rig_stream_write(rig, stream, samples, sizeof(samples),
                           &written, 1000, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Flush must succeed end-to-end: the forward reaches the daemon, which
     * dispatches \stream_drain to the real backend's rig_stream_drain. A bad
     * command or stream id would surface here as a non-OK return. */
    ret = rig_stream_drain(rig, stream, 1000);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_drain returned %d (expected RIG_OK)", ret);

    ret = rig_stream_close(rig, stream);
    TEST_CHECK(ret == RIG_OK);

    rig_close(rig);
    rig_cleanup(rig);
    stop_rigctld(&proc);
}


/* --- Code-based RX packet-processing tests (no subprocess) ---
 * These feed crafted packets straight to rig_stream_net_process_packet so
 * the client's seq/loss accounting is exercised deterministically. */

/* Bare S16 mono stream (2 bytes/frame) + minimal session. */
static void wb_setup(struct rig_stream *s, struct rig_stream_net_session *sess)
{
    memset(s, 0, sizeof(*s));
    TEST_ASSERT(stream_ringbuf_init(&s->ringbuf, 4096) == 0);
    stream_write_event_init(s);
    s->config.format = RIG_STREAM_FORMAT_PCM_S16;
    s->config.channels = 1;
    s->config.sample_rate = 48000;
    s->frame_bytes = 2;
    s->active = 1;
    s->type = RIG_STREAM_TYPE_AUDIO_RX;

    memset(sess, 0, sizeof(*sess));
    sess->remote_stream_id = 7;
    sess->subscribe_token = 0xABCD1234;
    sess->rx_first = 1;
    sess->stream = s;
    s->backend_priv = sess;
}

static size_t wb_data(unsigned char *buf, uint32_t seq, uint64_t ts, int frames,
                      const struct rig_stream_net_session *sess)
{
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_AUDIO_RX;
    hdr.stream_id = (uint16_t)sess->remote_stream_id;
    hdr.subscribe_token = sess->subscribe_token;
    hdr.seq = seq;
    hdr.timestamp = ts;
    hdr.sample_rate = 48000;
    hdr.format = RIG_STREAM_FMT_ID_PCM_S16;
    hdr.channels = 1;
    hdr.payload_len = (uint16_t)(frames * 2);
    stream_packet_header_pack(&hdr, buf);
    memset(buf + RIG_STREAM_HEADER_SIZE, 0x11, (size_t)frames * 2);
    return RIG_STREAM_HEADER_SIZE + (size_t)frames * 2;
}

static size_t wb_meta(unsigned char *buf, uint32_t seq, uint64_t ts,
                      const struct rig_stream_net_session *sess)
{
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_AUDIO_RX;
    hdr.stream_id = (uint16_t)sess->remote_stream_id;
    hdr.subscribe_token = sess->subscribe_token;
    hdr.seq = seq;
    hdr.timestamp = ts;
    hdr.control = RIG_STREAM_CTRL_METADATA;
    hdr.payload_len = RIG_STREAM_METADATA_WIRE_SIZE;
    stream_packet_header_pack(&hdr, buf);

    struct rig_stream_metadata meta;
    memset(&meta, 0, sizeof(meta));
    meta.field_mask = RIG_STREAM_META_VFO_FREQ | RIG_STREAM_META_PTT;
    meta.vfo_freq = 14074000;
    stream_metadata_pack(&meta, buf + RIG_STREAM_HEADER_SIZE);
    return RIG_STREAM_HEADER_SIZE + RIG_STREAM_METADATA_WIRE_SIZE;
}

static size_t wb_ctrl(unsigned char *buf, uint16_t control,
                      const struct rig_stream_net_session *sess)
{
    struct rig_stream_packet_header hdr;
    stream_control_header_init(&hdr, RIG_STREAM_TYPE_AUDIO_RX,
                               (uint16_t)sess->remote_stream_id,
                               sess->subscribe_token, control);
    stream_packet_header_pack(&hdr, buf);
    return RIG_STREAM_HEADER_SIZE;
}

static size_t wb_write_status(unsigned char *buf, uint32_t seq, uint16_t event,
                              uint64_t sample_index,
                              const struct rig_stream_net_session *sess)
{
    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_AUDIO_TX;
    hdr.stream_id = (uint16_t)sess->remote_stream_id;
    hdr.subscribe_token = sess->subscribe_token;
    hdr.seq = seq;
    hdr.timestamp = sample_index;   /* sample_index rides the header */
    hdr.control = RIG_STREAM_CTRL_WRITE_STATUS;
    hdr.payload_len = RIG_STREAM_WRITE_STATUS_WIRE_SIZE;
    stream_packet_header_pack(&hdr, buf);

    struct rig_stream_write_status st;
    memset(&st, 0, sizeof(st));
    st.event = event;
    stream_write_status_pack(&st, buf + RIG_STREAM_HEADER_SIZE);
    return RIG_STREAM_HEADER_SIZE + RIG_STREAM_WRITE_STATUS_WIRE_SIZE;
}

/* A header claiming more payload than the datagram carries must be rejected
 * (the payload_len <= received-body bound) without ingesting anything, so no
 * out-of-bounds read of the payload or time/metadata sub-blocks can occur. */
void test_rx_overlong_payload_len_rejected(void)
{
    struct rig_stream s;
    struct rig_stream_net_session sess;
    wb_setup(&s, &sess);

    unsigned char buf[256];

    struct rig_stream_packet_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_AUDIO_RX;
    hdr.stream_id = (uint16_t)sess.remote_stream_id;
    hdr.subscribe_token = sess.subscribe_token;
    hdr.sample_rate = 48000;
    hdr.format = RIG_STREAM_FMT_ID_PCM_S16;
    hdr.channels = 1;
    hdr.payload_len = 1000;   /* lie: claim far more than is present */
    stream_packet_header_pack(&hdr, buf);

    /* Only 20 payload bytes were actually received. */
    size_t received = RIG_STREAM_HEADER_SIZE + 20;
    TEST_CHECK(rig_stream_net_process_packet(&sess, &s, buf, received) == -1);
    TEST_CHECK(stream_ringbuf_available(&s.ringbuf) == 0);

    stream_ringbuf_destroy(&s.ringbuf);
}

/* A metadata frame between two data packets consumes a seq value on the
 * wire; the client must account for it and NOT report app-link loss. */
void test_rx_metadata_no_false_link_loss(void)
{
    struct rig_stream s;
    struct rig_stream_net_session sess;
    wb_setup(&s, &sess);

    unsigned char buf[256];

    TEST_CHECK(rig_stream_net_process_packet(&sess, &s,
               buf, wb_data(buf, 0, 0, 10, &sess)) == 0);
    TEST_CHECK(rig_stream_net_process_packet(&sess, &s,
               buf, wb_meta(buf, 1, 10, &sess)) == 0);
    TEST_CHECK(rig_stream_net_process_packet(&sess, &s,
               buf, wb_data(buf, 2, 10, 10, &sess)) == 0);

    TEST_CHECK_(s.link_loss == 0, "link_loss=%u (expected 0)", s.link_loss);
    TEST_CHECK(s.dropped_samples_link == 0);
    /* Both data packets' 40 bytes reached the ring buffer. */
    TEST_CHECK_(stream_ringbuf_available(&s.ringbuf) == 40,
                "avail=%zu (expected 40)", stream_ringbuf_available(&s.ringbuf));

    stream_ringbuf_destroy(&s.ringbuf);
}

/* The RX path drops any datagram whose source IP is not the negotiated server
 * (rig_stream_net_source_ip_equal is that decision, checked in
 * recv_from_server before token/stream_id). Tested directly because the
 * localhost harness cannot route a spoofed source address portably. Port is
 * intentionally ignored — a server may send data from a different source port
 * than the control port. */
void test_source_ip_equal_decision(void)
{
    struct sockaddr_in a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.sin_family = AF_INET;
    b.sin_family = AF_INET;

    /* Same IP, different ports -> kept. */
    a.sin_addr.s_addr = htonl(0x7f000001);  /* 127.0.0.1 */
    a.sin_port = htons(5000);
    b.sin_addr.s_addr = htonl(0x7f000001);
    b.sin_port = htons(6000);
    TEST_CHECK(rig_stream_net_source_ip_equal((struct sockaddr *)&a,
               (struct sockaddr *)&b) == 1);

    /* Different IP -> dropped. */
    b.sin_addr.s_addr = htonl(0x7f000002);  /* 127.0.0.2 */
    TEST_CHECK(rig_stream_net_source_ip_equal((struct sockaddr *)&a,
               (struct sockaddr *)&b) == 0);

    /* Different address family -> dropped. */
    struct sockaddr_in6 a6, b6;
    memset(&a6, 0, sizeof(a6));
    memset(&b6, 0, sizeof(b6));
    a6.sin6_family = AF_INET6;
    b6.sin6_family = AF_INET6;
    TEST_CHECK(rig_stream_net_source_ip_equal((struct sockaddr *)&a6,
               (struct sockaddr *)&b) == 0);

    /* Same IPv6 (both ::) -> kept; one bit different -> dropped. */
    TEST_CHECK(rig_stream_net_source_ip_equal((struct sockaddr *)&a6,
               (struct sockaddr *)&b6) == 1);
    b6.sin6_addr.s6_addr[15] = 1;  /* ::1 */
    TEST_CHECK(rig_stream_net_source_ip_equal((struct sockaddr *)&a6,
               (struct sockaddr *)&b6) == 0);
}

/* The subscribe_token is the client-side anti-hijack authenticator: a data
 * frame bearing the wrong token must be dropped without reaching the ring or
 * advancing seq accounting, and must not desync a subsequent valid frame. */
void test_rx_wrong_token_rejected(void)
{
    struct rig_stream s;
    struct rig_stream_net_session sess;
    wb_setup(&s, &sess);

    unsigned char buf[256];

    /* Stamp a mismatching token, then restore the session's expected token so
     * process_packet compares the frame's wrong token against the real one. */
    uint32_t good = sess.subscribe_token;
    sess.subscribe_token = good ^ 0x1u;
    size_t len = wb_data(buf, 0, 0, 10, &sess);
    sess.subscribe_token = good;

    TEST_CHECK(rig_stream_net_process_packet(&sess, &s, buf, len) == -1);
    TEST_CHECK_(stream_ringbuf_available(&s.ringbuf) == 0,
                "avail=%zu (expected 0)", stream_ringbuf_available(&s.ringbuf));

    /* A following good frame is still accepted; the drop did not desync. */
    TEST_CHECK(rig_stream_net_process_packet(&sess, &s,
               buf, wb_data(buf, 0, 0, 10, &sess)) == 0);
    TEST_CHECK_(s.link_loss == 0, "link_loss=%u (expected 0)", s.link_loss);
    TEST_CHECK_(stream_ringbuf_available(&s.ringbuf) == 20,
                "avail=%zu (expected 20)", stream_ringbuf_available(&s.ringbuf));

    stream_ringbuf_destroy(&s.ringbuf);
}

/* A re-sent SUBSCRIBE_ACK mid-stream (seq=0) must not be misread as data. */
void test_rx_ack_no_false_link_loss(void)
{
    struct rig_stream s;
    struct rig_stream_net_session sess;
    wb_setup(&s, &sess);

    unsigned char buf[256];

    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 0, 0, 10, &sess));
    TEST_CHECK(rig_stream_net_process_packet(&sess, &s,
               buf, wb_ctrl(buf, RIG_STREAM_CTRL_SUBSCRIBE_ACK, &sess)) == 0);
    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 1, 10, 10, &sess));

    TEST_CHECK_(s.link_loss == 0, "link_loss=%u (expected 0)", s.link_loss);

    stream_ringbuf_destroy(&s.ringbuf);
}

/* Regression: a genuinely lost data packet (seq gap) is still counted. */
void test_rx_real_gap_counts_link_loss(void)
{
    struct rig_stream s;
    struct rig_stream_net_session sess;
    wb_setup(&s, &sess);

    unsigned char buf[256];

    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 0, 0, 10, &sess));
    /* seq=2 skips seq=1 — a lost 10-frame data packet spanning ts 10..20. */
    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 2, 20, 10, &sess));

    TEST_CHECK_(s.link_loss == 1, "link_loss=%u (expected 1)", s.link_loss);
    TEST_CHECK_(s.dropped_samples_link == 10,
                "dropped_samples_link=%llu (expected 10)",
                (unsigned long long)s.dropped_samples_link);

    stream_ringbuf_destroy(&s.ringbuf);
}

/* A reserved ERROR frame must be dropped, never written as sample data,
 * and must not disturb seq accounting. */
void test_rx_error_frame_dropped(void)
{
    struct rig_stream s;
    struct rig_stream_net_session sess;
    wb_setup(&s, &sess);

    unsigned char buf[256];

    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 0, 0, 10, &sess));
    size_t avail_before = stream_ringbuf_available(&s.ringbuf);

    TEST_CHECK(rig_stream_net_process_packet(&sess, &s,
               buf, wb_ctrl(buf, RIG_STREAM_CTRL_ERROR, &sess)) == 0);

    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 1, 10, 10, &sess));

    TEST_CHECK_(s.link_loss == 0, "link_loss=%u (expected 0)", s.link_loss);
    /* Only the two data packets (2×20 B) reached the ring; ERROR wrote nothing. */
    TEST_CHECK_(stream_ringbuf_available(&s.ringbuf) == avail_before + 20,
                "avail=%zu (expected %zu)",
                stream_ringbuf_available(&s.ringbuf), avail_before + 20);

    stream_ringbuf_destroy(&s.ringbuf);
}

/* A received WRITE_STATUS frame must surface through
 * rig_stream_wait_write_status() (marked REMOTE), bump the matching remote_*
 * stat, and not disturb seq accounting or write sample data. */
void test_rx_write_status_frame(void)
{
    struct rig_stream s;
    struct rig_stream_net_session sess;
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    wb_setup(&s, &sess);
    s.type = RIG_STREAM_TYPE_AUDIO_TX;   /* write-status events are TX-only */

    TEST_ASSERT(rig != NULL);

    unsigned char buf[256];

    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 0, 0, 10, &sess));
    size_t avail_before = stream_ringbuf_available(&s.ringbuf);

    TEST_CHECK(rig_stream_net_process_packet(&sess, &s,
               buf, wb_write_status(buf, 1, RIG_STREAM_WRITE_EVENT_OVERRUN,
                                    4242, &sess)) == 0);

    rig_stream_net_process_packet(&sess, &s, buf, wb_data(buf, 2, 10, 10, &sess));

    /* No false link loss; the status frame wrote nothing to the ring. */
    TEST_CHECK_(s.link_loss == 0, "link_loss=%u (expected 0)", s.link_loss);
    TEST_CHECK_(stream_ringbuf_available(&s.ringbuf) == avail_before + 20,
                "avail=%zu (expected %zu)",
                stream_ringbuf_available(&s.ringbuf), avail_before + 20);

    /* Remote overrun surfaces through the remote_* stat and the event API. */
    TEST_CHECK_(s.remote_overruns == 1, "remote_overruns=%u", s.remote_overruns);

    struct rig_stream_write_status out;
    memset(&out, 0, sizeof(out));
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == RIG_OK);
    TEST_CHECK(out.event == RIG_STREAM_WRITE_EVENT_OVERRUN);
    TEST_CHECK(out.sample_index == 4242);
    TEST_CHECK(out.flags & RIG_STREAM_WRITE_STATUS_REMOTE);
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == -RIG_ETIMEOUT);

    stream_write_event_destroy(&s);
    stream_ringbuf_destroy(&s.ringbuf);
    rig_cleanup(rig);
}


/* --- Subscribe handshake against an unreliable link (no subprocess) ---
 * UDP may drop the SUBSCRIBE or its ACK, and an unrelated control frame may
 * reach the client before the ACK. A stand-in server reproduces both. */

struct sub_server
{
    int sock;
    int port;
    int drop_first;         /* answer nothing to the first SUBSCRIBE */
    int pong_before_ack;    /* emit a PONG ahead of the ACK */
    int subscribes_seen;
    HAMLIB_ATOMIC int stop;
    pthread_t thread;
};


static void *sub_server_thread(void *arg)
{
    struct sub_server *srv = (struct sub_server *)arg;

    while (!srv->stop)
    {
        fd_set fds;
        struct timeval tv = { 0, 50000 };  /* 50ms, so stop is seen promptly */

        FD_ZERO(&fds);
        FD_SET(srv->sock, &fds);

        if (select(srv->sock + 1, &fds, NULL, NULL, &tv) <= 0)
        {
            continue;
        }

        unsigned char buf[RIG_STREAM_MAX_DATAGRAM];
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(srv->sock, buf, sizeof(buf), 0,
                             (struct sockaddr *)&from, &from_len);
        struct rig_stream_packet_header hdr;

        if (n < RIG_STREAM_HEADER_SIZE
                || stream_packet_header_unpack(buf, (size_t)n, &hdr) != 0
                || !(hdr.control & RIG_STREAM_CTRL_SUBSCRIBE))
        {
            continue;
        }

        srv->subscribes_seen++;

        if (srv->drop_first && srv->subscribes_seen == 1)
        {
            continue;
        }

        unsigned char reply[RIG_STREAM_HEADER_SIZE];
        struct rig_stream_packet_header rhdr;

        if (srv->pong_before_ack)
        {
            stream_control_header_init(&rhdr, hdr.type, hdr.stream_id,
                                       hdr.subscribe_token,
                                       RIG_STREAM_CTRL_PONG);
            stream_packet_header_pack(&rhdr, reply);
            sendto(srv->sock, reply, sizeof(reply), 0,
                   (struct sockaddr *)&from, from_len);
        }

        stream_control_header_init(&rhdr, hdr.type, hdr.stream_id,
                                   hdr.subscribe_token,
                                   RIG_STREAM_CTRL_SUBSCRIBE_ACK);
        stream_packet_header_pack(&rhdr, reply);
        sendto(srv->sock, reply, sizeof(reply), 0,
               (struct sockaddr *)&from, from_len);
    }

    return NULL;
}


/* Bind the stand-in server and run it. Behaviour flags must already be set. */
static int sub_server_start(struct sub_server *srv)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    srv->sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (srv->sock < 0)
    {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(srv->sock, (struct sockaddr *)&addr, sizeof(addr)) < 0
            || getsockname(srv->sock, (struct sockaddr *)&addr, &addrlen) < 0)
    {
        close(srv->sock);
        return -1;
    }

    srv->port = ntohs(addr.sin_port);

    if (pthread_create(&srv->thread, NULL, sub_server_thread, srv) != 0)
    {
        close(srv->sock);
        return -1;
    }

    return 0;
}


static void sub_server_stop(struct sub_server *srv)
{
    srv->stop = 1;
    pthread_join(srv->thread, NULL);
    close(srv->sock);
}


/* Client session whose peer is the stand-in server. */
static int sub_session_init(struct rig_stream *s,
                            struct rig_stream_net_session *sess,
                            int server_port)
{
    struct sockaddr_in local;

    wb_setup(s, sess);

    sess->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sess->udp_sock < 0)
    {
        return -1;
    }

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local.sin_port = 0;

    if (bind(sess->udp_sock, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        close(sess->udp_sock);
        return -1;
    }

    struct sockaddr_in *peer = (struct sockaddr_in *)&sess->server_addr;

    memset(peer, 0, sizeof(*peer));
    peer->sin_family = AF_INET;
    peer->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    peer->sin_port = htons((uint16_t)server_port);
    sess->server_addr_len = sizeof(*peer);

    return 0;
}


static void sub_session_cleanup(struct rig_stream *s,
                                struct rig_stream_net_session *sess)
{
    close(sess->udp_sock);
    stream_write_event_destroy(s);
    stream_ringbuf_destroy(&s->ringbuf);
}


/* A dropped SUBSCRIBE must not fail the open: the client retransmits within
 * its budget and the second attempt is answered. */
void test_subscribe_retransmits_after_loss(void)
{
    struct sub_server srv;
    struct rig_stream s;
    struct rig_stream_net_session sess;

    memset(&srv, 0, sizeof(srv));
    srv.drop_first = 1;
    TEST_ASSERT(sub_server_start(&srv) == 0);
    TEST_ASSERT(sub_session_init(&s, &sess, srv.port) == 0);

    TEST_CHECK(rig_stream_net_subscribe(&sess, &s, 3000) == 0);
    TEST_MSG("subscribe failed although the retry would have been answered");

    TEST_CHECK(srv.subscribes_seen >= 2);
    TEST_MSG("server saw %d SUBSCRIBE packets, expected a retransmission",
             srv.subscribes_seen);

    sub_session_cleanup(&s, &sess);
    sub_server_stop(&srv);
}


/* An unrelated control frame arriving before the ACK must be discarded, not
 * taken as a failed handshake. */
void test_subscribe_ignores_non_ack_first(void)
{
    struct sub_server srv;
    struct rig_stream s;
    struct rig_stream_net_session sess;

    memset(&srv, 0, sizeof(srv));
    srv.pong_before_ack = 1;
    TEST_ASSERT(sub_server_start(&srv) == 0);
    TEST_ASSERT(sub_session_init(&s, &sess, srv.port) == 0);

    TEST_CHECK(rig_stream_net_subscribe(&sess, &s, 3000) == 0);
    TEST_MSG("subscribe failed on a PONG preceding the ACK");

    sub_session_cleanup(&s, &sess);
    sub_server_stop(&srv);
}


/* Open a stream over a raw TCP control connection and return the source_id
 * reported in the \stream_open response (-1 on any failure). */
static int query_source_id_once(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        return -1;
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }

    const char *cmd = "+\\stream_open AUDIO_RX PCM_S16 48000\n";

    if (send(sock, cmd, strlen(cmd), 0) != (ssize_t)strlen(cmd))
    {
        close(sock);
        return -1;
    }

    char buf[1024] = "";
    size_t total = 0;

    while (total < sizeof(buf) - 1 && strstr(buf, "RPRT") == NULL)
    {
        fd_set fds;
        struct timeval tv = { 3, 0 };
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        if (select(sock + 1, &fds, NULL, NULL, &tv) <= 0)
        {
            break;
        }

        ssize_t n = recv(sock, buf + total, sizeof(buf) - 1 - total, 0);

        if (n <= 0)
        {
            break;
        }

        total += (size_t)n;
        buf[total] = '\0';
    }

    close(sock);

    int source_id = -1;
    const char *p = strstr(buf, "source_id: ");

    if (p && sscanf(p, "source_id: %d", &source_id) == 1)
    {
        return source_id;
    }

    return -1;
}


/* A daemon that has only just begun listening may leave the first connection
 * unanswered on a busy machine, so allow the query a few attempts before
 * reporting the ID as unavailable. */
static int query_source_id_raw(int port)
{
    int attempt;

    for (attempt = 0; attempt < 3; attempt++)
    {
        int source_id = query_source_id_once(port);

        if (source_id >= 0)
        {
            return source_id;
        }

        usleep(100000);  /* 100ms */
    }

    return -1;
}


/* rigctld --stream-source-id stamps the CLI value; without it a stable
 * derived ID in 0x1000-0xFFFF is used, unchanged across a daemon restart
 * on the same static configuration. */
void test_stream_source_id_cli_and_derived(void)
{
    struct rigctld_proc proc;
    memset(&proc, 0, sizeof(proc));

    /* Explicit CLI value */
    TEST_ASSERT(start_rigctld_opt(&proc, "42") == 0);
    int explicit_id = query_source_id_raw(proc.port);
    stop_rigctld(&proc);
    TEST_CHECK(explicit_id == 42);
    TEST_MSG("explicit source_id=%d, expected 42", explicit_id);

    /* Derived default, stable across a restart on the same port */
    memset(&proc, 0, sizeof(proc));
    TEST_ASSERT(start_rigctld_opt(&proc, NULL) == 0);
    int derived1 = query_source_id_raw(proc.port);
    stop_rigctld(&proc);
    TEST_CHECK(derived1 >= 0x1000 && derived1 <= 0xFFFF);
    TEST_MSG("derived source_id=%d, expected 0x1000-0xFFFF", derived1);

    TEST_ASSERT(start_rigctld_opt(&proc, NULL) == 0);   /* same proc.port */
    int derived2 = query_source_id_raw(proc.port);
    stop_rigctld(&proc);
    TEST_CHECK(derived2 == derived1);
    TEST_MSG("derived ID changed across restart: %d != %d",
             derived2, derived1);
}


TEST_LIST =
{
    { "rx_overlong_payload_len_rejected", test_rx_overlong_payload_len_rejected },
    { "rx_metadata_no_false_link_loss", test_rx_metadata_no_false_link_loss },
    { "source_ip_equal_decision",       test_source_ip_equal_decision },
    { "rx_wrong_token_rejected",        test_rx_wrong_token_rejected },
    { "rx_ack_no_false_link_loss",      test_rx_ack_no_false_link_loss },
    { "rx_real_gap_counts_link_loss",   test_rx_real_gap_counts_link_loss },
    { "rx_error_frame_dropped",         test_rx_error_frame_dropped },
    { "rx_write_status_frame",          test_rx_write_status_frame },
    { "caps_discovery_all_types",  test_caps_discovery_all_types },
    { "rx_capture_time_propagates", test_rx_capture_time_propagates },
    { "open_close_tx",             test_open_close_tx },
    { "open_close_rx",             test_open_close_rx },
    { "write_tx_multi",            test_write_tx_multi },
    { "tx_write_status_e2e",       test_tx_write_status_e2e },
    { "tx_underrun_e2e",           test_tx_underrun_e2e },
    { "rx_data_received",          test_rx_data_received },
    { "rx_continuous_data",        test_rx_continuous_data },
    { "open_unsupported_rate",     test_open_unsupported_rate },
    { "open_unsupported_format",   test_open_unsupported_format },
    { "open_close_reopen",         test_open_close_reopen },
    { "rx_pause_resume",           test_rx_pause_resume },
    { "tx_flush_forwards",         test_tx_flush_forwards },
    { "stream_source_id_cli_and_derived", test_stream_source_id_cli_and_derived },
    { "subscribe_retransmits_after_loss", test_subscribe_retransmits_after_loss },
    { "subscribe_ignores_non_ack_first", test_subscribe_ignores_non_ack_first },
    { NULL, NULL }
};

#endif  /* _WIN32 */
