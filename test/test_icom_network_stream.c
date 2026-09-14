/*
 *  Hamlib Icom network backend stream tests
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

/* Tests for the Hamlib-facing layer of the Icom network backend
 * (rigs/icom/icom_network.c), driven through the public rig_ API against the
 * in-process mock radio.
 *
 * This layer had no coverage at all while holding the stream_open error paths
 * where two buffer leaks lived, so the failure paths matter here as much as
 * the happy one. Run under -fsanitize=address to make the leak checks bite. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"
#include "icom_network_mock.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "hamlib/riglist.h"
#include "icom.h"
#include "icom_defs.h"

/* Open the IC-7610 network model against the mock radio on loopback. Returns
 * NULL if the open failed, having cleaned up. */
static RIG *open_against_mock(struct mock_server *m, const char *extra_conf)
{
    char port[16];
    RIG *rig = rig_init(RIG_MODEL_IC7610NET);

    if (rig == NULL) { return NULL; }

    SNPRINTF(port, sizeof(port), "%u", (unsigned)m->ctrl_port);
    rig_set_conf(rig, TOK_NET_USERNAME, "user");
    rig_set_conf(rig, TOK_NET_PASSWORD, "pass");
    rig_set_conf(rig, TOK_NET_CONTROL_PORT, port);

    if (extra_conf != NULL)
    {
        char copy[128];
        char *tok, *saveptr = NULL;
        SNPRINTF(copy, sizeof(copy), "%s", extra_conf);

        for (tok = strtok_r(copy, ",", &saveptr); tok != NULL;
                tok = strtok_r(NULL, ",", &saveptr))
        {
            char *eq = strchr(tok, '=');

            if (eq)
            {
                *eq = '\0';
                rig_set_conf(rig, rig_token_lookup(rig, tok), eq + 1);
            }
        }
    }

    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), "127.0.0.1");

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        return NULL;
    }

    return rig;
}

/* rig_stream_open takes a library-allocated config whose struct_size gates
 * compatibility, and the stream type travels inside it. */
static int stream_open(RIG *rig, rig_stream_type_t type,
                       rig_stream_format_t format, int rate, int channels,
                       rig_stream_t **stream)
{
    struct rig_stream_config *config = rig_stream_config_alloc();
    int ret;

    if (config == NULL) { return -RIG_ENOMEM; }

    config->type = type;
    config->format = format;
    config->sample_rate = rate;
    config->channels = channels;

    ret = rig_stream_open(rig, config, stream);
    rig_stream_config_free(config);   /* the stream keeps its own copy */

    return ret;
}

/* The whole point of the backend: open the rig over the network and get a
 * working CI-V path. */
void test_stream_rig_open_close(void)
{
    struct mock_server mock;
    RIG *rig;

    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    TEST_CHECK(rig->caps->rig_model == RIG_MODEL_IC7610NET);
    TEST_CHECK(rig->caps->port_type == RIG_PORT_CUSTOM);

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}

/* Open and close every advertised stream type. Exercises the buffer
 * allocation, codec setup and thread start/stop in stream_open/stream_close. */
void test_stream_open_close_each_type(void)
{
    static const rig_stream_type_t types[] =
    {
        RIG_STREAM_TYPE_AUDIO_RX, RIG_STREAM_TYPE_AUDIO_TX
    };
    struct mock_server mock;
    RIG *rig;
    size_t i;

    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        rig_stream_t *stream = NULL;
        int ret = stream_open(rig, types[i], RIG_STREAM_FORMAT_PCM_S16, 48000, 1,
                              &stream);
        TEST_CHECK(ret == RIG_OK);
        TEST_MSG("stream type %d open: %s", (int)types[i], rigerror(ret));

        if (ret == RIG_OK)
        {
            TEST_CHECK(stream != NULL);
            TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK);
        }
    }

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}

/* Opening the same stream twice, and closing twice, must not corrupt state or
 * leak the second allocation. */
