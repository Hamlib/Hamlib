/*
 *  Hamlib Icom network session tests
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

/* Integration test for the Icom network session against an in-process mock */
/* server over UDP loopback: full handshake + a CI-V command round-trip. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"
#include "network_session.h"
#include "network_seqbuf.h"
#include "network_proto.h"

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "hamlib/rig.h"

#include "icom_network_mock.h"



void test_session_handshake_and_civ_roundtrip(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];
    int n;

    mock_start(&mock);

    memset(&config, 0, sizeof(config));
    strcpy(config.host, "127.0.0.1");
    config.control_port = mock.ctrl_port;
    strcpy(config.username, "user");
    strcpy(config.password, "pass");
    strcpy(config.client_name, "hamlib");
    strcpy(config.radio_name, "IC-7610");
    config.rx_codec = ICOM_NETWORK_CODEC_LPCM16;
    config.sample_rate = 48000;

    s = icom_network_session_alloc(&config);
    TEST_CHECK(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);

    /* CI-V command -> response round-trip through the session seam */
    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));

    n = icom_network_civ_recv(s, rx, sizeof(rx), 1000);
    TEST_CHECK(n == (int)sizeof(mock_freq_resp));

    if (n == (int)sizeof(mock_freq_resp))
    {
        TEST_CHECK(memcmp(rx, mock_freq_resp, n) == 0);
    }

    TEST_CHECK(mock.saw_civ_cmd == 1);

    icom_network_session_free(s);
    mock_stop(&mock);
}


void test_session_stale_frame_drain(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];
    int n;

    mock_start(&mock);
    mock.stale_nak = 1;   /* flush a stale NAK at CI-V stream-open */

    memset(&config, 0, sizeof(config));
    strcpy(config.host, "127.0.0.1");
    config.control_port = mock.ctrl_port;
    strcpy(config.username, "user");
    strcpy(config.password, "pass");
    strcpy(config.client_name, "hamlib");
    strcpy(config.radio_name, "IC-7610");
    config.rx_codec = ICOM_NETWORK_CODEC_LPCM16;
    config.sample_rate = 48000;

    s = icom_network_session_alloc(&config);
    TEST_CHECK(s != NULL);
    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);

    /* the stale NAK must never surface as a command response: the first
     * round-trip after connect returns the real answer */
    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));

    n = icom_network_civ_recv(s, rx, sizeof(rx), 1000);
    TEST_CHECK(n == (int)sizeof(mock_freq_resp));
    TEST_MSG("first frame length=%d first bytes=%02x %02x %02x %02x %02x", n,
             n > 0 ? rx[0] : 0, n > 1 ? rx[1] : 0, n > 2 ? rx[2] : 0,
             n > 3 ? rx[3] : 0, n > 4 ? rx[4] : 0);

    if (n == (int)sizeof(mock_freq_resp))
    {
        TEST_CHECK(memcmp(rx, mock_freq_resp, n) == 0);
    }

    icom_network_session_free(s);
    mock_stop(&mock);
}


/* test router: classify spectrum (cmd 0x27) frames as async and count them */
static volatile int test_async_count;

static int test_async_router(void *ctx, const unsigned char *frame,
                             size_t length)
{
    (void)ctx;

    if (length >= 6 && frame[4] == 0x27)
    {
        test_async_count++;
        return 1; /* consumed */
    }

    return 0; /* let the session queue it as a response */
}

void test_session_async_routing(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];
    int n;

    test_async_count = 0;
    mock_start(&mock);
    mock.send_spectrum = 1; /* emit a spectrum frame before each response */

    memset(&config, 0, sizeof(config));
    strcpy(config.host, "127.0.0.1");
    config.control_port = mock.ctrl_port;
    strcpy(config.username, "user");
    strcpy(config.password, "pass");
    strcpy(config.radio_name, "IC-7610");
    config.rx_codec = ICOM_NETWORK_CODEC_LPCM16;
    config.sample_rate = 48000;

    s = icom_network_session_alloc(&config);
    TEST_CHECK(s != NULL);
    icom_network_session_set_async_cb(s, test_async_router, NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));

    /* civ_recv must return the command response, NOT the spectrum frame */
    n = icom_network_civ_recv(s, rx, sizeof(rx), 1000);
    TEST_CHECK(n == (int)sizeof(mock_freq_resp));

    if (n == (int)sizeof(mock_freq_resp))
    {
        TEST_CHECK(memcmp(rx, mock_freq_resp, n) == 0);
    }

    /* the spectrum frame was routed to the async callback, not queued */
    TEST_CHECK(test_async_count >= 1);

    icom_network_session_free(s);
    mock_stop(&mock);
}


void test_session_audio_rx(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t rx[2048];
    int n;

    mock_start(&mock);

    memset(&config, 0, sizeof(config));
    strcpy(config.host, "127.0.0.1");
    config.control_port = mock.ctrl_port;
    strcpy(config.username, "user");
    strcpy(config.password, "pass");
    strcpy(config.radio_name, "IC-7610");
    config.rx_codec = ICOM_NETWORK_CODEC_LPCM16;
    config.sample_rate = 48000;

    s = icom_network_session_alloc(&config);
    TEST_CHECK(s != NULL);
    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);

    /* start audio: the mock emits one audio payload on stream-open */
    TEST_CHECK(icom_network_audio_start(s) == RIG_OK);

    n = icom_network_audio_recv(s, rx, sizeof(rx), 1000, NULL);
    TEST_CHECK(n == (int)sizeof(mock_audio));

    if (n == (int)sizeof(mock_audio))
    {
        TEST_CHECK(memcmp(rx, mock_audio, n) == 0);
    }

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}


/* Shared config for the capability tests: name-based selection (the default),
 * 48 kHz, TX path requested. */
static void capability_config(struct icom_network_session_config *config,
                              const struct mock_server *mock,
                              const char *radio_name)
{
    memset(config, 0, sizeof(*config));
    strcpy(config->host, "127.0.0.1");
    config->control_port = mock->ctrl_port;
    strcpy(config->username, "user");
    strcpy(config->password, "pass");
    strcpy(config->client_name, "hamlib");
    strcpy(config->radio_name, radio_name);
    config->radio_index = -1;
    config->rx_codec = ICOM_NETWORK_CODEC_LPCM16;
    config->tx_codec = ICOM_NETWORK_CODEC_LPCM16;
    config->sample_rate = 48000;
    config->tx_enable = 1;
}

static void mock_add_radio(struct mock_server *m, const char *name, uint8_t civ,
                           uint16_t rx_rate, uint16_t tx_rate)
{
    int i = m->radio_count++;

    TEST_ASSERT(i < (int)(sizeof(m->radios) / sizeof(m->radios[0])));
    m->radios[i].name = name;
    m->radios[i].civ = civ;
    m->radios[i].rx_rate = rx_rate;
    m->radios[i].tx_rate = tx_rate;
}

/* The name sent on connect is the one the server reported, not the model name
 * the backend was configured with. */
