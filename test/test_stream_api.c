/*
 *  Hamlib streaming API tests
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

/* Frontend API unit tests for the Hamlib streaming subsystem. */
/* Uses a minimal stub backend for stream open/close and loopback. */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"
#include "stream.h"
#include "cache.h"
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <hamlib/rig.h>
#include "hamlib/rig_state.h"
#include "token.h"

/*
 * Stub backend: minimal rig_caps with stream capabilities.
 * stream_open just returns RIG_OK (ring buffer is pre-created by frontend).
 * No actual I/O thread — tests write/read the ring buffer directly.
 */

static int stub_stream_open(RIG *rig, struct rig_stream *stream)
{
    (void)rig;
    (void)stream;
    return RIG_OK;
}

static int stub_stream_close(RIG *rig, struct rig_stream *stream)
{
    (void)rig;
    (void)stream;
    return RIG_OK;
}

static int stub_rig_open(RIG *rig)
{
    (void)rig;
    return RIG_OK;
}

static int stub_rig_close(RIG *rig)
{
    (void)rig;
    return RIG_OK;
}

static int stub_set_freq_calls;
static freq_t stub_set_freq_last;

static int stub_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    (void)rig;
    (void)vfo;
    stub_set_freq_calls++;
    stub_set_freq_last = freq;
    return RIG_OK;
}

static int stub_rig_init(RIG *rig)
{
    (void)rig;
    return RIG_OK;
}

static int stub_rig_cleanup(RIG *rig)
{
    (void)rig;
    return RIG_OK;
}

/* Caps with streaming support */
static const struct rig_stream_caps stub_stream_caps[] =
{
    {
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .formats = RIG_STREAM_FORMAT_PCM_S16 | RIG_STREAM_FORMAT_PCM_F32,
        .sample_rates = { 8000, 48000, 0 },
        .channels = { 1, 2, 0 },
        .max_streams = 2,
    },
    {
        .type = RIG_STREAM_TYPE_IQ_RX,
        .formats = RIG_STREAM_FORMAT_IQ_CS16 | RIG_STREAM_FORMAT_IQ_CF32,
        .sample_rates = { 48000, 192000, 0 },
        .channels = { 1, 0 },
        .max_streams = 1,
    },
    { 0 }  /* Terminator */
};

/* Mono-only audio and 2-channel coherent I/Q, for the channel-derivation
 * rules: audio widens to stereo (upmix), I/Q never fabricates channels. */
static const struct rig_stream_caps stub_mono_stream_caps[] =
{
    {
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .formats = RIG_STREAM_FORMAT_PCM_F32,
        .sample_rates = { 48000, 0 },
        .channels = { 1, 0 },
        .max_streams = 1,
    },
    {
        .type = RIG_STREAM_TYPE_IQ_RX,
        .formats = RIG_STREAM_FORMAT_IQ_CF32,
        .sample_rates = { 48000, 0 },
        .channels = { 2, 0 },
        .max_streams = 1,
    },
    { 0 }  /* Terminator */
};

static struct rig_caps stub_caps_with_stream =
{
    .rig_model = 1,
    .model_name = "Stub Stream",
    .mfg_name = "Test",
    .version = "1.0",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 1000,
    .retry = 0,
    .stream_caps = stub_stream_caps,
    .rig_init = stub_rig_init,
    .rig_cleanup = stub_rig_cleanup,
    .rig_open = stub_rig_open,
    .rig_close = stub_rig_close,
    .set_freq = stub_set_freq,
    .stream_open = stub_stream_open,
    .stream_close = stub_stream_close,
};

/* A backend stream_read that stays "in flight" long enough to race a close. */
static volatile int slow_read_in_progress;
static volatile int slow_read_done;

static int stub_stream_read_slow(RIG *rig, struct rig_stream *stream,
                                 void *buffer, size_t buffer_size,
                                 size_t *bytes_read, int timeout_ms,
                                 struct rig_stream_read_info *info)
{
    (void)rig;
    (void)stream;
    (void)buffer;
    (void)buffer_size;
    (void)timeout_ms;
    (void)info;
    slow_read_in_progress = 1;
    usleep(100 * 1000);
    *bytes_read = 0;
    slow_read_done = 1;
    return RIG_OK;
}

static struct rig_caps stub_caps_slow_read =
{
    .rig_model = 3,
    .model_name = "Stub Slow Read",
    .mfg_name = "Test",
    .version = "1.0",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 1000,
    .retry = 0,
    .stream_caps = stub_stream_caps,
    .rig_init = stub_rig_init,
    .rig_cleanup = stub_rig_cleanup,
    .rig_open = stub_rig_open,
    .rig_close = stub_rig_close,
    .stream_open = stub_stream_open,
    .stream_close = stub_stream_close,
    .stream_read = stub_stream_read_slow,
};

/* Caps without streaming support */
static struct rig_caps stub_caps_no_stream =
{
    .rig_model = 2,
    .model_name = "Stub No Stream",
    .mfg_name = "Test",
    .version = "1.0",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 1000,
    .retry = 0,
    .rig_init = stub_rig_init,
    .rig_cleanup = stub_rig_cleanup,
    .rig_open = stub_rig_open,
    .rig_close = stub_rig_close,
    .set_freq = stub_set_freq,
};

static struct rig_caps stub_caps_mono_stream =
{
    .rig_model = 4,
    .model_name = "Stub Mono Stream",
    .mfg_name = "Test",
    .version = "1.0",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 1000,
    .retry = 0,
    .stream_caps = stub_mono_stream_caps,
    .rig_init = stub_rig_init,
    .rig_cleanup = stub_rig_cleanup,
    .rig_open = stub_rig_open,
    .rig_close = stub_rig_close,
    .stream_open = stub_stream_open,
    .stream_close = stub_stream_close,
};

/* A non-contiguous native channel list: coherent I/Q hardware that opens
 * 1 or 4 channels but nothing in between. The list is exact — the
 * frontend never fills the gap, so 2 and 3 are simply not openable. */
static const struct rig_stream_caps stub_gap_channel_stream_caps[] =
{
    {
        .type = RIG_STREAM_TYPE_IQ_RX,
        .formats = RIG_STREAM_FORMAT_IQ_CF32,
        .sample_rates = { 48000, 0 },
        .channels = { 1, 4, 0 },
        .max_streams = 1,
    },
    { 0 }  /* Terminator */
};

static struct rig_caps stub_caps_gap_channel_stream =
{
    .rig_model = 6,
    .model_name = "Stub Gap Channels",
    .mfg_name = "Test",
    .version = "1.0",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 1000,
    .retry = 0,
    .stream_caps = stub_gap_channel_stream_caps,
    .rig_init = stub_rig_init,
    .rig_cleanup = stub_rig_cleanup,
    .rig_open = stub_rig_open,
    .rig_close = stub_rig_close,
    .stream_open = stub_stream_open,
    .stream_close = stub_stream_close,
};

/* Codec (OPUS) alongside raw PCM, plus a codec-only entry. Codec formats
 * are opaque packet streams: native-only at open, and a codec-only entry
 * is served verbatim by derivation (no widening). */
static const struct rig_stream_caps stub_codec_stream_caps[] =
{
    {
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .formats = RIG_STREAM_FORMAT_PCM_S16 | RIG_STREAM_FORMAT_OPUS,
        .sample_rates = { 48000, 0 },
        .channels = { 1, 0 },
        .max_streams = 2,
    },
    {
        .type = RIG_STREAM_TYPE_AUDIO_TX,
        .formats = RIG_STREAM_FORMAT_OPUS,   /* codec-only entry */
        .sample_rates = { 48000, 0 },
        .channels = { 1, 0 },
        .max_streams = 1,
    },
    { 0 }  /* Terminator */
};

static struct rig_caps stub_caps_codec_stream =
{
    .rig_model = 5,
    .model_name = "Stub Codec Stream",
    .mfg_name = "Test",
    .version = "1.0",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 1000,
    .retry = 0,
    .stream_caps = stub_codec_stream_caps,
    .rig_init = stub_rig_init,
    .rig_cleanup = stub_rig_cleanup,
    .rig_open = stub_rig_open,
    .rig_close = stub_rig_close,
    .stream_open = stub_stream_open,
    .stream_close = stub_stream_close,
};