void test_stream_reopen(void)
{
    struct mock_server mock;
    rig_stream_t *stream = NULL;
    RIG *rig;
    int cycle;

    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    for (cycle = 0; cycle < 3; cycle++)
    {
        TEST_CHECK(stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                               RIG_STREAM_FORMAT_PCM_S16, 48000, 1,
                               &stream) == RIG_OK);
        TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK);
        stream = NULL;
    }

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}

/* A configuration the backend cannot satisfy must be refused, and refusing it
 * must not leak the buffers allocated before the check. This is the path both
 * stream_open leaks lived on. */
void test_stream_open_rejects_bad_config(void)
{
    struct mock_server mock;
    rig_stream_t *stream = NULL;
    RIG *rig;

    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    /* a rate the radio never advertised, with resampling unavailable for I/Q */
    TEST_CHECK(stream_open(rig, RIG_STREAM_TYPE_IQ_RX,
                           RIG_STREAM_FORMAT_IQ_CS16, 96000, 1,
                           &stream) != RIG_OK);

    /* a channel count outside the advertised range */
    TEST_CHECK(stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                           RIG_STREAM_FORMAT_PCM_S16, 48000, 8,
                           &stream) != RIG_OK);

    /* the rig stays usable afterwards */
    TEST_CHECK(stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                           RIG_STREAM_FORMAT_PCM_S16, 48000, 1,
                           &stream) == RIG_OK);
    TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK);

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}

/* Closing the rig with a stream still open must tear the stream down rather
 * than leaving its thread running on freed state. */
void test_stream_left_open_at_rig_close(void)
{
    struct mock_server mock;
    rig_stream_t *stream = NULL;
    RIG *rig;

    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    TEST_CHECK(stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                           RIG_STREAM_FORMAT_PCM_S16, 48000, 1,
                           &stream) == RIG_OK);

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}

/* I/Q mode selects the stereo wire codec at connect; the resulting session
 * must still open an I/Q stream. */
void test_stream_iq_mode(void)
{
    struct mock_server mock;
    rig_stream_t *stream = NULL;
    RIG *rig;

    mock_start(&mock);
    rig = open_against_mock(&mock, "net_iq_mode=1");
    TEST_ASSERT(rig != NULL);

    TEST_CHECK(stream_open(rig, RIG_STREAM_TYPE_IQ_RX,
                           RIG_STREAM_FORMAT_IQ_CS16, 48000, 1,
                           &stream) == RIG_OK);

    if (stream) { TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK); }

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}

/* stream_open allocates its buffers before bringing the audio flow up, so a
 * failure there must release all of them. Nothing else exercises this path, so
 * a leak on it is otherwise invisible. Meaningful under -fsanitize=address;
 * without it this still proves the failure is reported rather than swallowed. */
void test_stream_open_fails_after_alloc(void)
{
    struct mock_server mock;
    rig_stream_t *stream = NULL;
    RIG *rig;
    int i;

    mock_start(&mock);
    mock.no_audio_port = 1;      /* audio_start will refuse */

    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    /* repeat: a single leak is easy to miss, a repeated one is not */
    for (i = 0; i < 5; i++)
    {
        int ret = stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                              RIG_STREAM_FORMAT_PCM_S16, 48000, 1, &stream);
        TEST_CHECK(ret != RIG_OK);
        TEST_MSG("attempt %d should have failed, got %s", i, rigerror(ret));
    }

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}

/* Find a backend's own declared entry, which is the native truth. This is not
 * rig_stream_caps_at(): that serves the wider effective set the frontend
 * derives, so it would report formats no Icom radio ever produces. */
/* The model declaration: what this radio can do across every configuration,
 * which is what dump_caps reports and what no session may ever rewrite. */
static const struct rig_stream_caps *native_entry(RIG *rig,
        rig_stream_type_t type)
{
    const struct rig_stream_caps *c = rig->caps->stream_caps;
    int i;

    for (i = 0; c != NULL && c[i].formats != 0; i++)
    {
        if (c[i].type == type)
        {
            return &c[i];
        }
    }

    return NULL;
}