void test_session_capability_name_is_echoed(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);
    mock.radios[0].name = "IC-7610  ";   /* server pads the fixed-width field */

    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_ASSERT(mock.saw_connection_info);
    TEST_CHECK(strcmp((const char *)mock.connection_info + 0x40, "IC-7610") == 0);
    TEST_MSG("name sent = '%s'", (const char *)mock.connection_info + 0x40);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* With several radios advertised, the name picks one and its identity block is
 * echoed verbatim. */
void test_session_capability_select_by_name(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);
    mock_add_radio(&mock, "IC-9700", 0xa2, MOCK_ALL_RATES, MOCK_ALL_RATES);
    mock_add_radio(&mock, "IC-705", 0xa4, MOCK_ALL_RATES, MOCK_ALL_RATES);

    capability_config(&config, &mock, "IC-705");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_ASSERT(mock.saw_connection_info);
    TEST_CHECK(strcmp((const char *)mock.connection_info + 0x40, "IC-705") == 0);
    /* the mock stamps the entry index into the last identity byte */
    TEST_CHECK(mock.connection_info[0x20 + 0x0f] == 2);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* An explicit index overrides name matching. */
void test_session_capability_select_by_index(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);
    mock_add_radio(&mock, "IC-9700", 0xa2, MOCK_ALL_RATES, MOCK_ALL_RATES);

    capability_config(&config, &mock, "IC-7610");
    config.radio_index = 1;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_ASSERT(mock.saw_connection_info);
    TEST_CHECK(strcmp((const char *)mock.connection_info + 0x40, "IC-9700") == 0);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* No advertised radio matches: the open fails rather than connecting to some
 * other radio. */
void test_session_capability_no_match(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);

    capability_config(&config, &mock, "IC-9700");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == -RIG_ECONF);
    TEST_CHECK(mock.saw_connection_info == 0);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

void test_session_capability_index_out_of_range(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);

    capability_config(&config, &mock, "IC-7610");
    config.radio_index = 3;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == -RIG_ECONF);
    TEST_CHECK(mock.saw_connection_info == 0);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A rate the radio does not advertise fails at open, where the message can name
 * the rates it does support, rather than later in the audio path. */
void test_session_capability_rate_rejected(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);
    mock.radios[0].rx_rate = ICOM_NETWORK_RATE_8000;
    mock.radios[0].tx_rate = ICOM_NETWORK_RATE_8000;

    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == -RIG_ECONF);
    TEST_CHECK(mock.saw_connection_info == 0);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);

    /* the same radio at a rate it does advertise connects */
    mock_start(&mock);
    mock.radios[0].rx_rate = ICOM_NETWORK_RATE_8000;
    mock.radios[0].tx_rate = ICOM_NETWORK_RATE_8000;

    capability_config(&config, &mock, "IC-7610");
    config.sample_rate = 8000;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A radio advertising no TX rate (an RX-only model) still connects, but no TX
 * audio path is requested and none can be opened. */
void test_session_capability_tx_suppressed(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);
    mock.radios[0].tx_rate = 0;

    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_CHECK(icom_network_session_tx_audio_available(s) == 0);
    TEST_ASSERT(mock.saw_connection_info);
    TEST_CHECK(mock.connection_info[0x70] == 1);   /* rx_enable */
    TEST_CHECK(mock.connection_info[0x71] == 0);   /* tx_enable suppressed */

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);

    /* and a TX-capable radio does reserve the path */
    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_CHECK(icom_network_session_tx_audio_available(s) == 1);
    TEST_ASSERT(mock.saw_connection_info);
    TEST_CHECK(mock.connection_info[0x71] == 1);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A loss burst wider than the replay window cannot be recovered packet by
 * packet. The tracker signals that, and the session must resynchronise instead
 * of asking for retransmits forever -- for a long time it signalled and nobody
 * listened. */