/* Helper: set up a RIG with the given caps, call rig_open-equivalent init. */
static RIG *setup_rig(struct rig_caps *caps)
{
    /* Use rig_init_new for proper allocation matching the caps.
     * Since our stub uses RIG_PORT_NONE, rig_open should succeed. */
    rig_register(caps);
    RIG *rig = rig_init(caps->rig_model);

    if (!rig)
    {
        return NULL;
    }

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        return NULL;
    }

    return rig;
}

static void teardown_rig(RIG *rig)
{
    if (rig)
    {
        rig_close(rig);
        rig_cleanup(rig);
    }
}


/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/* True if rate appears in a 0-terminated caps rate list. */
static int rate_in_list(const int *rates, int rate)
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

void test_get_stream_caps_none(void)
{
    RIG *rig = setup_rig(&stub_caps_no_stream);
    TEST_ASSERT(rig != NULL);

    int count = rig_stream_caps_count(rig);
    TEST_CHECK(count == 0);
    TEST_MSG("Expected 0 caps, got %d", count);
    TEST_CHECK(rig_stream_caps_at(rig, 0) == NULL);

    teardown_rig(rig);
}

void test_get_stream_caps(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    int count = rig_stream_caps_count(rig);
    TEST_CHECK(count == 2);
    TEST_MSG("Expected 2 caps, got %d", count);

    const struct rig_stream_caps *c0 = rig_stream_caps_at(rig, 0);
    const struct rig_stream_caps *c1 = rig_stream_caps_at(rig, 1);
    TEST_ASSERT(c0 != NULL && c1 != NULL);

    TEST_CHECK(c0->type == RIG_STREAM_TYPE_AUDIO_RX);
    TEST_CHECK(c0->formats & RIG_STREAM_FORMAT_PCM_S16);
    /* The served list is the effective set (sorted, possibly widened by
     * the resampler rules); the declared rates are in it, and the native
     * view preserves the declaration verbatim. */
    TEST_CHECK(rate_in_list(c0->sample_rates, 8000));
    TEST_CHECK(rate_in_list(c0->sample_rates, 48000));
    TEST_CHECK(c0->native_sample_rates[0] == 8000);
    TEST_CHECK(c0->native_sample_rates[1] == 48000);
    TEST_CHECK(c0->native_sample_rates[2] == 0);

    TEST_CHECK(c1->type == RIG_STREAM_TYPE_IQ_RX);
    TEST_CHECK(c1->formats & RIG_STREAM_FORMAT_IQ_CS16);

    /* Out-of-range indices return NULL (the borrow accessor's bound). */
    TEST_CHECK(rig_stream_caps_at(rig, 2) == NULL);
    TEST_CHECK(rig_stream_caps_at(rig, -1) == NULL);

    teardown_rig(rig);
}

/* The app-visible caps are the derived view: classic fields widened to the
 * effective set, native_* preserving the backend's declaration. Rules per
 * HAMLIB_STREAMING_FORMAT_CONVERSION.md section 3.4. */
void test_caps_derived_formats(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    const struct rig_stream_caps *audio = rig_stream_caps_at(rig, 0);
    const struct rig_stream_caps *iq = rig_stream_caps_at(rig, 1);
    TEST_ASSERT(audio != NULL && iq != NULL);

    /* Native view == declaration, exactly. */
    TEST_CHECK(audio->native_formats == (RIG_STREAM_FORMAT_PCM_S16
                                         | RIG_STREAM_FORMAT_PCM_F32));
    TEST_CHECK(iq->native_formats == (RIG_STREAM_FORMAT_IQ_CS16
                                      | RIG_STREAM_FORMAT_IQ_CF32));

    /* Effective view: the whole family is reachable... */
    TEST_CHECK(audio->formats & RIG_STREAM_FORMAT_PCM_S8);
    TEST_CHECK(audio->formats & RIG_STREAM_FORMAT_PCM_U8);
    TEST_CHECK(iq->formats & RIG_STREAM_FORMAT_IQ_CS8);
    TEST_CHECK(iq->formats & RIG_STREAM_FORMAT_IQ_CU8);

    /* ...but families do not leak into each other, and codec formats are
     * not fabricated. */
    TEST_CHECK(!(audio->formats & RIG_STREAM_FORMAT_IQ_CF32));
    TEST_CHECK(!(audio->formats & RIG_STREAM_FORMAT_OPUS));
    TEST_CHECK(!(iq->formats & RIG_STREAM_FORMAT_PCM_S16));

    teardown_rig(rig);
}

void test_caps_derived_rates(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    const struct rig_stream_caps *audio = rig_stream_caps_at(rig, 0);
    const struct rig_stream_caps *iq = rig_stream_caps_at(rig, 1);
    TEST_ASSERT(audio != NULL && iq != NULL);

    /* Native rates always present in the effective list. */
    TEST_CHECK(rate_in_list(audio->sample_rates, 8000));
    TEST_CHECK(rate_in_list(audio->sample_rates, 48000));
    TEST_CHECK(rate_in_list(iq->sample_rates, 192000));

#ifdef HAVE_SAMPLERATE
    /* Curated standards bounded by the largest native rate. */
    TEST_CHECK(rate_in_list(audio->sample_rates, 44100));
    TEST_CHECK(rate_in_list(audio->sample_rates, 24000));
    /* Integer divisions of native rates (exact results only):
     * 48000/5 and 48000/10 are not curated standards. */
    TEST_CHECK(rate_in_list(audio->sample_rates, 9600));
    TEST_CHECK(rate_in_list(audio->sample_rates, 4800));
    /* Audio is bounded by the largest native rate. */
    TEST_CHECK(!rate_in_list(audio->sample_rates, 96000));

    /* I/Q: downward only — nothing above native, curated strictly below
     * the largest native rate, integer divisions included. */
    TEST_CHECK(rate_in_list(iq->sample_rates, 96000));   /* curated, /2 */
    TEST_CHECK(rate_in_list(iq->sample_rates, 38400));   /* 192000/5 */
    TEST_CHECK(rate_in_list(iq->sample_rates, 19200));   /* 192000/10 */

    /* Sorted ascending. */
    for (int i = 1; i < HAMLIB_MAX_STREAM_RATES
            && audio->sample_rates[i] != 0; i++)
    {
        TEST_CHECK(audio->sample_rates[i] > audio->sample_rates[i - 1]);
    }

#else
    /* Without the resampler the effective rates equal the native rates. */
    TEST_CHECK(audio->sample_rates[0] == 8000);
    TEST_CHECK(audio->sample_rates[1] == 48000);
    TEST_CHECK(audio->sample_rates[2] == 0);
    TEST_CHECK(iq->sample_rates[0] == 48000);
    TEST_CHECK(iq->sample_rates[1] == 192000);
    TEST_CHECK(iq->sample_rates[2] == 0);
#endif

    teardown_rig(rig);
}

void test_caps_derived_channels(void)
{
    RIG *rig = setup_rig(&stub_caps_mono_stream);
    TEST_ASSERT(rig != NULL);

    const struct rig_stream_caps *audio = rig_stream_caps_at(rig, 0);
    const struct rig_stream_caps *iq = rig_stream_caps_at(rig, 1);
    TEST_ASSERT(audio != NULL && iq != NULL);

    /* Mono-only audio hardware: effective gains stereo via upmix... */
    TEST_CHECK(audio->native_channels[0] == 1
               && audio->native_channels[1] == 0);
    TEST_CHECK(audio->channels[0] == 1 && audio->channels[1] == 2
               && audio->channels[2] == 0);

    /* ...but the I/Q list is exact: coherent channels never widen, so a
     * 2-channel-only declaration serves 2 channels and nothing else. */
    TEST_CHECK(iq->native_channels[0] == 2 && iq->native_channels[1] == 0);
    TEST_CHECK(iq->channels[0] == 2 && iq->channels[1] == 0);

    teardown_rig(rig);
}

/* A native channel list need not be contiguous, and it is exact: the
 * effective list equals the declared list (no audio count present, so
 * no mono<->stereo widening either), a listed count opens native, and
 * every unlisted count — inside the gap or above the maximum — is
 * refused. */