/* The served view: what this session can actually open, which is where a
 * negotiated geometry shows up (in the native_* fields). */
static const struct rig_stream_caps *session_entry(RIG *rig,
        rig_stream_type_t type)
{
    int count = rig_stream_caps_count(rig);
    int i;

    for (i = 0; i < count; i++)
    {
        const struct rig_stream_caps *c = rig_stream_caps_at(rig, i);

        if (c != NULL && c->type == type)
        {
            return c;
        }
    }

    return NULL;
}


/* Every wire codec decodes to the same S16 pivot, and the I/Q payload is
 * already CS16, so those are the only formats this hardware has. Declaring
 * the float variants too would label a frontend conversion as native and
 * break require_native for anyone asking for it. */
void test_caps_declare_native_only(void)
{
    struct mock_server mock;
    RIG *rig;
    const struct rig_stream_caps *audio, *iq;

    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    /* Exactly the two the hardware reaches without help: the pivot every codec
     * decodes to, and the unsigned bytes the 8-bit LPCM codecs put on the wire
     * as they are. Nothing that would only be reachable through a frontend
     * conversion stage belongs here -- F32 above all. */
    audio = native_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->formats == (RIG_STREAM_FORMAT_PCM_S16
                                  | RIG_STREAM_FORMAT_PCM_U8));
    TEST_MSG("audio RX native formats = 0x%x", (unsigned)audio->formats);

    iq = native_entry(rig, RIG_STREAM_TYPE_IQ_RX);

    if (iq != NULL)
    {
        TEST_CHECK(iq->formats == RIG_STREAM_FORMAT_IQ_CS16);
        TEST_MSG("I/Q native formats = 0x%x", (unsigned)iq->formats);
    }

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* A session fixes one audio rate at connect and changing it means
 * reconnecting, so once open that is the only rate the backend can serve.
 * Leaving the whole hardware list advertised would let the frontend resolve
 * to a native rate no stream could be opened at. */
void test_audio_rate_pinned_to_session(void)
{
    struct mock_server mock;
    RIG *rig;
    const struct rig_stream_caps *audio;

    mock_start(&mock);
    rig = open_against_mock(&mock, "net_sample_rate=2");
    TEST_ASSERT(rig != NULL);

    audio = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);

    TEST_CHECK(audio->native_sample_rates[0] > 0);
    TEST_CHECK(audio->native_sample_rates[1] == 0);
    TEST_MSG("expected one pinned rate, got %d then %d",
             audio->native_sample_rates[0], audio->native_sample_rates[1]);

    /* The model declaration keeps every rate the radio can be configured
     * for: it is shared by every rig of this model and describes the
     * hardware, not this connection. */
    audio = native_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->sample_rates[1] != 0);
    TEST_MSG("the model declaration was narrowed to a single rate");

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* The negotiated codec fixes the channel count as firmly as it fixes the
 * rate, and getting this wrong is silent: handing stereo to a mono codec is
 * refused nowhere, the wire bytes are just read as twice as many mono
 * samples and the radio transmits at double speed. */