void test_session_sequence_resync(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];
    unsigned civ_resyncs = 0, audio_resyncs = 0;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    /* Establish the baseline: the tracker ignores the very first sequence it
     * sees, so a gap is only detectable from the second packet onwards. */
    TEST_ASSERT(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
    TEST_ASSERT(icom_network_civ_recv(s, rx, sizeof(rx), 1000) > 0);

    icom_network_session_resync_counts(s, &civ_resyncs, NULL);
    TEST_CHECK(civ_resyncs == 0);
    TEST_MSG("a clean link must not resync, got %u", civ_resyncs);

    /* jump further than ICOM_NETWORK_MISSING_FLUSH in one step */
    mock.civ_sequence_jump = ICOM_NETWORK_MISSING_FLUSH + 10;

    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
    TEST_CHECK(icom_network_civ_recv(s, rx, sizeof(rx), 1000) > 0);

    icom_network_session_resync_counts(s, &civ_resyncs, &audio_resyncs);
    TEST_CHECK(civ_resyncs == 1);
    TEST_MSG("expected one CI-V resync, got %u", civ_resyncs);
    TEST_CHECK(audio_resyncs == 0);

    /* and the session keeps working afterwards -- the point of resyncing */
    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
    TEST_CHECK(icom_network_civ_recv(s, rx, sizeof(rx), 1000) > 0);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* Minimal connected session against the mock, for the CI-V tests below. */
static struct icom_network_session *civ_session(struct mock_server *mock)
{
    struct icom_network_session_config config;
    struct icom_network_session *s;

    capability_config(&config, mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    return s;
}

/* A CI-V reply whose packet happens to be as long as a management packet is
 * still delivered: the CI-V socket classifies by structure, not length. */
void test_session_civ_reply_with_management_length(void)
{
    static const int totals[] = { 0x40, 0x50, 0x60, 0x80, 0x90, 0xa8 };
    struct mock_server mock;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x1a, 0x00, 0xfd };
    uint8_t rx[512];
    size_t i;

    mock_start(&mock);
    s = civ_session(&mock);

    for (i = 0; i < sizeof(totals) / sizeof(totals[0]); i++)
    {
        int want = totals[i] - ICOM_NETWORK_CIV_LEN;
        int n;

        mock.civ_reply_length = want;
        TEST_ASSERT(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
        n = icom_network_civ_recv(s, rx, sizeof(rx), 1000);
        TEST_CHECK_(n == want, "packet of 0x%x bytes: got %d-byte frame, expected %d",
                    (unsigned)totals[i], n, want);
    }

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* The radio numbers its CI-V-socket idles in the same sequence as its frames.
 * Tracking only the frames saw a gap for every idle and kept asking the radio
 * to resend idles (and resynced whenever idles outnumbered the window). */
void test_session_civ_idles_not_requested(void)
{
    struct mock_server mock;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];
    unsigned civ_resyncs = 99;
    int i;

    mock_start(&mock);
    s = civ_session(&mock);

    /* several commands spread over idles, with more idles in between than the
     * resync threshold would allow */
    for (i = 0; i < 4; i++)
    {
        TEST_ASSERT(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
        TEST_CHECK(icom_network_civ_recv(s, rx, sizeof(rx), 1000) > 0);
        hl_usleep(i == 2 ? (ICOM_NETWORK_MISSING_FLUSH + 20) * 50 * 1000 : 300 * 1000);
    }

    icom_network_session_resync_counts(s, &civ_resyncs, NULL);
    TEST_CHECK_(mock.civ_retransmit_requests == 0,
                "%d retransmit requests on a clean link",
                mock.civ_retransmit_requests);
    TEST_CHECK_(civ_resyncs == 0, "%u resyncs on a clean link", civ_resyncs);

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A reply that arrives twice is delivered once; a repeated ACK or reply would
 * otherwise be taken as the answer to the next command. */
void test_session_civ_duplicate_dropped(void)
{
    struct mock_server mock;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];
    int n;

    mock_start(&mock);
    s = civ_session(&mock);

    mock.civ_duplicate_reply = 1;
    TEST_ASSERT(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
    TEST_CHECK(icom_network_civ_recv(s, rx, sizeof(rx), 1000)
               == (int)sizeof(mock_freq_resp));
    n = icom_network_civ_recv(s, rx, sizeof(rx), 300);
    TEST_CHECK_(n == -RIG_ETIMEOUT, "second copy delivered (%d)", n);

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A socket error the system would pass on as momentary, and one it would not.
 * The session must tell them apart, and the codes differ per platform. */
#ifdef __MINGW32__
#define TEST_TRANSIENT_SOCKET_ERROR WSAEWOULDBLOCK
#define TEST_HARD_SOCKET_ERROR      WSAECONNRESET
#else
#define TEST_TRANSIENT_SOCKET_ERROR EAGAIN
#define TEST_HARD_SOCKET_ERROR      ECONNREFUSED
#endif

/* Wait up to ms for the session to be declared lost; returns the time taken,
 * or -1 if it stayed valid. */
static int wait_lost(struct icom_network_session *s, int ms)
{
    int waited;

    for (waited = 0; waited < ms; waited += 10)
    {
        if (!icom_network_session_is_valid(s)) { return waited; }

        hl_usleep(10 * 1000);
    }

    return -1;
}

/* The radio stops serving the CI-V stream while the control socket carries
 * on exchanging keepalives. Before, any packet on any socket kept the one
 * liveness clock fresh, so the session looked healthy with CI-V dead. */
void test_session_civ_silence_is_link_timeout(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 1000;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    /* healthy for longer than the timeout first */
    TEST_CHECK(wait_lost(s, 1500) < 0);

    mock.civ_silent = 1;
    waited = wait_lost(s, 3000);
    TEST_CHECK_(waited >= 0, "still valid 3 s after CI-V went silent");
    TEST_CHECK_(icom_network_session_loss_reason(s) == RIG_COMM_REASON_LINK_TIMEOUT,
                "reason %u", icom_network_session_loss_reason(s));

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* The radio's CI-V port refuses packets. Where the system reports that (an
 * ICMP port-unreachable reaching a connected UDP socket), it shows up as a
 * socket error on the send or the receive, a CI-V command fails at once
 * instead of timing out, and the session is lost as SOCKET_ERROR rather than
 * LINK_TIMEOUT once the liveness timeout runs out. Windows does not report it
 * on a loopback socket, so there the refusal is indistinguishable from
 * silence and only the loss itself is checked. */
void test_session_civ_port_refusing_is_socket_error(void)
{
#ifdef __MINGW32__
    /* Windows does not report a refused local port to a connected UDP socket,
     * so there is nothing to observe here; the injected-error test below
     * covers the same paths on every system. */
    TEST_MSG("skipped: Windows does not report a refused loopback port");
    TEST_CHECK(1);
    return;
#else
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    int i, eio = 0, waited = -1;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 1000;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    mock.close_civ = 1;

    for (i = 0; i < 40; i++)
    {
        if (icom_network_civ_send(s, cmd, sizeof(cmd)) == -RIG_EIO) { eio++; }

        if (!icom_network_session_is_valid(s))
        {
            waited = i * 100;
            break;
        }

        hl_usleep(100 * 1000);
    }

    /* No lower bound on the time: the timeout is measured from the socket's
     * last packet, which is already a little way in the past when the port
     * closes. */
    TEST_CHECK_(waited >= 0 && waited < 3000, "lost after %d ms", waited);
    TEST_CHECK_(eio > 0, "no CI-V send reported the refused port");
    TEST_CHECK_(icom_network_session_loss_reason(s) == RIG_COMM_REASON_SOCKET_ERROR,
                "reason %u", icom_network_session_loss_reason(s));

    icom_network_session_free(s);
    mock_stop(&mock);
#endif
}

/* The same paths driven through the test seam, so they are covered where the
 * system will not produce a real socket error (Windows, for a local peer):
 * a socket that reported an error fails its sends at once, and silence after
 * it is reported as SOCKET_ERROR rather than LINK_TIMEOUT. */
void test_session_injected_socket_error_is_socket_error(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 1000;
    /* No CI-V idles at all: connect() drains the socket, so after this the
     * stream is genuinely quiet and no packet can arrive later to clear the
     * error the test is about to inject (any packet does -- see
     * icom_network_session_rx_common). Silencing the mock alone leaves an
     * idle already on its way. */
    mock.civ_idle_ms = 0;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    /* The socket has to stay silent, or the next packet clears the error. */
    mock.civ_silent = 1;
    icom_network_session_test_fail_socket(s, ICOM_NETWORK_ROLE_CIV,
                                          TEST_HARD_SOCKET_ERROR);

    TEST_CHECK_(icom_network_civ_send(s, cmd, sizeof(cmd)) == -RIG_EIO,
                "a send on a failing socket must not report success");

    waited = wait_lost(s, 3000);
    TEST_CHECK_(waited >= 0, "still valid 3 s after the error");
    TEST_CHECK_(icom_network_session_loss_reason(s) == RIG_COMM_REASON_SOCKET_ERROR,
                "reason %u, expected SOCKET_ERROR",
                icom_network_session_loss_reason(s));

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* Sockets usually go quiet together, and then the reason should be the more
 * telling one: only the CI-V socket here reports an error, and the control
 * socket -- silent, healthy, and the first one checked -- must not turn that
 * into plain silence. Both are put well past the timeout, CI-V the fresher of
 * the two so it is reached last, which is the case that used to be masked. */
void test_session_socket_error_wins_over_a_silent_link(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 1000;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    /* Nothing answers from here on, so the ages set below stand. */
    mock.go_silent = 1;
    icom_network_session_test_fail_socket(s, ICOM_NETWORK_ROLE_CIV,
                                          TEST_HARD_SOCKET_ERROR);
    icom_network_session_test_age_socket(s, ICOM_NETWORK_ROLE_CONTROL, 5000);
    icom_network_session_test_age_socket(s, ICOM_NETWORK_ROLE_CIV, 4900);

    waited = wait_lost(s, 3000);
    TEST_CHECK_(waited >= 0, "still valid 3 s after the link went quiet");
    TEST_CHECK_(icom_network_session_loss_reason(s) == RIG_COMM_REASON_SOCKET_ERROR,
                "reason %u, expected SOCKET_ERROR",
                icom_network_session_loss_reason(s));

    icom_network_session_free(s);
    mock_stop(&mock);
}


/* A momentary error is not a failing socket: the send still goes out, and if
 * the radio then goes quiet the session is lost as plain silence. This is what
 * keeps a busy send buffer or an interrupted call from being read as a dead
 * link -- and the codes that mean "momentary" are platform-specific, so this
 * covers the Windows list where it runs. */
void test_session_transient_socket_error_is_not_a_failure(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 1000;
    /* No CI-V idles at all: connect() drains the socket, so after this the
     * stream is genuinely quiet and no packet can arrive later to clear the
     * error the test is about to inject (any packet does -- see
     * icom_network_session_rx_common). Silencing the mock alone leaves an
     * idle already on its way. */
    mock.civ_idle_ms = 0;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    mock.civ_silent = 1;
    icom_network_session_test_fail_socket(s, ICOM_NETWORK_ROLE_CIV,
                                          TEST_TRANSIENT_SOCKET_ERROR);

    TEST_CHECK_(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd),
                "a momentary error must not fail the send");

    waited = wait_lost(s, 3000);
    TEST_CHECK_(waited >= 0, "still valid 3 s after the radio went quiet");
    TEST_CHECK_(icom_network_session_loss_reason(s) == RIG_COMM_REASON_LINK_TIMEOUT,
                "reason %u, expected LINK_TIMEOUT",
                icom_network_session_loss_reason(s));

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* With the liveness timeout at 0 (never give up), socket errors do not end the
 * session either; where the system reports a refused port, commands also fail
 * at once while it lasts (see above for Windows). */
void test_session_socket_errors_respect_liveness_disabled(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    int i, eio = 0;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 0;
    /* No CI-V idles at all: connect() drains the socket, so after this the
     * stream is genuinely quiet and no packet can arrive later to clear the
     * error the test is about to inject (any packet does -- see
     * icom_network_session_rx_common). Silencing the mock alone leaves an
     * idle already on its way. */
    mock.civ_idle_ms = 0;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    /* through the seam, so this holds on systems that never report a refused
     * port of their own */
    mock.civ_silent = 1;
    icom_network_session_test_fail_socket(s, ICOM_NETWORK_ROLE_CIV,
                                          TEST_HARD_SOCKET_ERROR);

    for (i = 0; i < 25; i++)
    {
        if (icom_network_civ_send(s, cmd, sizeof(cmd)) == -RIG_EIO) { eio++; }

        hl_usleep(100 * 1000);
    }

    TEST_CHECK_(eio > 0, "a send on a failing socket must not report success");
    TEST_CHECK_(icom_network_session_is_valid(s),
                "session lost (reason %u) with the liveness timeout disabled",
                icom_network_session_loss_reason(s));

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* Freeing a session whose reconnect thread is running, without a disconnect
 * first: the thread has cleared connected, so free() used to skip the join and
 * free the session under it. Both the handshake phase (the radio still
 * silent) and the backoff phase (every attempt refused at once) are covered;
 * free() must return promptly and nothing may be sent afterwards. */
static void free_during_reconnect(int refuse)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    struct timespec t0, t1;
    long elapsed_ms;
    int before;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 500;
    config.auto_reconnect = 1;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    mock.go_silent = 1;
    TEST_ASSERT(wait_lost(s, 3000) >= 0);

    if (refuse)
    {
        /* answer again, but refuse the connection: attempts fail quickly and
         * the thread spends its time in backoff */
        mock.status_error = 1;
        mock.go_silent = 0;
        hl_usleep(2500 * 1000);
    }
    else
    {
        /* first attempt after 1 s of backoff, then a long handshake */
        hl_usleep(1600 * 1000);
    }

    clock_gettime(CLOCK_MONOTONIC, &t0);
    icom_network_session_free(s);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000
                 + (t1.tv_nsec - t0.tv_nsec) / 1000000;

    TEST_CHECK_(elapsed_ms < 1500, "free() took %ld ms", elapsed_ms);

    /* Let anything already on its way arrive before counting: what this looks
     * for is a thread that outlived free() and keeps sending, which shows up
     * as traffic for as long as it runs. */
    hl_usleep(300 * 1000);
    before = mock.rx_packets;
    hl_usleep(2500 * 1000);
    TEST_CHECK_(mock.rx_packets == before,
                "%d packets sent after free()", mock.rx_packets - before);

    mock_stop(&mock);
}

void test_session_free_during_reconnect_handshake(void)
{
    free_during_reconnect(0);
}

void test_session_free_during_reconnect_backoff(void)
{
    free_during_reconnect(1);
}

#ifdef __MINGW32__
/* An application that links libhamlib and opens an Icom LAN rig has not
 * started Winsock, and the session opens its sockets itself rather than
 * through network_open(), so it has to hold a reference of its own.
 *
 * Winsock counts references, so the check is: with only the session holding
 * one, sockets still work; once it is freed, they do not. The process is put
 * into a known state first -- every reference released -- because this test
 * cannot otherwise tell its own reference from someone else's, and a test
 * that cannot fail is worse than no test. Nothing else in this process is
 * using sockets at that point: each test starts and stops its own mock. */
void test_session_starts_winsock(void)
{
    struct icom_network_session_config config;
    struct icom_network_session *s;
    WSADATA wsadata;
    SOCKET probe;

    while (WSACleanup() == 0) { }   /* drain every reference */

    probe = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT_(probe == INVALID_SOCKET,
                 "Winsock is still started after releasing every reference");

    /* one of our own, so the rest of the process keeps working */
    TEST_ASSERT(WSAStartup(MAKEWORD(2, 2), &wsadata) == 0);

    memset(&config, 0, sizeof(config));
    strcpy(config.host, "127.0.0.1");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    /* drop ours: the session's reference alone must keep Winsock up */
    WSACleanup();
    probe = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_CHECK_(probe != INVALID_SOCKET,
                "the session holds no Winsock reference (error %d)",
                WSAGetLastError());

    if (probe != INVALID_SOCKET) { closesocket(probe); }

    /* and it gives that reference back */
    icom_network_session_free(s);
    probe = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_CHECK_(probe == INVALID_SOCKET,
                "the session did not release its Winsock reference");

    if (probe != INVALID_SOCKET) { closesocket(probe); }

    /* leave the process as the tests after this one expect to find it */
    WSAStartup(MAKEWORD(2, 2), &wsadata);
}
#endif

/* Application calls racing a reconnect that replaces the sockets: they must
 * fail cleanly (not send on a closed or reused descriptor), and work again
 * once the session is back. Meant to be run under ThreadSanitizer too. */
struct hammer
{
    struct icom_network_session *s;
    HAMLIB_ATOMIC int stop;
    HAMLIB_ATOMIC int sent, failed;
};

static void *hammer_civ(void *arg)
{
    struct hammer *h = arg;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };

    while (!h->stop)
    {
        if (icom_network_civ_send(h->s, cmd, sizeof(cmd)) > 0) { h->sent++; }
        else { h->failed++; }

        hl_usleep(2 * 1000);
    }

    return NULL;
}

void test_session_civ_send_during_reconnect(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct hammer h;
    pthread_t thread;
    int cycle;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 300;
    config.auto_reconnect = 1;
    memset(&h, 0, sizeof(h));
    h.s = icom_network_session_alloc(&config);
    TEST_ASSERT(h.s != NULL);
    TEST_ASSERT(icom_network_session_connect(h.s) == RIG_OK);
    TEST_ASSERT(pthread_create(&thread, NULL, hammer_civ, &h) == 0);

    for (cycle = 0; cycle < 2; cycle++)
    {
        int waited;

        mock.go_silent = 1;
        TEST_CHECK(wait_lost(h.s, 3000) >= 0);
        mock.go_silent = 0;

        for (waited = 0; waited < 8000 && !icom_network_session_is_valid(h.s);
                waited += 10)
        {
            hl_usleep(10 * 1000);
        }

        TEST_CHECK_(icom_network_session_is_valid(h.s),
                    "cycle %d: not re-established", cycle);
    }

    h.sent = 0;
    hl_usleep(300 * 1000);
    TEST_CHECK_(h.sent > 0, "no CI-V send succeeded after the reconnects");

    h.stop = 1;
    pthread_join(thread, NULL);
    icom_network_session_free(h.s);
    mock_stop(&mock);
}

/* A lost handshake reply is recovered the way the protocol intends: the gap
 * the radio's next tracked packet leaves is noticed and the reply requested
 * again. Resending the request does not help, because the radio has already
 * received that sequence and does not answer it twice. */
void test_session_handshake_recovers_lost_reply(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    struct timespec t0, t1;
    long elapsed_ms;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    mock.ctrl_tracked = 1;
    mock.ctrl_ignore_resends = 1;
    mock.drop_capabilities_replies = 1;

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000
                 + (t1.tv_nsec - t0.tv_nsec) / 1000000;

    TEST_CHECK_(mock.ctrl_retransmit_requests >= 1,
                "the lost capabilities reply must be requested again");
    TEST_CHECK_(elapsed_ms < 3000, "connect took %ld ms", elapsed_ms);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* The radio numbers its tracked replies from 0, so the login response is the
 * very first one. Lost, it leaves no gap to notice unless tracking expects 0;
 * found on an IC-7610 with 5% loss, where a lost capabilities reply (1) right
 * after the login response (0) failed the connect. */
void test_session_handshake_recovers_lost_first_reply(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    mock.ctrl_tracked = 1;
    mock.ctrl_ignore_resends = 1;
    mock.drop_login_replies = 1;

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_CHECK_(mock.ctrl_retransmit_requests >= 1,
                "the lost login reply must be requested again");

    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A connect that fails after login gives the token back and disconnects, so
 * the radio does not hold the slot and refuse the next attempt. */
void test_session_failed_connect_releases_slot(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    mock.status_error = 0xffffffffu;

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_CHECK(icom_network_session_connect(s) == -RIG_EPROTO);

    for (waited = 0; waited < 100 && (mock.saw_token_remove == 0
                                      || mock.saw_ctrl_disconnect == 0); waited++)
    {
        usleep(10 * 1000);
    }

    TEST_CHECK_(mock.saw_token_remove >= 1,
                "the token must be removed after a failed connect");
    TEST_CHECK_(mock.saw_ctrl_disconnect >= 1,
                "the control socket must disconnect after a failed connect");
    icom_network_session_free(s);

    /* And a new attempt then succeeds. */
    mock.status_error = 0;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}


/* ---- audio ordering and loss (reorder window) ---- */

struct audio_rx_item
{
    int seq;                          /* the payload's first sample value */
    int bytes;
    struct icom_network_audio_loss loss;
};

/* Start audio with the given reorder window and swallow the one packet the mock
 * emits on stream-open (sequence 0), so scripts start from a clean baseline. */
static struct icom_network_session *audio_session(struct mock_server *mock,
        uint32_t reorder_ms)
{
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t rx[2048];

    capability_config(&config, mock, "IC-7610");
    config.rx_reorder_ms = reorder_ms;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);
    TEST_ASSERT(icom_network_audio_start(s) == RIG_OK);
    TEST_ASSERT(icom_network_audio_recv(s, rx, sizeof(rx), 1000, NULL) > 0);

    return s;
}

static void audio_script(struct mock_server *mock, const uint16_t *seqs,
                         int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        mock->audio_script[i] = seqs[i];
        mock->audio_script_bytes[i] = 16;
    }

    mock->audio_script_count = count;
    mock->audio_script_go = 1;
}

/* Collect up to max payloads, stopping after idle_ms with nothing new. */
static int audio_collect(struct icom_network_session *s,
                         struct audio_rx_item *items, int max, int idle_ms)
{
    uint8_t rx[2048];
    int count = 0;

    while (count < max)
    {
        struct icom_network_audio_loss loss;
        int n = icom_network_audio_recv(s, rx, sizeof(rx), idle_ms, &loss);

        if (n <= 0) { break; }

        items[count].seq = rx[0] | (rx[1] << 8);
        items[count].bytes = n;
        items[count].loss = loss;
        count++;
    }

    return count;
}

/* Window 0: a gap is attached to the payload after it, sized from the packet
 * sizes already seen, and no retransmit is ever asked for. */
void test_session_audio_window0_gap(void)
{
    static const uint16_t script[] = { 1, 2, 5, 6 };
    struct mock_server mock;
    struct icom_network_session *s;
    struct audio_rx_item got[8];
    int n;

    mock_start(&mock);
    s = audio_session(&mock, 0);

    audio_script(&mock, script, 4);
    n = audio_collect(s, got, 8, 500);

    TEST_CHECK_(n == 4, "got %d payloads, expected 4", n);

    if (n == 4)
    {
        TEST_CHECK(got[0].seq == 1 && got[1].seq == 2);
        TEST_CHECK(got[2].seq == 5 && got[3].seq == 6);
        TEST_CHECK(got[0].loss.lost_packets == 0 && got[1].loss.lost_packets == 0);
        TEST_CHECK_(got[2].loss.lost_packets == 2, "lost_packets=%u",
                    got[2].loss.lost_packets);
        TEST_CHECK_(got[2].loss.lost_bytes == 32, "lost_bytes=%u (2 x 16)",
                    got[2].loss.lost_bytes);
        TEST_CHECK(got[3].loss.lost_packets == 0);
    }

    usleep(300 * 1000);
    TEST_CHECK_(mock.audio_retransmit_requests == 0,
                "window 0 must not request retransmits, saw %d",
                mock.audio_retransmit_requests);

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* Window 0: a packet that arrives after the one following it is dropped, never
 * played out of order. */
void test_session_audio_window0_late_dropped(void)
{
    static const uint16_t script[] = { 1, 3, 2, 4 };
    struct mock_server mock;
    struct icom_network_session *s;
    struct audio_rx_item got[8];
    struct stream_reorder_stats st;
    int n;

    mock_start(&mock);
    s = audio_session(&mock, 0);

    audio_script(&mock, script, 4);
    n = audio_collect(s, got, 8, 500);

    TEST_CHECK_(n == 3, "got %d payloads, expected 3", n);

    if (n == 3)
    {
        TEST_CHECK(got[0].seq == 1 && got[1].seq == 3 && got[2].seq == 4);
        TEST_CHECK(got[1].loss.lost_packets == 1);
    }

    icom_network_session_audio_stats(s, &st);
    TEST_CHECK_(st.late == 1, "late=%llu", (unsigned long long)st.late);

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A window puts a reordered packet back in place, with no loss reported. */
void test_session_audio_window_reorders(void)
{
    static const uint16_t script[] = { 1, 3, 2, 4 };
    struct mock_server mock;
    struct icom_network_session *s;
    struct audio_rx_item got[8];
    int n, i;

    mock_start(&mock);
    s = audio_session(&mock, 200);

    audio_script(&mock, script, 4);
    n = audio_collect(s, got, 8, 600);

    TEST_CHECK_(n == 4, "got %d payloads, expected 4", n);

    for (i = 0; i < n; i++)
    {
        TEST_CHECK_(got[i].seq == i + 1, "payload %d is seq %d", i, got[i].seq);
        TEST_CHECK(got[i].loss.lost_packets == 0 && !got[i].loss.lost_unsized);
    }

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A window asks the radio for a missing packet and slots the resend into place:
 * nothing is lost. */
void test_session_audio_window_retransmit_recovers(void)
{
    static const uint16_t script[] = { 1, 2, 4, 5 };
    struct mock_server mock;
    struct icom_network_session *s;
    struct audio_rx_item got[8];
    int n, i;

    mock_start(&mock);
    s = audio_session(&mock, 300);

    mock.audio_withheld = 3;
    mock.audio_withheld_bytes = 16;
    audio_script(&mock, script, 4);
    n = audio_collect(s, got, 8, 800);

    TEST_CHECK_(n == 5, "got %d payloads, expected 5", n);

    for (i = 0; i < n; i++)
    {
        TEST_CHECK_(got[i].seq == i + 1, "payload %d is seq %d", i, got[i].seq);
        TEST_CHECK(got[i].loss.lost_packets == 0);
    }

    TEST_CHECK(mock.audio_retransmit_requests >= 1);
    TEST_CHECK(mock.audio_withheld == -1);

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* The hold window used by the retransmission tests. The session repeats a
 * request every window/2, capped at 100 ms, so the second request falls due
 * 100 ms after the hole and the window does not close for another 300 ms.
 * Nothing here turns on the machine being able to hold a deadline: the test
 * waits for the session to repeat the request, not for time to pass. The
 * window/2 spacing itself is checked, against a fake clock, by
 * test_stream_reorder's next_request test. */
#define AUDIO_RETRY_WINDOW_MS 400

/* Wait until the mock has seen `want` retransmit requests on the audio
 * socket. Returns the count it reached, so a caller that timed out can say
 * how far it got. */
static int audio_wait_requests(struct mock_server *mock, int want,
                               int timeout_ms)
{
    int waited;

    for (waited = 0; waited < timeout_ms; waited += 5)
    {
        int seen = mock->audio_retransmit_requests;

        if (seen >= want) { return seen; }

        hl_usleep(5 * 1000);
    }

    return mock->audio_retransmit_requests;
}

/* Packet 3 is withheld and the first resend is dropped, so the packet only
 * arrives if the session asks a second time. `trailing` says whether traffic
 * keeps flowing after the hole: without it the receiver has nothing to wake
 * it but the retry deadline itself. */
static void audio_retry_check(int trailing)
{
    static const uint16_t with_trailing[] = { 1, 2, 4, 5 };
    static const uint16_t without_trailing[] = { 1, 2, 4 };
    const uint16_t *script = trailing ? with_trailing : without_trailing;
    int count = trailing ? 4 : 3;
    int expected = count + 1;          /* the withheld packet, recovered */
    struct mock_server mock;
    struct icom_network_session *s;
    struct audio_rx_item got[8];
    int n, i, requests;

    mock_start(&mock);
    s = audio_session(&mock, AUDIO_RETRY_WINDOW_MS);

    mock.audio_withheld = 3;
    mock.audio_withheld_bytes = 16;
    mock.audio_withheld_ignore = 1;   /* the first resend is lost */
    audio_script(&mock, script, count);

    requests = audio_wait_requests(&mock, 2, 3000);
    TEST_CHECK_(requests >= 2, "the lost resend was requested %d time(s)",
                requests);

    n = audio_collect(s, got, 8, 600);
    TEST_CHECK_(n == expected, "got %d packets, expected %d", n, expected);

    for (i = 0; i < n; i++)
    {
        TEST_CHECK_(got[i].seq == i + 1, "packet %d has sequence %u", i,
                    got[i].seq);
        TEST_CHECK_(got[i].loss.lost_packets == 0,
                    "packet %d reported %u lost", i, got[i].loss.lost_packets);
    }

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* How long the receive loop is allowed to sleep, against a clock the test
 * supplies -- the one part of the retransmit path whose timing can be pinned
 * down exactly, since it is a pure function of the reorder state.
 *
 * The 400 ms window the tests above use leaves the retry inside the loop's
 * 50 ms idle poll, so the poll alone would carry it. A short window does not:
 * there the wait has to shorten to the retry, or the loop sleeps past it and
 * the packet is given up when the window closes. That is the case hardware
 * loss run 3 found, and the case checked here. */
void test_session_audio_wait_follows_the_retry(void)
{
    struct stream_reorder *r = stream_reorder_new(60, 16, 64, 50, 16);
    const uint8_t payload[1] = { 0 };
    struct stream_reorder_item item;
    uint32_t missing[4];
    static const int64_t period = 30;   /* half of the 60 ms window */

    TEST_ASSERT(r != NULL);

    /* Nothing held: the loop sleeps its full poll interval. */
    TEST_CHECK_(icom_network_session_audio_wait_ms(r, 0, period) == 50,
                "idle wait was %d ms",
                icom_network_session_audio_wait_ms(r, 0, period));

    stream_reorder_push(r, 1, payload, sizeof(payload), 0);
    stream_reorder_push(r, 3, payload, sizeof(payload), 0);

    /* Release what is ready, the way the receive loop does, leaving only the
     * hole at 2 and the packet held behind it. */
    while (stream_reorder_pop(r, 0, &item) == STREAM_REORDER_PACKET) { }

    /* Sequence 2 is missing and has never been asked for: no sleeping. */
    TEST_CHECK_(icom_network_session_audio_wait_ms(r, 0, period) == 0,
                "wait before the first request was %d ms",
                icom_network_session_audio_wait_ms(r, 0, period));

    TEST_CHECK(stream_reorder_missing(r, 0, period, missing, 4) == 1);

    /* Asked for at 0, so the repeat falls due at 30 -- inside the poll
     * interval, and only sent on time if the wait follows it. */
    TEST_CHECK_(icom_network_session_audio_wait_ms(r, 0, period) == 30,
                "wait to the repeat was %d ms",
                icom_network_session_audio_wait_ms(r, 0, period));
    TEST_CHECK_(icom_network_session_audio_wait_ms(r, 20, period) == 10,
                "wait to the repeat at t=20 was %d ms",
                icom_network_session_audio_wait_ms(r, 20, period));

    /* Past the window nothing can be asked for again, and the packet is
     * released now, so the loop must not sleep on it. */
    TEST_CHECK_(icom_network_session_audio_wait_ms(r, 60, period) == 0,
                "wait at the deadline was %d ms",
                icom_network_session_audio_wait_ms(r, 60, period));

    stream_reorder_free(r);
}


/* A resend that is itself lost is asked for again, and the packet comes back
 * in order. Traffic keeps flowing after the hole here. */
void test_session_audio_retry_after_lost_resend(void)
{
    audio_retry_check(1);
}

/* The same with nothing arriving after the hole, so the repeat has to be
 * issued by the receive loop on its own rather than off an incoming packet. */
void test_session_audio_retry_without_traffic(void)
{
    audio_retry_check(0);
}


/* A window gives up on a packet that never comes, after the window, and
 * reports it once. */
void test_session_audio_window_gives_up(void)
{
    static const uint16_t script[] = { 1, 2, 4, 5 };
    struct mock_server mock;
    struct icom_network_session *s;
    struct audio_rx_item got[8];
    int n;

    mock_start(&mock);
    s = audio_session(&mock, 100);

    audio_script(&mock, script, 4);
    n = audio_collect(s, got, 8, 600);

    TEST_CHECK_(n == 4, "got %d payloads, expected 4", n);

    if (n == 4)
    {
        TEST_CHECK(got[2].seq == 4);
        TEST_CHECK_(got[2].loss.lost_packets == 1, "lost_packets=%u",
                    got[2].loss.lost_packets);
        TEST_CHECK(got[3].loss.lost_packets == 0);
    }

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A reader that falls behind loses the oldest payloads, and the loss is carried
 * to the first payload it does get, exactly in bytes. */
void test_session_audio_queue_overflow_reported(void)
{
    struct mock_server mock;
    struct icom_network_session *s;
    struct audio_rx_item got[128];
    uint16_t script[100];
    uint32_t overrun = 0;
    int n, i;

    mock_start(&mock);
    s = audio_session(&mock, 0);

    for (i = 0; i < 100; i++) { script[i] = (uint16_t)(i + 1); }

    audio_script(&mock, script, 100);
    usleep(500 * 1000);                 /* do not read while they arrive */
    n = audio_collect(s, got, 128, 300);

    for (i = 0; i < n; i++) { overrun += got[i].loss.overrun_bytes; }

    TEST_CHECK_(n < 100, "the queue cannot hold all 100 (got %d)", n);
    TEST_CHECK_(overrun == (uint32_t)(100 - n) * 16,
                "overrun_bytes=%u, expected %d x 16", overrun, 100 - n);

    if (n > 0)
    {
        TEST_CHECK(got[0].loss.overrun_bytes == overrun);
        TEST_CHECK_(got[0].seq == 100 - n + 1, "first kept is seq %d", got[0].seq);
    }

    icom_network_audio_stop(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* The radio asks for a packet it missed; the session replays it from the
 * transmit buffer. */
void test_session_retransmit_request(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    /* send a tracked CI-V frame, then ask for its sequence back */
    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));

    mock.retransmit_sequence = 1;   /* first tracked packet on the CI-V socket */
    mock.saw_retransmit_reply = 0;
    mock.ask_retransmit = 1;

    for (waited = 0; waited < 200 && !mock.saw_retransmit_reply; waited++)
    {
        hl_usleep(10000);
    }

    TEST_CHECK(mock.saw_retransmit_reply > 0);
    TEST_MSG("no replay of sequence %d after %d ms", mock.retransmit_sequence,
             waited * 10);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* The radio pings the client; the client must answer, or the radio concludes
 * the session is gone. Only the reply direction was exercised before. */
void test_session_ping_request(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    mock.saw_ping_reply = 0;
    mock.ask_ping = 1;

    for (waited = 0; waited < 200 && !mock.saw_ping_reply; waited++)
    {
        hl_usleep(10000);
    }

    TEST_CHECK(mock.saw_ping_reply);
    TEST_MSG("client did not answer the ping request within %d ms", waited * 10);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* Repeated connect/disconnect against one radio. The radio holds the CI-V and
 * audio streams open for its own timeout if the client disconnects untidily,
 * and then refuses a quick reconnect -- which is why disconnect() closes the
 * streams before stopping the threads that answer retransmit requests. */
void test_session_reconnect_cycles(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];
    int cycle;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");

    for (cycle = 0; cycle < 5; cycle++)
    {
        struct icom_network_session *s = icom_network_session_alloc(&config);

        TEST_ASSERT(s != NULL);
        TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
        TEST_MSG("cycle %d failed to connect", cycle);

        TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
        TEST_CHECK(icom_network_civ_recv(s, rx, sizeof(rx), 1000) > 0);
        TEST_MSG("cycle %d got no CI-V response", cycle);

        icom_network_session_disconnect(s);
        icom_network_session_free(s);
    }

    mock_stop(&mock);
}

/* Disconnecting with a command still in flight -- the response never read --
 * must not wedge the next session. */
void test_session_reconnect_after_dirty_close(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    uint8_t cmd[] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    uint8_t rx[256];

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);
    /* send, then walk away without reading the reply */
    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
    icom_network_session_disconnect(s);
    icom_network_session_free(s);

    /* the next session must be clean: the stale reply from the previous one is
     * drained at connect rather than mistaken for this session's answer */
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_CHECK(icom_network_session_connect(s) == RIG_OK);
    TEST_CHECK(icom_network_civ_send(s, cmd, sizeof(cmd)) == (int)sizeof(cmd));
    TEST_CHECK(icom_network_civ_recv(s, rx, sizeof(rx), 1000) > 0);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* Freeing a session that never connected must not touch uninitialised state.
 * The per-socket mutexes are created in session_alloc and destroyed in
 * session_free precisely so this path cannot destroy a mutex that was never
 * initialised -- creating them in socket_open would reintroduce that. */
void test_session_free_without_connect(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    icom_network_session_free(s);          /* never connected */

    /* and a session whose connect failed: nothing is listening on this port */
    config.control_port = 1;
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_CHECK(icom_network_session_connect(s) != RIG_OK);
    icom_network_session_free(s);

    mock_stop(&mock);
}

/* A radio that stops answering must be noticed, not pinged at forever. */
void test_session_liveness_timeout(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 1000;  /* the configurable floor */

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);
    TEST_CHECK(icom_network_session_is_valid(s));

    mock.go_silent = 1;

    for (waited = 0; waited < 300 && icom_network_session_is_valid(s); waited++)
    {
        hl_usleep(10000);
    }

    TEST_CHECK(!icom_network_session_is_valid(s));
    TEST_MSG("still valid after %d ms of silence", waited * 10);
    TEST_CHECK(icom_network_session_loss_reason(s) == RIG_COMM_REASON_LINK_TIMEOUT);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* A healthy session must never trip the liveness check. */
void test_session_liveness_no_false_positive(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int i;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    /* Comfortably above the 500 ms keepalive interval: a threshold at the ping
     * cadence would trip on ordinary jitter, which is why set_conf enforces a
     * floor. */
    config.liveness_timeout_ms = 2000;

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    /* several times the threshold, with the mock answering normally */
    for (i = 0; i < 60; i++)
    {
        hl_usleep(100000);
        TEST_ASSERT(icom_network_session_is_valid(s));
    }

    TEST_CHECK(icom_network_session_loss_reason(s) == RIG_COMM_REASON_NONE);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* The radio announcing a disconnect is reported as such, not as a timeout --
 * the distinction tells an operator whether someone else took the radio. */
void test_session_peer_disconnect(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    mock.announce_disconnect = 1;

    for (waited = 0; waited < 200 && icom_network_session_is_valid(s); waited++)
    {
        hl_usleep(10000);
    }

    TEST_CHECK(!icom_network_session_is_valid(s));
    TEST_CHECK(icom_network_session_loss_reason(s)
               == RIG_COMM_REASON_PEER_DISCONNECT);
    TEST_MSG("reason was %u", icom_network_session_loss_reason(s));

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* With the liveness check disabled the session is never lost on its own, which is
 * the pre-existing behaviour and must remain available. */
void test_session_liveness_disabled(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int i;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 0;    /* disabled */

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    mock.go_silent = 1;

    for (i = 0; i < 15; i++) { hl_usleep(100000); }

    TEST_CHECK(icom_network_session_is_valid(s));
    TEST_MSG("session lost despite the check being disabled");

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

/* Opt-in reconnect: the radio goes away and comes back, and the session
 * re-establishes itself without the caller doing anything. */
void test_session_auto_reconnect(void)
{
    struct mock_server mock;
    struct icom_network_session_config config;
    struct icom_network_session *s;
    int waited;

    mock_start(&mock);
    capability_config(&config, &mock, "IC-7610");
    config.liveness_timeout_ms = 1000;
    config.auto_reconnect = 1;

    s = icom_network_session_alloc(&config);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT(icom_network_session_connect(s) == RIG_OK);

    mock.go_silent = 1;

    for (waited = 0; waited < 300 && icom_network_session_is_valid(s); waited++)
    {
        hl_usleep(10000);
    }

    TEST_ASSERT(!icom_network_session_is_valid(s));

    /* radio comes back */
    mock.go_silent = 0;

    for (waited = 0; waited < 800 && !icom_network_session_is_valid(s); waited++)
    {
        hl_usleep(10000);
    }

    TEST_CHECK(icom_network_session_is_valid(s));
    TEST_MSG("not re-established after %d ms", waited * 10);

    icom_network_session_disconnect(s);
    icom_network_session_free(s);
    mock_stop(&mock);
}

TEST_LIST =
{
    { "handshake_and_civ_roundtrip", test_session_handshake_and_civ_roundtrip },
    { "stale_frame_drain",           test_session_stale_frame_drain },
    { "async_routing",               test_session_async_routing },
    { "audio_rx",                    test_session_audio_rx },
    { "failed_connect_releases_slot", test_session_failed_connect_releases_slot },
    { "handshake_recovers_lost_reply", test_session_handshake_recovers_lost_reply },
    { "handshake_recovers_lost_first_reply", test_session_handshake_recovers_lost_first_reply },
    { "audio_window0_gap",           test_session_audio_window0_gap },
    { "audio_window0_late_dropped",  test_session_audio_window0_late_dropped },
    { "audio_window_reorders",       test_session_audio_window_reorders },
    { "audio_window_retransmit_recovers", test_session_audio_window_retransmit_recovers },
    { "audio_window_gives_up",       test_session_audio_window_gives_up },
    { "audio_wait_follows_the_retry", test_session_audio_wait_follows_the_retry },
    { "audio_retry_after_lost_resend", test_session_audio_retry_after_lost_resend },
    { "audio_retry_without_traffic", test_session_audio_retry_without_traffic },
    { "audio_queue_overflow_reported", test_session_audio_queue_overflow_reported },
    { "capability_name_is_echoed",   test_session_capability_name_is_echoed },
    { "capability_select_by_name",   test_session_capability_select_by_name },
    { "capability_select_by_index",  test_session_capability_select_by_index },
    { "capability_no_match",         test_session_capability_no_match },
    { "capability_index_out_of_range", test_session_capability_index_out_of_range },
    { "capability_rate_rejected",    test_session_capability_rate_rejected },
    { "capability_tx_suppressed",    test_session_capability_tx_suppressed },
    { "sequence_resync",             test_session_sequence_resync },
    { "civ_reply_with_management_length", test_session_civ_reply_with_management_length },
    { "civ_idles_not_requested",     test_session_civ_idles_not_requested },
    { "civ_duplicate_dropped",       test_session_civ_duplicate_dropped },
    { "civ_silence_is_link_timeout", test_session_civ_silence_is_link_timeout },
    { "civ_port_refusing_is_socket_error", test_session_civ_port_refusing_is_socket_error },
    { "injected_socket_error_is_socket_error", test_session_injected_socket_error_is_socket_error },
    { "socket_error_wins_over_a_silent_link", test_session_socket_error_wins_over_a_silent_link },
    { "transient_socket_error_is_not_a_failure", test_session_transient_socket_error_is_not_a_failure },
    { "socket_errors_respect_liveness_disabled", test_session_socket_errors_respect_liveness_disabled },
    { "free_during_reconnect_handshake", test_session_free_during_reconnect_handshake },
    { "free_during_reconnect_backoff", test_session_free_during_reconnect_backoff },
    { "civ_send_during_reconnect",   test_session_civ_send_during_reconnect },
#ifdef __MINGW32__
    { "starts_winsock",              test_session_starts_winsock },
#endif
    { "retransmit_request",          test_session_retransmit_request },
    { "ping_request",                test_session_ping_request },
    { "reconnect_cycles",            test_session_reconnect_cycles },
    { "reconnect_after_dirty_close", test_session_reconnect_after_dirty_close },
    { "free_without_connect",        test_session_free_without_connect },
    { "liveness_timeout",            test_session_liveness_timeout },
    { "liveness_no_false_positive",  test_session_liveness_no_false_positive },
    { "liveness_disabled",           test_session_liveness_disabled },
    { "peer_disconnect",             test_session_peer_disconnect },
    { "auto_reconnect",              test_session_auto_reconnect },
    { NULL, NULL }
};