void test_channel_list_exact(void)
{
    RIG *rig = setup_rig(&stub_caps_gap_channel_stream);
    TEST_ASSERT(rig != NULL);

    const struct rig_stream_caps *iq = rig_stream_caps_at(rig, 0);
    TEST_ASSERT(iq != NULL);

    /* Native {1,4} preserved and served verbatim as the effective set. */
    TEST_CHECK(iq->native_channels[0] == 1 && iq->native_channels[1] == 4
               && iq->native_channels[2] == 0);
    TEST_CHECK(iq->channels[0] == 1 && iq->channels[1] == 4
               && iq->channels[2] == 0);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_IQ_RX;
    cfg->format = RIG_STREAM_FORMAT_IQ_CF32;
    cfg->sample_rate = 48000;

    /* Listed counts open native. */
    rig_stream_t *stream = NULL;
    cfg->channels = 4;
    int ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("native 4-channel open returned %d", ret);

    if (ret == RIG_OK)
    {
        TEST_CHECK(rig_stream_get_conversions(stream)
                   == RIG_STREAM_CONV_NONE);
        rig_stream_close(rig, stream);
    }

    /* Unlisted counts are refused — in the gap and above the maximum. */
    for (int want = 2; want <= 3; want++)
    {
        cfg->channels = want;
        stream = NULL;
        ret = rig_stream_open(rig, cfg, &stream);
        TEST_CHECK(ret == -RIG_EINVAL);
        TEST_MSG("%d-channel open: got %d, expected %d",
                 want, ret, -RIG_EINVAL);
    }

    cfg->channels = 5;
    stream = NULL;
    ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == -RIG_EINVAL);
    TEST_MSG("5-channel open: got %d, expected %d", ret, -RIG_EINVAL);

    /* Listed counts still open under require_native. */
    cfg->channels = 1;
    cfg->require_native = 1;
    stream = NULL;
    ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("require_native native-count open returned %d", ret);

    if (ret == RIG_OK)
    {
        TEST_CHECK(rig_stream_get_conversions(stream)
                   == RIG_STREAM_CONV_NONE);
        rig_stream_close(rig, stream);
    }

    rig_stream_config_free(cfg);
    teardown_rig(rig);
}

/* Codec formats are opaque packet streams: a codec open must match the
 * native declaration exactly (no conversion stage applies) and always
 * reports a native stream, while raw formats in the same entry keep the
 * normal conversion rules. Applies to any format outside the raw PCM/I/Q
 * family masks, present or future. */
void test_codec_format_native_only(void)
{
    RIG *rig = setup_rig(&stub_caps_codec_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    rig_stream_t *stream = NULL;

    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_OPUS;
    cfg->sample_rate = 48000;
    cfg->channels = 1;

    int ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("native codec open returned %d", ret);

    if (ret == RIG_OK)
    {
        TEST_CHECK(rig_stream_get_conversions(stream)
                   == RIG_STREAM_CONV_NONE);
        rig_stream_close(rig, stream);
    }

    /* Non-native rate: clean -RIG_EINVAL even where a raw format would
     * be resampled (24000 divides 48000 exactly). */
    cfg->sample_rate = 24000;
    stream = NULL;
    ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == -RIG_EINVAL);
    TEST_MSG("codec at non-native rate: got %d, expected %d",
             ret, -RIG_EINVAL);

    /* Non-native channel count: the audio mono<->stereo map does not
     * apply to packets. */
    cfg->sample_rate = 48000;
    cfg->channels = 2;
    ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == -RIG_EINVAL);
    TEST_MSG("codec at non-native channels: got %d, expected %d",
             ret, -RIG_EINVAL);

    /* The raw family in the same entry still converts: stereo S16 is
     * served through the mono->stereo channel map. */
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("raw stereo open returned %d", ret);

    if (ret == RIG_OK)
    {
        TEST_CHECK(rig_stream_get_conversions(stream)
                   == RIG_STREAM_CONV_CHANNELS);
        rig_stream_close(rig, stream);
    }

    rig_stream_config_free(cfg);
    teardown_rig(rig);
}

/* Codec-frame API semantics against the stub backend (no data threads):
 * frames produced with the internal backend API come back through
 * rig_stream_read() one whole frame per call with exact index/duration
 * reporting; undersized buffers consume nothing; oversized frames are
 * refused; a full ring drops the newest frame with overrun accounting. */
void test_codec_frame_read_api(void)
{
    RIG *rig = setup_rig(&stub_caps_codec_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_OPUS;
    cfg->sample_rate = 48000;
    cfg->channels = 1;
    cfg->buffer_bytes = 4096;   /* small, deterministic codec ring */

    rig_stream_t *st = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &st) == RIG_OK && st != NULL);
    rig_stream_config_free(cfg);

    uint8_t f1[100], f2[200], buf[2048];

    for (int i = 0; i < 200; i++)
    {
        if (i < 100) { f1[i] = (uint8_t)i; }

        f2[i] = (uint8_t)(100 + i);
    }

    TEST_CHECK(stream_backend_write_frame(st, f1, sizeof(f1), 480)
               == (ssize_t)sizeof(f1));
    TEST_CHECK(stream_backend_write_frame(st, f2, sizeof(f2), 480)
               == (ssize_t)sizeof(f2));
    TEST_CHECK(rig_stream_get_samples_written(st) == 960);

    size_t got = 0;
    struct rig_stream_read_info info;

    TEST_CHECK(rig_stream_read(rig, st, buf, sizeof(buf), &got, 100,
                               &info) == RIG_OK);
    TEST_CHECK(got == sizeof(f1));
    TEST_CHECK(memcmp(buf, f1, sizeof(f1)) == 0);
    TEST_CHECK(info.sample_index == 0);
    TEST_CHECK(info.codec_frame_samples == 480);
    TEST_MSG("frame 1: got %lu bytes, index %llu, dur %u",
             (unsigned long)got,
             (unsigned long long)info.sample_index,
             info.codec_frame_samples);

    /* Undersized buffer: refused, frame 2 stays queued intact. */
    TEST_CHECK(rig_stream_read(rig, st, buf, 100, &got, 100, &info)
               == -RIG_EINVAL);

    TEST_CHECK(rig_stream_read(rig, st, buf, sizeof(buf), &got, 100,
                               &info) == RIG_OK);
    TEST_CHECK(got == sizeof(f2));
    TEST_CHECK(memcmp(buf, f2, sizeof(f2)) == 0);
    TEST_CHECK(info.sample_index == 480);
    TEST_CHECK(info.codec_frame_samples == 480);

    /* A frame beyond max_payload is impossible on the wire: refused. */
    static uint8_t big[4000];
    TEST_CHECK(stream_backend_write_frame(st, big, sizeof(big), 480)
               == -RIG_EINVAL);

    /* Drop-newest: overfill the 4096-byte ring with 200-byte frames. */
    int drops = 0;

    for (int i = 0; i < 40; i++)
    {
        if (stream_backend_write_frame(st, f2, sizeof(f2), 480) == 0)
        {
            drops++;
        }
    }

    TEST_CHECK(drops > 0);
    TEST_MSG("expected drops on a full 4096-byte ring, got %d", drops);

    struct rig_stream_stats stats;
    TEST_CHECK(rig_stream_get_stats(rig, st, &stats) == RIG_OK);
    TEST_CHECK(stats.overruns == (uint32_t)drops);
    TEST_MSG("overruns %u, drops %d", stats.overruns, drops);
    TEST_CHECK(stats.dropped_samples_overrun == (uint64_t)drops * 480);
    /* Frames produced = the two consumed at the top plus the accepted
     * portion of the overfill (dropped frames never count). */
    TEST_CHECK(stats.codec_frames == (uint64_t)(2 + 40 - drops));
    TEST_MSG("codec_frames %llu, expected %d",
             (unsigned long long)stats.codec_frames, 2 + 40 - drops);

    /* Drain, then produce one more frame: its start index sits past the
     * dropped-frame hole and the read reports the loss. */
    while (rig_stream_read(rig, st, buf, sizeof(buf), &got, 0,
                           &info) == RIG_OK)
    {
    }

    TEST_CHECK(stream_backend_write_frame(st, f1, sizeof(f1), 480)
               == (ssize_t)sizeof(f1));
    TEST_CHECK(rig_stream_read(rig, st, buf, sizeof(buf), &got, 100,
                               &info) == RIG_OK);
    TEST_CHECK(info.dropped_samples == (uint32_t)drops * 480);
    TEST_MSG("dropped_samples %u", info.dropped_samples);
    TEST_CHECK((info.drop_flags & RIG_STREAM_DROP_OVERRUN) != 0);

    rig_stream_close(rig, st);
    teardown_rig(rig);
}