void test_audio_channels_pinned_to_codec(void)
{
    struct mock_server mock;
    RIG *rig;
    const struct rig_stream_caps *audio;

    /* No token: the radio defaults to mono LPCM16. */
    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    audio = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->native_channels[0] == 1 && audio->native_channels[1] == 0);
    TEST_MSG("mono codec, got %d then %d", audio->native_channels[0],
             audio->native_channels[1]);

    /* I/Q rides the two slots of a stereo codec as one complex channel, so a
     * mono session cannot carry it. Advertising it anyway would let an
     * application open a stream that hands it mono audio to read as I/Q. */
    TEST_CHECK(session_entry(rig, RIG_STREAM_TYPE_IQ_RX) == NULL);
    TEST_MSG("a mono session must not offer I/Q");

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);

    /* net_rx_codec=3 is the stereo LPCM16, which really does make 2 native. */
    mock_start(&mock);
    rig = open_against_mock(&mock, "net_rx_codec=3");
    TEST_ASSERT(rig != NULL);

    audio = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->native_channels[0] == 2 && audio->native_channels[1] == 0);
    TEST_MSG("stereo codec, got %d then %d", audio->native_channels[0],
             audio->native_channels[1]);

    /* Now the two slots exist, so I/Q is carryable -- as a single complex
     * channel, never as two. */
    audio = session_entry(rig, RIG_STREAM_TYPE_IQ_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->native_channels[0] == 1 && audio->native_channels[1] == 0);
    TEST_MSG("I/Q is one complex channel, got %d then %d",
             audio->native_channels[0], audio->native_channels[1]);

    /* Receive and transmit negotiate separate codecs, so the transmit entry
     * follows its own: every transmit codec the protocol offers is mono, and
     * transmit is the direction where a stereo stream reaching a mono codec
     * puts the radio on the air at double speed. */
    audio = session_entry(rig, RIG_STREAM_TYPE_AUDIO_TX);

    if (audio != NULL)
    {
        TEST_CHECK(audio->native_channels[0] == 1
                   && audio->native_channels[1] == 0);
        TEST_MSG("transmit codec is mono, got %d then %d",
                 audio->native_channels[0], audio->native_channels[1]);
    }

    /* Through all of that the model declaration still offers both counts. */
    audio = native_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->channels[0] == 1 && audio->channels[1] == 2);
    TEST_MSG("the model declaration was narrowed to the session's channels");

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* The backend serves only its native configuration, so a request outside it
 * has to be met by the frontend: it must still open, and say which stages are
 * carrying it. */
void test_non_native_request_is_converted(void)
{
    struct mock_server mock;
    RIG *rig;
    rig_stream_t *stream = NULL;
    int rate;

    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);
    TEST_ASSERT(rig != NULL);

    /* The session's rate, not the model's first: opening at a rate this
     * connection did not negotiate needs a resampler that is not in every
     * build. */
    rate = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX)->native_sample_rates[0];

    if (stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                    RIG_STREAM_FORMAT_PCM_F32, rate, 1, &stream) == RIG_OK)
    {
        TEST_CHECK((rig_stream_get_conversions(stream)
                    & RIG_STREAM_CONV_FORMAT) != 0);
        TEST_MSG("F32 is not native here, so a format stage must be active");
        rig_stream_close(rig, stream);
    }
    else
    {
        TEST_CHECK(0);
        TEST_MSG("a convertible request must still open");
    }

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* The 8-bit LPCM codecs carry UNSIGNED samples, unlike the signed 16-bit ones,
 * so they are the wire's PCM_U8. An application that asks for S16 over one of
 * them still gets S16 -- the frontend widens it -- and the byte count is the
 * tell that the widening happened at all: one 8-bit wire sample must arrive as
 * two. Reading the wire as S16 instead would be silent, the bytes accepted and
 * the counters clean, and what came out would be noise at half the rate. */
void test_linear8_is_widened_to_the_pivot(void)
{
    struct mock_server mock;
    RIG *rig;
    rig_stream_t *stream = NULL;
    uint8_t buf[256];
    size_t got = 0;
    int rate;
    int tries;

    /* net_rx_codec=1 is LPCM 1ch 8-bit. */
    mock_start(&mock);
    rig = open_against_mock(&mock, "net_rx_codec=1");
    TEST_ASSERT(rig != NULL);

    rate = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX)->native_sample_rates[0];

    TEST_ASSERT(stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                            RIG_STREAM_FORMAT_PCM_S16, rate, 1,
                            &stream) == RIG_OK);

    /* The mock emits one audio packet of mock_audio when the stream goes
     * live; give the receive thread a moment to decode it. */
    for (tries = 0; tries < 50 && got == 0; tries++)
    {
        rig_stream_read(rig, stream, buf, sizeof(buf), &got, 100, NULL);
    }

    TEST_CHECK(got == sizeof(mock_audio) * 2);
    TEST_MSG("expected %zu pivot bytes from %zu 8-bit wire bytes, got %zu",
             sizeof(mock_audio) * 2, sizeof(mock_audio), got);

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* Choosing an 8-bit codec halves the network traffic, and its wire samples are
 * unsigned bytes -- PCM_U8 exactly. So a session that negotiated one can serve
 * a U8 stream with no conversion anywhere: the bytes an application reads are
 * the bytes that crossed the network. Widening them to the pivot only for the
 * frontend to narrow them back is lossless but pure waste, and it would also
 * misreport the stream as converted. */
