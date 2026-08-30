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

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

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

    n = icom_network_audio_recv(s, rx, sizeof(rx), 1000);
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
    { "capability_name_is_echoed",   test_session_capability_name_is_echoed },
    { "capability_select_by_name",   test_session_capability_select_by_name },
    { "capability_select_by_index",  test_session_capability_select_by_index },
    { "capability_no_match",         test_session_capability_no_match },
    { "capability_index_out_of_range", test_session_capability_index_out_of_range },
    { "capability_rate_rejected",    test_session_capability_rate_rejected },
    { "capability_tx_suppressed",    test_session_capability_tx_suppressed },
    { "sequence_resync",             test_session_sequence_resync },
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