/* Codec-frame TX path: rig_stream_write() carries one whole frame per
 * call with the app-declared duration; the backend dequeues records with
 * exact metadata; a full ring blocks up to the timeout instead of
 * dropping or overwriting. */
void test_codec_frame_write_api(void)
{
    RIG *rig = setup_rig(&stub_caps_codec_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg->format = RIG_STREAM_FORMAT_OPUS;
    cfg->sample_rate = 48000;
    cfg->channels = 1;
    cfg->buffer_bytes = 1024;   /* tiny ring to exercise the timeout */

    rig_stream_t *st = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &st) == RIG_OK && st != NULL);
    rig_stream_config_free(cfg);

    uint8_t frame[180], buf[2048];

    for (int i = 0; i < (int)sizeof(frame); i++)
    {
        frame[i] = (uint8_t)(i ^ 0x5A);
    }

    struct rig_stream_write_info winfo;
    memset(&winfo, 0, sizeof(winfo));
    winfo.codec_frame_samples = 480;

    size_t written = 0;
    TEST_CHECK(rig_stream_write(rig, st, frame, sizeof(frame), &written,
                                100, &winfo) == RIG_OK);
    TEST_CHECK(written == sizeof(frame));
    TEST_CHECK(rig_stream_get_samples_written(st) == 480);

    size_t len = 0;
    uint32_t dur = 0;
    uint64_t idx = 1;
    TEST_CHECK(stream_backend_read_frame(st, buf, sizeof(buf), &len, &dur,
                                         &idx, 100) == RIG_OK);
    TEST_CHECK(len == sizeof(frame));
    TEST_CHECK(memcmp(buf, frame, sizeof(frame)) == 0);
    TEST_CHECK(dur == 480);
    TEST_CHECK(idx == 0);

    /* Undersized consumer cap: refused, record left queued. */
    TEST_CHECK(rig_stream_write(rig, st, frame, sizeof(frame), &written,
                                100, &winfo) == RIG_OK);
    TEST_CHECK(stream_backend_read_frame(st, buf, 50, &len, &dur, &idx,
                                         100) == -RIG_EINVAL);
    TEST_CHECK(stream_backend_read_frame(st, buf, sizeof(buf), &len, &dur,
                                         &idx, 100) == RIG_OK);
    TEST_CHECK(idx == 480);

    /* Fill the 1024-byte ring (5 x 192-byte records fit), then the next
     * write times out — never dropped, never overwritten, no overrun
     * accounting. */
    int accepted = 0, ret;

    do
    {
        ret = rig_stream_write(rig, st, frame, sizeof(frame), &written,
                               20, &winfo);

        if (ret == RIG_OK)
        {
            accepted++;
        }
    }
    while (ret == RIG_OK && accepted < 20);

    TEST_CHECK(ret == -RIG_ETIMEOUT);
    TEST_MSG("expected -RIG_ETIMEOUT on the full ring, got %d", ret);
    TEST_CHECK(accepted > 0 && accepted < 20);

    struct rig_stream_stats stats;
    TEST_CHECK(rig_stream_get_stats(rig, st, &stats) == RIG_OK);
    TEST_CHECK(stats.overruns == 0);
    TEST_CHECK(stats.dropped_samples_overrun == 0);

    rig_stream_close(rig, st);
    teardown_rig(rig);
}


/* A codec-only caps entry is served verbatim: derivation must not widen
 * formats, rates or channels it cannot serve. */
void test_caps_codec_only_not_widened(void)
{
    RIG *rig = setup_rig(&stub_caps_codec_stream);
    TEST_ASSERT(rig != NULL);

    const struct rig_stream_caps *mixed = rig_stream_caps_at(rig, 0);
    const struct rig_stream_caps *codec = rig_stream_caps_at(rig, 1);
    TEST_ASSERT(mixed != NULL && codec != NULL);

    /* Mixed entry: PCM family widened, the codec bit passes through. */
    TEST_CHECK((mixed->formats & RIG_STREAM_FORMAT_PCM_F32) != 0);
    TEST_CHECK((mixed->formats & RIG_STREAM_FORMAT_OPUS) != 0);

    /* Codec-only entry: exactly what the backend declared. */
    TEST_CHECK(codec->formats == RIG_STREAM_FORMAT_OPUS);
    TEST_MSG("codec-only formats: got 0x%x", (unsigned)codec->formats);
    TEST_CHECK(codec->sample_rates[0] == 48000);
    TEST_CHECK(codec->sample_rates[1] == 0);
    TEST_MSG("codec-only rate list must stay native (got second rate %d)",
             codec->sample_rates[1]);
    TEST_CHECK(codec->channels[0] == 1 && codec->channels[1] == 0);

    /* Native view mirrors the declaration as usual. */
    TEST_CHECK(codec->native_formats == RIG_STREAM_FORMAT_OPUS);
    TEST_CHECK(codec->native_sample_rates[0] == 48000);
    TEST_CHECK(codec->native_channels[0] == 1
               && codec->native_channels[1] == 0);

    teardown_rig(rig);
}

/* Before rig_open there is no stream state: the accessor serves the raw
 * backend declaration (native semantics, native_* fields zero). */
void test_caps_pre_open_raw(void)
{
    rig_register(&stub_caps_with_stream);
    RIG *rig = rig_init(stub_caps_with_stream.rig_model);
    TEST_ASSERT(rig != NULL);

    const struct rig_stream_caps *audio = rig_stream_caps_at(rig, 0);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->formats == (RIG_STREAM_FORMAT_PCM_S16
                                  | RIG_STREAM_FORMAT_PCM_F32));
    TEST_CHECK(audio->native_formats == 0);

    rig_cleanup(rig);
}

/* A fully native open (format, rate and channels all declared) reports
 * RIG_STREAM_CONV_NONE and satisfies require_native. */
void test_conversions_getter(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    TEST_CHECK(rig_stream_get_conversions(NULL) == -RIG_EINVAL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    cfg->sample_rate = 48000;
    cfg->channels = 1;
    cfg->require_native = 1;

    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &stream) == RIG_OK);
    TEST_CHECK(rig_stream_get_conversions(stream) == RIG_STREAM_CONV_NONE);

    rig_stream_close(rig, stream);
    rig_stream_config_free(cfg);
    teardown_rig(rig);
}

/* rig_stream_config_alloc returns a zeroed, library-sized config; the app
 * fills fields, opens with it, then frees it (the stream keeps its own copy).
 * rig_stream_config_free(NULL) is a no-op. */
void test_config_alloc_free(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);

    /* Zeroed on alloc: 0 == default everywhere */
    TEST_CHECK(cfg->type == 0);
    TEST_CHECK(cfg->format == 0);
    TEST_CHECK(cfg->sample_rate == 0);
    TEST_CHECK(cfg->channels == 0);

    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    cfg->sample_rate = 48000;
    cfg->channels = 1;

    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, cfg, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    rig_stream_close(rig, stream);

    /* Freeing the config after open is safe: the stream copied it. */
    rig_stream_config_free(cfg);
    /* free(NULL) is a no-op */
    rig_stream_config_free(NULL);

    teardown_rig(rig);
}

/* rig_stream_get_max_payload reports the effective, frame-aligned payload
 * derived from config.mtu: default (0) gives the 1500-derived value, and a
 * huge MTU clamps to the jumbo ceiling. NULL stream returns -1. */
void test_get_max_payload(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;   /* frame_bytes = 2 */
    cfg->sample_rate = 48000;
    cfg->channels = 1;

    /* Default MTU: 1500 - 40 - 8 - 32 = 1420, already 2-aligned. */
    rig_stream_t *s = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &s) == RIG_OK);
    TEST_CHECK(rig_stream_get_max_payload(s) == 1420);
    TEST_MSG("default max_payload = %d", rig_stream_get_max_payload(s));
    rig_stream_close(rig, s);

    /* Huge MTU clamps to the jumbo ceiling: 9216 - 80 = 9136, 2-aligned. */
    cfg->mtu = 100000;
    s = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &s) == RIG_OK);
    TEST_CHECK(rig_stream_get_max_payload(s) == 9136);
    TEST_MSG("clamped max_payload = %d", rig_stream_get_max_payload(s));
    rig_stream_close(rig, s);

    rig_stream_config_free(cfg);

    TEST_CHECK(rig_stream_get_max_payload(NULL) == -1);

    teardown_rig(rig);
}