void test_linear8_serves_u8_natively(void)
{
    struct mock_server mock;
    RIG *rig;
    rig_stream_t *stream = NULL;
    const struct rig_stream_caps *audio;
    uint8_t buf[256];
    size_t got = 0;
    int rate;
    int tries;

    mock_start(&mock);
    rig = open_against_mock(&mock, "net_rx_codec=1");   /* LPCM 1ch 8bit */
    TEST_ASSERT(rig != NULL);

    audio = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->native_formats == RIG_STREAM_FORMAT_PCM_U8);
    TEST_MSG("an 8-bit LPCM session serves U8 and nothing else natively, "
             "native=0x%x", (unsigned)audio->native_formats);
    /* S16 stays openable -- the frontend widens the pivot for anyone who wants
     * it -- but the session must not claim to serve it for free. */
    TEST_CHECK((audio->formats & RIG_STREAM_FORMAT_PCM_S16) != 0);
    TEST_MSG("S16 must still be reachable through conversion");

    rate = audio->native_sample_rates[0];

    TEST_ASSERT(stream_open(rig, RIG_STREAM_TYPE_AUDIO_RX,
                            RIG_STREAM_FORMAT_PCM_U8, rate, 1,
                            &stream) == RIG_OK);

    TEST_CHECK(rig_stream_get_conversions(stream) == RIG_STREAM_CONV_NONE);
    TEST_MSG("U8 over an 8-bit codec must need no conversion stage, got 0x%x",
             rig_stream_get_conversions(stream));

    for (tries = 0; tries < 50 && got == 0; tries++)
    {
        rig_stream_read(rig, stream, buf, sizeof(buf), &got, 100, NULL);
    }

    /* One wire byte per sample, handed over as it arrived -- half of what the
     * same payload becomes once widened to the pivot. */
    TEST_CHECK(got == sizeof(mock_audio));
    TEST_MSG("expected the %zu wire bytes verbatim, got %zu",
             sizeof(mock_audio), got);

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* uLaw is one byte on the wire too, and worth choosing for the same reason,
 * but it is companded rather than linear and Hamlib has no uLaw sample format.
 * It therefore has to reach the application through the pivot, and a session
 * that negotiated it must not claim otherwise. */