/* open must reject a NULL config with EINVAL rather than dereferencing it. */
void test_open_null_config(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    rig_stream_t *stream = NULL;
    TEST_CHECK(rig_stream_open(rig, NULL, &stream) == -RIG_EINVAL);

    teardown_rig(rig);
}

void test_open_close(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_open returned %d", ret);
    TEST_CHECK(stream != NULL);

    ret = rig_stream_close(rig, stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_close returned %d", ret);

    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_default_buffer_derived_from_rate(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    /* No buffer_bytes / buffer_duration_ms: audio defaults to 250 ms at the
     * configured rate and format (48k S16 mono = 24000 B, ring rounds up to
     * the next power of 2). */
    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);
    TEST_CHECK(stream->ringbuf.capacity == 32768);
    TEST_MSG("capacity=%zu", stream->ringbuf.capacity);
    rig_stream_close(rig, stream);

    /* An explicit buffer_duration_ms still wins (100 ms = 9600 B -> 16384). */
    config->buffer_duration_ms = 100;
    stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);
    TEST_CHECK(stream->ringbuf.capacity == 16384);
    TEST_MSG("capacity=%zu", stream->ringbuf.capacity);
    rig_stream_close(rig, stream);

    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_open_unsupported_format(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_OPUS;  /* Not in stub caps */
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_CHECK(ret == -RIG_EINVAL);
    TEST_MSG("Expected -RIG_EINVAL, got %d", ret);

    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_open_unsupported_rate(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 96000;  /* Not in stub caps */
    config->channels = 1;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_CHECK(ret == -RIG_EINVAL);
    TEST_MSG("Expected -RIG_EINVAL, got %d", ret);

    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_open_no_backend(void)
{
    RIG *rig = setup_rig(&stub_caps_no_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_CHECK(ret == -RIG_ENIMPL);
    TEST_MSG("Expected -RIG_ENIMPL, got %d", ret);

    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_close_null(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    int ret = rig_stream_close(rig, NULL);
    TEST_CHECK(ret == -RIG_EINVAL);

    teardown_rig(rig);
}

/* read/write must reject NULL rig/stream/buffer/count pointers with EINVAL
 * rather than dereferencing them. */
void test_read_write_null_args(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);

    unsigned char buf[16];
    size_t n = 0;

    TEST_CHECK(rig_stream_read(rig, NULL, buf, sizeof(buf), &n, 10, NULL)
               == -RIG_EINVAL);
    TEST_CHECK(rig_stream_read(rig, stream, NULL, sizeof(buf), &n, 10, NULL)
               == -RIG_EINVAL);
    TEST_CHECK(rig_stream_read(rig, stream, buf, sizeof(buf), NULL, 10, NULL)
               == -RIG_EINVAL);

    TEST_CHECK(rig_stream_write(rig, NULL, buf, sizeof(buf), &n, 10, NULL)
               == -RIG_EINVAL);
    TEST_CHECK(rig_stream_write(rig, stream, NULL, sizeof(buf), &n, 10, NULL)
               == -RIG_EINVAL);
    TEST_CHECK(rig_stream_write(rig, stream, buf, sizeof(buf), NULL, 10, NULL)
               == -RIG_EINVAL);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_read_write_ringbuf(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_ASSERT(ret == RIG_OK);

    /* Write directly to ring buffer (simulating backend producer) */
    int16_t samples[4] = { 100, -200, 32767, -32768 };
    stream_ringbuf_write(&stream->ringbuf, samples, sizeof(samples));

    /* Read via API */
    int16_t out[4] = { 0 };
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, out, sizeof(out), &bytes_read, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read == sizeof(samples));
    TEST_CHECK(memcmp(samples, out, sizeof(samples)) == 0);
    TEST_MSG("bytes_read=%zu", bytes_read);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_overrun_underrun_counters(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->buffer_bytes = 64;  /* Small buffer for easy overrun */
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_stats stats;
    TEST_CHECK(rig_stream_get_stats(rig, stream, &stats) == RIG_OK);
    TEST_CHECK(stats.overruns == 0);
    TEST_CHECK(stats.underruns == 0);

    /* Cause overrun: fill buffer, then write more to overflow */
    uint8_t data[64];
    memset(data, 0xAA, sizeof(data));
    stream_ringbuf_write(&stream->ringbuf, data,
                         sizeof(data));  /* Fill to capacity */
    stream_ringbuf_write(&stream->ringbuf, data,
                         1);             /* Overflows by 1 byte */
    rig_stream_get_stats(rig, stream, &stats);
    TEST_CHECK(stats.overruns == 1);

    /* Cause underrun: read from empty buffer with short timeout */
    uint8_t out[8];
    uint8_t drain[64];
    size_t bytes_read = 0;
    /* Drain first */
    stream_ringbuf_read(&stream->ringbuf, drain, sizeof(drain), 10);
    /* Now read from empty */
    ret = rig_stream_read(rig, stream, out, sizeof(out), &bytes_read, 10, NULL);
    TEST_CHECK(ret == -RIG_ETIMEOUT);
    rig_stream_get_stats(rig, stream, &stats);
    TEST_CHECK(stats.underruns >= 1);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_gap_counter(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->buffer_bytes = 256;
    rig_stream_t *stream = NULL;
    struct rig_stream_stats stats;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_ASSERT(ret == RIG_OK);

    /* Initially zero */
    TEST_CHECK(rig_stream_get_stats(rig, stream, &stats) == RIG_OK);
    TEST_CHECK(stats.gaps == 0);

    /* Backend reports radio-side gaps */
    rig_stream_mark_gap(stream, 100);
    rig_stream_get_stats(rig, stream, &stats);
    TEST_CHECK(stats.gaps == 1);
    TEST_CHECK(stats.dropped_samples_gap == 100);

    rig_stream_mark_gap(stream, 0);
    rig_stream_get_stats(rig, stream, &stats);
    TEST_CHECK(stats.gaps == 2);
    TEST_CHECK(stats.gaps_unknown == 1);

    /* NULL checks */
    TEST_CHECK(rig_stream_get_stats(NULL, stream, &stats) == -RIG_EINVAL);
    TEST_CHECK(rig_stream_get_stats(rig, NULL, &stats) == -RIG_EINVAL);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_get_type_id(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    TEST_CHECK(rig_stream_get_type(stream) == RIG_STREAM_TYPE_AUDIO_RX);
    TEST_CHECK(rig_stream_get_id(stream) == 0);  /* First stream, id=0 */

    /* Open a second stream, verify id increments */
    rig_stream_t *stream2 = NULL;
    rig_stream_open(rig, config, &stream2);
    TEST_ASSERT(stream2 != NULL);
    TEST_CHECK(rig_stream_get_id(stream2) == 1);

    rig_stream_close(rig, stream);
    rig_stream_close(rig, stream2);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_per_type_namespace(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *audio_config = rig_stream_config_alloc();
    TEST_ASSERT(audio_config != NULL);
    audio_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    audio_config->format = RIG_STREAM_FORMAT_PCM_S16;
    audio_config->sample_rate = 48000;
    audio_config->channels = 1;
    struct rig_stream_config *iq_config = rig_stream_config_alloc();
    TEST_ASSERT(iq_config != NULL);
    iq_config->type = RIG_STREAM_TYPE_IQ_RX;
    iq_config->format = RIG_STREAM_FORMAT_IQ_CS16;
    iq_config->sample_rate = 48000;
    iq_config->channels = 1;

    rig_stream_t *audio_stream = NULL;
    rig_stream_t *iq_stream = NULL;

    int ret1 = rig_stream_open(rig, audio_config, &audio_stream);
    int ret2 = rig_stream_open(rig, iq_config, &iq_stream);

    TEST_CHECK(ret1 == RIG_OK);
    TEST_CHECK(ret2 == RIG_OK);

    /* Both should have id=0 since they're different types */
    TEST_CHECK(rig_stream_get_id(audio_stream) == 0);
    TEST_CHECK(rig_stream_get_id(iq_stream) == 0);
    TEST_CHECK(rig_stream_get_type(audio_stream) == RIG_STREAM_TYPE_AUDIO_RX);
    TEST_CHECK(rig_stream_get_type(iq_stream) == RIG_STREAM_TYPE_IQ_RX);

    rig_stream_close(rig, audio_stream);
    rig_stream_close(rig, iq_stream);
    rig_stream_config_free(iq_config);
    rig_stream_config_free(audio_config);
    teardown_rig(rig);
}

void test_pause_resume(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    TEST_CHECK(stream->paused == 0);

    /* Write data so there's something to read */
    int16_t samples[4] = { 100, -200, 32767, -32768 };
    stream_ringbuf_write(&stream->ringbuf, samples, sizeof(samples));

    /* Pause the stream */
    int ret = rig_stream_pause(rig, stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(stream->paused == 1);

    /* Read while paused should return -RIG_ETIMEOUT (data not delivered) */
    int16_t out[4] = { 0 };
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, out, sizeof(out), &bytes_read, 10, NULL);
    TEST_CHECK(ret == -RIG_ETIMEOUT);
    TEST_MSG("Expected -RIG_ETIMEOUT while paused, got %d", ret);
    TEST_CHECK(bytes_read == 0);

    /* Resume */
    ret = rig_stream_resume(rig, stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(stream->paused == 0);

    /* Now read should succeed and return the original data */
    ret = rig_stream_read(rig, stream, out, sizeof(out), &bytes_read, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read == sizeof(samples));
    TEST_CHECK(memcmp(samples, out, sizeof(samples)) == 0);
    TEST_MSG("Data after resume should match original");

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_mute_unmute(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    TEST_CHECK(stream->muted == 0);

    /* Write nonzero data */
    int16_t samples[4] = { 100, -200, 32767, -32768 };
    stream_ringbuf_write(&stream->ringbuf, samples, sizeof(samples));

    /* Mute the stream */
    int ret = rig_stream_mute(rig, stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(stream->muted == 1);

    /* Read while muted: should return zeros (silence), not the real data */
    int16_t out[4] = { 99, 99, 99, 99 };
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, out, sizeof(out), &bytes_read, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read == sizeof(samples));

    for (int i = 0; i < 4; i++)
    {
        TEST_CHECK(out[i] == 0);
        TEST_MSG("out[%d] = %d (expected 0 while muted)", i, out[i]);
    }

    /* Data should be consumed from ring buffer even when muted */
    TEST_CHECK(stream_ringbuf_available(&stream->ringbuf) == 0);

    /* Unmute */
    ret = rig_stream_unmute(rig, stream);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(stream->muted == 0);

    /* Write new data and read — should get real data now */
    int16_t samples2[2] = { 500, -500 };
    stream_ringbuf_write(&stream->ringbuf, samples2, sizeof(samples2));
    ret = rig_stream_read(rig, stream, out, sizeof(samples2), &bytes_read, 100,
                          NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read == sizeof(samples2));
    TEST_CHECK(out[0] == 500 && out[1] == -500);
    TEST_MSG("After unmute: out[0]=%d out[1]=%d", out[0], out[1]);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_set_buffer_size(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->buffer_bytes = 4096;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_ASSERT(ret == RIG_OK);

    /* Ring buffer capacity should be >= requested (power of 2) */
    TEST_CHECK(stream->ringbuf.capacity >= 4096);
    TEST_MSG("capacity=%zu", stream->ringbuf.capacity);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_max_streams_enforced(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    cfg->sample_rate = 48000;
    cfg->channels = 1;
    rig_stream_t *s1 = NULL;
    rig_stream_t *s2 = NULL;
    rig_stream_t *s3 = NULL;

    TEST_CHECK(rig_stream_open(rig, cfg, &s1) == RIG_OK);
    TEST_CHECK(rig_stream_open(rig, cfg, &s2) == RIG_OK);
    TEST_CHECK(rig_stream_open(rig, cfg, &s3) == -RIG_EINVAL);
    TEST_MSG("third AUDIO_RX open should fail (max_streams=2)");

    rig_stream_close(rig, s1);
    rig_stream_close(rig, s2);
    rig_stream_config_free(cfg);
    teardown_rig(rig);
}


void test_buffer_duration_ms(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->buffer_bytes = 0;
    config->buffer_duration_ms = 100;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    TEST_CHECK(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);
    /* 48000 * 2 * 0.1 = 9600 bytes requested; ring rounds up to power of 2 */
    TEST_CHECK(stream->ringbuf.capacity >= 9600);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}


void test_mute_while_paused(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;

    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    int16_t samples[4] = { 100, -200, 300, -400 };
    stream_ringbuf_write(&stream->ringbuf, samples, sizeof(samples));

    TEST_CHECK(rig_stream_pause(rig, stream) == RIG_OK);
    TEST_CHECK(rig_stream_mute(rig, stream) == RIG_OK);

    int16_t out[4] = { 0 };
    size_t br = 0;
    int ret = rig_stream_read(rig, stream, out, sizeof(out), &br, 10, NULL);

    TEST_CHECK(ret == -RIG_ETIMEOUT);
    TEST_CHECK(br == 0);

    TEST_CHECK(rig_stream_resume(rig, stream) == RIG_OK);

    ret = rig_stream_read(rig, stream, out, sizeof(out), &br, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(br == sizeof(samples));

    for (int i = 0; i < 4; i++)
    {
        TEST_CHECK(out[i] == 0);
    }

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}


void test_default_buffer_size(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    /* Audio stream with buffer_bytes=0 defaults to 250 ms at the configured
     * rate/format (48k S16 mono = 24000 B, rounded up to a power of 2) */
    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);
    TEST_CHECK(stream->ringbuf.capacity == 32768);
    TEST_MSG("audio capacity=%zu", stream->ringbuf.capacity);
    rig_stream_close(rig, stream);

    /* I/Q stream with buffer_bytes=0 should get default IQ ring size */
    struct rig_stream_config *iq_config = rig_stream_config_alloc();
    TEST_ASSERT(iq_config != NULL);
    iq_config->type = RIG_STREAM_TYPE_IQ_RX;
    iq_config->format = RIG_STREAM_FORMAT_IQ_CS16;
    iq_config->sample_rate = 48000;
    iq_config->channels = 1;
    iq_config->buffer_bytes = 0;
    rig_stream_t *iq_stream = NULL;
    rig_stream_open(rig, iq_config, &iq_stream);
    TEST_ASSERT(iq_stream != NULL);
    TEST_CHECK(iq_stream->ringbuf.capacity == 4 * 1024 * 1024);
    TEST_MSG("iq capacity=%zu", iq_stream->ringbuf.capacity);
    rig_stream_close(rig, iq_stream);

    rig_stream_config_free(iq_config);
    rig_stream_config_free(config);
    teardown_rig(rig);
}


/* ------------------------------------------------------------------ */
/* Metadata tests                                                      */
/* ------------------------------------------------------------------ */

void test_get_latest_metadata(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    /* Set a known frequency in cache */
    rig_set_cache_freq(rig, RIG_VFO_A, 14074000);
    CACHE(rig)->ptt = RIG_PTT_OFF;

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    struct rig_stream_metadata meta;
    int ret = rig_stream_read_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(meta.vfo_freq == 14074000);
    TEST_CHECK(meta.ptt == 0);
    TEST_CHECK(meta.field_mask & RIG_STREAM_META_VFO_FREQ);
    TEST_CHECK(meta.field_mask & RIG_STREAM_META_PTT);
    TEST_CHECK(meta.field_mask & RIG_STREAM_META_VFO_ID);
    TEST_MSG("freq=%llu ptt=%d mask=0x%x",
             (unsigned long long)meta.vfo_freq, meta.ptt, meta.field_mask);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_write_metadata(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    /* Write metadata with a frequency change */
    struct rig_stream_metadata meta =
    {
        .field_mask = RIG_STREAM_META_VFO_FREQ,
        .sample_index = 1000,
        .vfo_freq = 14200000,
        .ptt = 0,
        .vfo_id = 0,
    };
    stub_set_freq_calls = 0;
    stub_set_freq_last = 0;

    int ret = rig_stream_write_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("write_metadata returned %d", ret);

    /* First write applies the frequency exactly once. */
    TEST_CHECK(stub_set_freq_calls == 1);
    TEST_CHECK(stub_set_freq_last == 14200000);
    TEST_CHECK(stream->last_metadata.vfo_freq == 14200000);
    TEST_MSG("last_metadata.vfo_freq = %llu",
             (unsigned long long)stream->last_metadata.vfo_freq);

    /* Writing the same frequency is suppressed by change detection: no
     * additional rig_set_freq call. */
    ret = rig_stream_write_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(stub_set_freq_calls == 1);

    /* A different frequency applies once more and updates last_metadata. */
    meta.vfo_freq = 14250000;
    ret = rig_stream_write_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(stub_set_freq_calls == 2);
    TEST_CHECK(stub_set_freq_last == 14250000);
    TEST_CHECK(stream->last_metadata.vfo_freq == 14250000);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_metadata_changed(void)
{
    struct rig_stream_metadata a =
    {
        .field_mask = RIG_STREAM_META_VFO_FREQ | RIG_STREAM_META_PTT,
        .sample_index = 100,
        .vfo_freq = 14074000,
        .ptt = 0,
        .vfo_id = 0,
    };
    struct rig_stream_metadata b = a;

    /* Identical (ignoring sample_index) -> 0 */
    b.sample_index = 200;
    TEST_CHECK(stream_metadata_changed(&a, &b) == 0);
    TEST_MSG("Identical metadata should return 0");

    /* Change freq -> 1 */
    b.vfo_freq = 14075000;
    TEST_CHECK(stream_metadata_changed(&a, &b) == 1);
    TEST_MSG("Different freq should return 1");

    /* Restore freq, change PTT -> 1 */
    b.vfo_freq = a.vfo_freq;
    b.ptt = 1;
    TEST_CHECK(stream_metadata_changed(&a, &b) == 1);
    TEST_MSG("Different ptt should return 1");

    /* Change only sample_index -> 0 */
    b = a;
    b.sample_index = 999;
    TEST_CHECK(stream_metadata_changed(&a, &b) == 0);
    TEST_MSG("Only sample_index differs should return 0");
}

void test_metadata_field_mask(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    /* Set freq to 0 — should still have FREQ bit set */
    rig_set_cache_freq(rig, RIG_VFO_A, 0);
    CACHE(rig)->ptt = RIG_PTT_OFF;

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    struct rig_stream_metadata meta;
    rig_stream_read_metadata(rig, stream, &meta);

    /* All three fields should always be present */
    TEST_CHECK((meta.field_mask & RIG_STREAM_META_VFO_FREQ) != 0);
    TEST_CHECK((meta.field_mask & RIG_STREAM_META_PTT) != 0);
    TEST_CHECK((meta.field_mask & RIG_STREAM_META_VFO_ID) != 0);
    TEST_CHECK(meta.vfo_freq == 0);
    TEST_CHECK(meta.ptt == 0);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}


void test_write_via_api(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    /* Write via the public API (not stream_ringbuf_write) */
    int16_t samples[4] = { 111, -222, 333, -444 };
    size_t bytes_written = 0;
    int ret = rig_stream_write(rig, stream, samples, sizeof(samples),
                               &bytes_written, 0, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_written == sizeof(samples));
    TEST_MSG("bytes_written=%zu", bytes_written);

    /* Read back via API and verify data integrity */
    int16_t out[4] = { 0 };
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, out, sizeof(out), &bytes_read, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read == sizeof(samples));
    TEST_CHECK(memcmp(samples, out, sizeof(samples)) == 0);
    TEST_MSG("Write-read roundtrip via API: data mismatch");

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

void test_flush(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    rig_stream_open(rig, config, &stream);
    TEST_ASSERT(stream != NULL);

    /* Write data to buffer */
    int16_t samples[4] = { 1, 2, 3, 4 };
    stream_ringbuf_write(&stream->ringbuf, samples, sizeof(samples));
    TEST_CHECK(stream_ringbuf_available(&stream->ringbuf) == sizeof(samples));

    /* Flush on an unconsumed buffer should timeout (no consumer draining it) */
    int ret = rig_stream_drain(rig, stream, 20);
    TEST_CHECK(ret == -RIG_ETIMEOUT);
    TEST_MSG("flush with data and no consumer should timeout, got %d", ret);

    /* Drain the buffer manually, then flush should succeed */
    uint8_t drain[64];
    stream_ringbuf_read(&stream->ringbuf, drain, sizeof(drain), 10);
    TEST_CHECK(stream_ringbuf_available(&stream->ringbuf) == 0);

    ret = rig_stream_drain(rig, stream, 20);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("flush on empty buffer should succeed, got %d", ret);

    rig_stream_close(rig, stream);
    rig_stream_config_free(config);
    teardown_rig(rig);
}

/* A close of a handle the registry does not know (never opened here, or a
 * stale pointer after its real close) is rejected via the table lookup rather
 * than dereferenced. The real pointer is freed by its first close, so this
 * uses a separate zeroed struct — it does not re-close the freed handle
 * (that would be undefined behaviour). */
void test_close_unknown_stream(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    TEST_ASSERT(ret == RIG_OK);

    /* First close of the real handle succeeds and frees it. */
    ret = rig_stream_close(rig, stream);
    TEST_CHECK(ret == RIG_OK);

    /* A handle the registry does not contain is rejected, not dereferenced. */
    struct rig_stream unknown;
    memset(&unknown, 0, sizeof(unknown));
    unknown.type = RIG_STREAM_TYPE_AUDIO_RX;
    ret = rig_stream_close(rig, &unknown);
    TEST_CHECK(ret == -RIG_EINVAL);
    TEST_MSG("Close of unknown handle should return -RIG_EINVAL, got %d", ret);

    rig_stream_config_free(config);
    teardown_rig(rig);
}


/* The generic (frontend, all-streams) stream conf tokens round-trip through
 * rig_set_conf / rig_get_conf2 and reject out-of-range input. */
void test_stream_conf_tokens(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);
    char val[32];

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_TRANSPORT_BUFFER_MS, "500") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_TRANSPORT_BUFFER_MS, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "500") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_TRANSPORT_BUFFER_BYTES,
                            "1048576") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_TRANSPORT_BUFFER_BYTES, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "1048576") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_METADATA_REFRESH, "200") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_METADATA_REFRESH, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "200") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_METADATA_INTERVAL, "50") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_METADATA_INTERVAL, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "50") == 0);

    /* Resampler quality: combo defaults to medium, accepts the three
     * levels, rejects anything else (value kept on failed set). */
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_RESAMPLE_QUALITY, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "medium") == 0);
    TEST_MSG("default quality: got '%s', expected 'medium'", val);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_RESAMPLE_QUALITY,
                            "best") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_RESAMPLE_QUALITY, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "best") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_RESAMPLE_QUALITY,
                            "fast") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_RESAMPLE_QUALITY,
                            "bogus") == -RIG_EINVAL);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_RESAMPLE_QUALITY, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "fast") == 0);
    TEST_MSG("after rejected set: got '%s', expected 'fast'", val);

    /* out-of-range rejected */
    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_TRANSPORT_BUFFER_MS,
                            "-1") == -RIG_EINVAL);

    /* stream_source_id: -1 = unset/derive (default), 0 = emit unset on the
     * wire, 1..65535 = explicit ID */
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_SOURCE_ID, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "-1") == 0);
    TEST_MSG("default: got %s, expected -1", val);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_SOURCE_ID, "7") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_SOURCE_ID, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "7") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_SOURCE_ID, "0") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_STREAM_SOURCE_ID, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "0") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_SOURCE_ID, "-1") == RIG_OK);

    /* out-of-range rejected */
    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_SOURCE_ID, "65536") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_STREAM_SOURCE_ID, "-2") == -RIG_EINVAL);

    teardown_rig(rig);
}


/* Close-while-blocked-reader: a reader parked in rig_stream_read() with a long
 * timeout must be woken by a concurrent rig_stream_close() (returning promptly,
 * not after the full timeout) and must not touch freed memory. */
struct close_race_ctx
{
    RIG *rig;
    rig_stream_t *stream;
    int read_ret;
    size_t got;
};

static void *blocked_reader(void *arg)
{
    struct close_race_ctx *c = arg;
    unsigned char buf[256];
    size_t got = 0;

    c->read_ret = rig_stream_read(c->rig, c->stream, buf, sizeof(buf),
                                  &got, 5000, NULL);
    c->got = got;
    return NULL;
}

/* Reader with an infinite (timeout_ms < 0) wait, plus a done flag so the test
 * can confirm it actually blocks rather than returning early or spinning. */
static volatile int block_forever_done;

static void *block_forever_reader(void *arg)
{
    struct close_race_ctx *c = arg;
    unsigned char buf[256];
    size_t got = 0;

    c->read_ret = rig_stream_read(c->rig, c->stream, buf, sizeof(buf),
                                  &got, -1, NULL);
    c->got = got;
    block_forever_done = 1;
    return NULL;
}

/* timeout_ms < 0 must block indefinitely (no instant timeout, no busy-spin)
 * and be released by close. */