void test_mulaw_is_served_through_the_pivot(void)
{
    struct mock_server mock;
    RIG *rig;
    const struct rig_stream_caps *audio;

    mock_start(&mock);
    rig = open_against_mock(&mock, "net_rx_codec=2");   /* uLaw 1ch 8bit */
    TEST_ASSERT(rig != NULL);

    audio = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK((audio->native_formats & RIG_STREAM_FORMAT_PCM_S16) != 0);
    TEST_MSG("uLaw still reaches the application, through the pivot");
    TEST_CHECK((audio->native_formats & RIG_STREAM_FORMAT_PCM_U8) == 0);
    TEST_MSG("uLaw bytes are companded, so they are not natively PCM_U8");

    /* It stays openable as U8 -- the frontend can convert the pivot to it.
     * What must not happen is the session claiming that costs nothing. */
    TEST_CHECK((audio->formats & RIG_STREAM_FORMAT_PCM_U8) != 0);
    TEST_MSG("U8 is still reachable through conversion");

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);

    /* A 16-bit codec is not U8 either, for the plainer reason. */
    mock_start(&mock);
    rig = open_against_mock(&mock, NULL);               /* LPCM 1ch 16bit */
    TEST_ASSERT(rig != NULL);

    audio = session_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK((audio->native_formats & RIG_STREAM_FORMAT_PCM_U8) == 0);
    TEST_MSG("a 16-bit codec has no 8-bit samples to hand over");

    /* The model declaration keeps both, because the model reaches both --
     * which one this connection serves is what the session view narrows. */
    audio = native_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK((audio->formats & RIG_STREAM_FORMAT_PCM_U8) != 0
               && (audio->formats & RIG_STREAM_FORMAT_PCM_S16) != 0);
    TEST_MSG("the model declaration was narrowed to the session's formats");

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* I/Q is IQ_CS16: {I,Q} pairs of signed 16-bit samples. Three codecs carry two
 * wire slots, but only LPCM16S carries them as signed 16-bit -- uLaw-stereo is
 * companded and LPCM8S is unsigned bytes. Offering I/Q over either of those
 * would hand an application bytes it reads as complex samples and hears as
 * noise, with every counter clean, so a session must not advertise it. */
void test_iq_needs_the_16bit_stereo_codec(void)
{
    struct mock_server mock;
    RIG *rig;

    /* net_rx_codec=3 is LPCM 2ch 16bit -- the one that really carries I/Q. */
    mock_start(&mock);
    rig = open_against_mock(&mock, "net_rx_codec=3");
    TEST_ASSERT(rig != NULL);
    TEST_CHECK(session_entry(rig, RIG_STREAM_TYPE_IQ_RX) != NULL);
    TEST_MSG("the 16-bit stereo codec must offer I/Q");
    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);

    /* net_rx_codec=4 is uLaw 2ch: two slots, but companded bytes. */
    mock_start(&mock);
    rig = open_against_mock(&mock, "net_rx_codec=4");
    TEST_ASSERT(rig != NULL);
    TEST_CHECK(session_entry(rig, RIG_STREAM_TYPE_IQ_RX) == NULL);
    TEST_MSG("uLaw is companded, so its two slots are not {I,Q} samples");
    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);

    /* net_rx_codec=5 is LPCM 2ch 8bit: two slots, but unsigned bytes. */
    mock_start(&mock);
    rig = open_against_mock(&mock, "net_rx_codec=5");
    TEST_ASSERT(rig != NULL);
    TEST_CHECK(session_entry(rig, RIG_STREAM_TYPE_IQ_RX) == NULL);
    TEST_MSG("8-bit samples are not IQ_CS16");
    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


TEST_LIST =
{
    { "rig_open_close",            test_stream_rig_open_close },
    { "open_close_each_type",      test_stream_open_close_each_type },
    { "reopen",                    test_stream_reopen },
    { "open_rejects_bad_config",   test_stream_open_rejects_bad_config },
    { "left_open_at_rig_close",    test_stream_left_open_at_rig_close },
    { "iq_mode",                   test_stream_iq_mode },
    { "open_fails_after_alloc",    test_stream_open_fails_after_alloc },
    { "caps_declare_native_only",  test_caps_declare_native_only },
    { "audio_rate_pinned_to_session", test_audio_rate_pinned_to_session },
    { "audio_channels_pinned_to_codec", test_audio_channels_pinned_to_codec },
    { "non_native_request_is_converted", test_non_native_request_is_converted },
    { "linear8_is_widened_to_the_pivot", test_linear8_is_widened_to_the_pivot },
    { "linear8_serves_u8_natively", test_linear8_serves_u8_natively },
    { "iq_needs_the_16bit_stereo_codec", test_iq_needs_the_16bit_stereo_codec },
    { "mulaw_is_served_through_the_pivot", test_mulaw_is_served_through_the_pivot },
    { NULL, NULL }
};