void test_read_block_forever_until_close(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    cfg->sample_rate = 48000;
    cfg->channels = 1;

    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &stream) == RIG_OK);
    rig_stream_config_free(cfg);

    struct close_race_ctx ctx = { rig, stream, 999, 0 };
    block_forever_done = 0;
    pthread_t th;
    TEST_ASSERT(pthread_create(&th, NULL, block_forever_reader, &ctx) == 0);

    /* With no producer, an infinite wait must still be blocking after a delay. */
    usleep(200 * 1000);
    TEST_CHECK_(block_forever_done == 0,
                "read(timeout<0) returned before any data or close");

    /* Close releases the blocked reader. */
    TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK);
    pthread_join(th, NULL);

    TEST_CHECK(block_forever_done == 1);
    TEST_CHECK(ctx.read_ret == -RIG_ENAVAIL);
    TEST_CHECK(ctx.got == 0);

    teardown_rig(rig);
}

void test_close_wakes_blocked_reader(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    cfg->sample_rate = 48000;
    cfg->channels = 1;

    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &stream) == RIG_OK);
    rig_stream_config_free(cfg);

    struct close_race_ctx ctx = { rig, stream, 999, 0 };
    pthread_t th;
    TEST_ASSERT(pthread_create(&th, NULL, blocked_reader, &ctx) == 0);

    /* Let the reader reach the blocking wait, then close it out from under. */
    usleep(150 * 1000);
    TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK);

    pthread_join(th, NULL);

    /* Woken by close, not by its own 5 s timeout. */
    TEST_CHECK(ctx.read_ret == -RIG_ENAVAIL);
    TEST_CHECK(ctx.got == 0);

    teardown_rig(rig);
}


struct inflight_ctx { RIG *rig; rig_stream_t *stream; };

static void *inflight_reader(void *arg)
{
    struct inflight_ctx *c = arg;
    unsigned char buf[64];
    size_t got = 0;
    rig_stream_read(c->rig, c->stream, buf, sizeof(buf), &got, 0, NULL);
    return NULL;
}

/* The in-use guard must make rig_stream_close() wait for a non-blocked call
 * that is in flight (here, inside the backend read hook) before it frees the
 * stream. Without the guard, close would free the stream while the read is
 * still running. */
void test_close_waits_for_inflight_call(void)
{
    RIG *rig = setup_rig(&stub_caps_slow_read);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    cfg->sample_rate = 48000;
    cfg->channels = 1;

    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, cfg, &stream) == RIG_OK);
    rig_stream_config_free(cfg);

    slow_read_in_progress = 0;
    slow_read_done = 0;

    struct inflight_ctx ctx = { rig, stream };
    pthread_t th;
    TEST_ASSERT(pthread_create(&th, NULL, inflight_reader, &ctx) == 0);

    /* Wait until the reader is inside the backend read hook. */
    while (!slow_read_in_progress)
    {
        usleep(1000);
    }

    /* Close must block until the in-flight read returns. */
    TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK);
    TEST_CHECK_(slow_read_done == 1,
                "close returned before the in-flight read completed");

    pthread_join(th, NULL);
    teardown_rig(rig);
}


/* The struct_size field is the ABI forward-compatibility gate: rig_stream_open
 * rejects a zero struct_size and copies min(struct_size, sizeof) over a zeroed
 * target, so an older/smaller or newer/larger caller struct stays safe. */
void test_struct_size_gate(void)
{
    RIG *rig = setup_rig(&stub_caps_with_stream);
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config cfg;
    rig_stream_t *stream = NULL;

    /* struct_size == 0 (config not from the allocator) is rejected. */
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;
    cfg.struct_size = 0;
    TEST_CHECK(rig_stream_open(rig, &cfg, &stream) == -RIG_EINVAL);

    /* NULL out-pointer is rejected. */
    cfg.struct_size = sizeof(cfg);
    TEST_CHECK(rig_stream_open(rig, &cfg, NULL) == -RIG_EINVAL);

    /* An older/smaller struct_size opens; the uncopied tail defaults to 0. */
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;
    cfg.struct_size = offsetof(struct rig_stream_config, frame_samples);
    stream = NULL;
    TEST_CHECK(rig_stream_open(rig, &cfg, &stream) == RIG_OK);
    TEST_CHECK(stream != NULL);

    if (stream)
    {
        rig_stream_close(rig, stream);
    }

    /* A larger-than-known struct_size clamps to our sizeof (no over-read). */
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;
    cfg.struct_size = sizeof(cfg) + 64;
    stream = NULL;
    TEST_CHECK(rig_stream_open(rig, &cfg, &stream) == RIG_OK);

    if (stream)
    {
        rig_stream_close(rig, stream);
    }

    teardown_rig(rig);
}


TEST_LIST =
{
    { "get_stream_caps_none",     test_get_stream_caps_none },
    { "stream_conf_tokens",       test_stream_conf_tokens },
    { "get_stream_caps",          test_get_stream_caps },
    { "caps_derived_formats",     test_caps_derived_formats },
    { "caps_derived_rates",       test_caps_derived_rates },
    { "caps_derived_channels",    test_caps_derived_channels },
    { "channel_list_exact",       test_channel_list_exact },
    { "codec_format_native_only", test_codec_format_native_only },
    { "codec_frame_read_api",     test_codec_frame_read_api },
    { "codec_frame_write_api",    test_codec_frame_write_api },
    { "caps_codec_only_not_widened", test_caps_codec_only_not_widened },
    { "caps_pre_open_raw",        test_caps_pre_open_raw },
    { "conversions_getter",       test_conversions_getter },
    { "config_alloc_free",        test_config_alloc_free },
    { "get_max_payload",          test_get_max_payload },
    { "open_null_config",         test_open_null_config },
    { "open_close",               test_open_close },
    { "default_buffer_from_rate", test_default_buffer_derived_from_rate },
    { "open_unsupported_format",      test_open_unsupported_format },
    { "open_unsupported_rate",        test_open_unsupported_rate },
    { "open_no_backend",          test_open_no_backend },
    { "close_null",               test_close_null },
    { "read_write_null_args",     test_read_write_null_args },
    { "read_write_ringbuf",       test_read_write_ringbuf },
    { "write_via_api",            test_write_via_api },
    { "overrun_underrun_counters", test_overrun_underrun_counters },
    { "gap_counter",              test_gap_counter },
    { "get_type_id",              test_get_type_id },
    { "per_type_namespace",       test_per_type_namespace },
    { "pause_resume",             test_pause_resume },
    { "mute_unmute",              test_mute_unmute },
    { "flush",                    test_flush },
    { "set_buffer_size",          test_set_buffer_size },
    { "max_streams_enforced",     test_max_streams_enforced },
    { "buffer_duration_ms",       test_buffer_duration_ms },
    { "mute_while_paused",        test_mute_while_paused },
    { "default_buffer_size",      test_default_buffer_size },
    { "close_unknown_stream",     test_close_unknown_stream },
    { "close_wakes_blocked_reader", test_close_wakes_blocked_reader },
    { "close_waits_for_inflight_call", test_close_waits_for_inflight_call },
    { "read_block_forever_until_close", test_read_block_forever_until_close },
    { "struct_size_gate",         test_struct_size_gate },
    /* Metadata */
    { "get_latest_metadata",      test_get_latest_metadata },
    { "write_metadata",           test_write_metadata },
    { "stream_metadata_changed",         test_metadata_changed },
    { "metadata_field_mask",      test_metadata_field_mask },
    { NULL, NULL }
};
