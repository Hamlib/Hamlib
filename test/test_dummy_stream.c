/*
 *  Hamlib dummy backend streaming tests
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

/* Integration tests for the dummy backend's streaming implementation. */
/* Exercises the streaming API end-to-end using RIG_MODEL_DUMMY. */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"
#include <hamlib/rig.h>
#include "hamlib/rig_state.h"
#include <string.h>
#include <math.h>

#include "stream_time.h"

/* Access dummy backend private data for mode configuration */
#include "../rigs/dummy/dummy.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* ------------------------------------------------------------------ */
/* Minimal radix-2 DIT FFT for test validation (real input).           */
/* ------------------------------------------------------------------ */

/* In-place complex FFT.  re[] and im[] must have size n (power of 2). */
static void fft_dit(float *re, float *im, size_t n)
{
    /* Bit-reversal permutation */
    for (size_t i = 1, j = 0; i < n; i++)
    {
        size_t bit = n >> 1;

        for (; j & bit; bit >>= 1)
        {
            j ^= bit;
        }

        j ^= bit;

        if (i < j)
        {
            float tmp;
            tmp = re[i]; re[i] = re[j]; re[j] = tmp;
            tmp = im[i]; im[i] = im[j]; im[j] = tmp;
        }
    }

    /* Cooley-Tukey butterfly */
    for (size_t len = 2; len <= n; len *= 2)
    {
        float ang = -2.0f * (float)M_PI / (float)len;
        float wr = cosf(ang);
        float wi = sinf(ang);

        for (size_t i = 0; i < n; i += len)
        {
            float cur_r = 1.0f, cur_i = 0.0f;

            for (size_t j = 0; j < len / 2; j++)
            {
                float tr = cur_r * re[i + j + len / 2]
                           - cur_i * im[i + j + len / 2];
                float ti = cur_r * im[i + j + len / 2]
                           + cur_i * re[i + j + len / 2];
                re[i + j + len / 2] = re[i + j] - tr;
                im[i + j + len / 2] = im[i + j] - ti;
                re[i + j] += tr;
                im[i + j] += ti;
                float new_r = cur_r * wr - cur_i * wi;
                cur_i = cur_r * wi + cur_i * wr;
                cur_r = new_r;
            }
        }
    }
}

/* Compute magnitude spectrum from real input.  n must be power of 2.
 * magnitude[] must have size n. Only first n/2 bins are meaningful. */
static void test_fft_magnitude(const float *input, float *magnitude, size_t n)
{
    float *re = malloc(n * sizeof(float));
    float *im = calloc(n, sizeof(float));

    memcpy(re, input, n * sizeof(float));
    fft_dit(re, im, n);

    for (size_t i = 0; i < n; i++)
    {
        magnitude[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    }

    free(re);
    free(im);
}

/* Compute magnitude spectrum from complex (interleaved I/Q) input. */
static void test_fft_magnitude_complex(const float *iq_input,
                                       float *magnitude, size_t n)
{
    float *re = malloc(n * sizeof(float));
    float *im = malloc(n * sizeof(float));

    for (size_t i = 0; i < n; i++)
    {
        re[i] = iq_input[2 * i];
        im[i] = iq_input[2 * i + 1];
    }

    fft_dit(re, im, n);

    for (size_t i = 0; i < n; i++)
    {
        magnitude[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    }

    free(re);
    free(im);
}

/* Find the bin with the highest magnitude. */
static size_t test_fft_peak_bin(const float *magnitude, size_t n)
{
    size_t peak = 0;
    float max_val = magnitude[0];

    for (size_t i = 1; i < n; i++)
    {
        if (magnitude[i] > max_val)
        {
            max_val = magnitude[i];
            peak = i;
        }
    }

    return peak;
}

/* Convert bin index to frequency in Hz. */
static float test_fft_bin_to_freq(size_t bin, size_t n, int sample_rate)
{
    return (float)bin * (float)sample_rate / (float)n;
}


/* Helper: initialize and open a dummy rig.  Returns NULL on failure. */
static RIG *open_dummy(void)
{
    rig_load_all_backends();

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

    return rig;
}

/* Helper: close and clean up a rig. */
static void close_dummy(RIG *rig)
{
    if (rig)
    {
        rig_close(rig);
        rig_cleanup(rig);
    }
}


/* ------------------------------------------------------------------ */
/* Capabilities tests                                                  */
/* ------------------------------------------------------------------ */

void test_caps_query(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    int count = rig_stream_caps_count(rig);

    /* Dummy declares 4 stream types: audio RX/TX, I/Q RX/TX */
    TEST_CHECK(count == 4);

    /* Verify each type is present exactly once */
    int found[RIG_STREAM_TYPE_COUNT] = {0};

    for (int i = 0; i < count; i++)
    {
        const struct rig_stream_caps *c = rig_stream_caps_at(rig, i);
        TEST_ASSERT(c != NULL);
        TEST_CHECK(c->type >= 0 && c->type < RIG_STREAM_TYPE_COUNT);
        found[c->type]++;
    }

    TEST_CHECK(found[RIG_STREAM_TYPE_AUDIO_RX] == 1);
    TEST_CHECK(found[RIG_STREAM_TYPE_AUDIO_TX] == 1);
    TEST_CHECK(found[RIG_STREAM_TYPE_IQ_RX] == 1);
    TEST_CHECK(found[RIG_STREAM_TYPE_IQ_TX] == 1);

    close_dummy(rig);
}

void test_caps_audio_all_formats(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    int count = rig_stream_caps_count(rig);
    TEST_ASSERT(count >= 1);

    /* Find audio RX caps */
    const struct rig_stream_caps *audio_rx = NULL;

    for (int i = 0; i < count; i++)
    {
        const struct rig_stream_caps *c = rig_stream_caps_at(rig, i);

        if (c && c->type == RIG_STREAM_TYPE_AUDIO_RX)
        {
            audio_rx = c;
            break;
        }
    }

    TEST_ASSERT(audio_rx != NULL);

    /* All 4 PCM formats supported */
    TEST_CHECK(audio_rx->formats & RIG_STREAM_FORMAT_PCM_S8);
    TEST_CHECK(audio_rx->formats & RIG_STREAM_FORMAT_PCM_U8);
    TEST_CHECK(audio_rx->formats & RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(audio_rx->formats & RIG_STREAM_FORMAT_PCM_F32);

    /* Mono and stereo */
    TEST_CHECK(audio_rx->channels_min == 1);
    TEST_CHECK(audio_rx->channels_max == 2);

    /* Sample rates include 8000 and 48000 */
    int has_8k = 0, has_48k = 0;

    for (int i = 0; i < HAMLIB_MAX_STREAM_RATES && audio_rx->sample_rates[i]; i++)
    {
        if (audio_rx->sample_rates[i] == 8000) { has_8k = 1; }

        if (audio_rx->sample_rates[i] == 48000) { has_48k = 1; }
    }

    TEST_CHECK(has_8k);
    TEST_CHECK(has_48k);

    close_dummy(rig);
}

void test_caps_iq_all_formats(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    int count = rig_stream_caps_count(rig);
    TEST_ASSERT(count >= 1);

    /* Find I/Q RX caps */
    const struct rig_stream_caps *iq_rx = NULL;

    for (int i = 0; i < count; i++)
    {
        const struct rig_stream_caps *c = rig_stream_caps_at(rig, i);

        if (c && c->type == RIG_STREAM_TYPE_IQ_RX)
        {
            iq_rx = c;
            break;
        }
    }

    TEST_ASSERT(iq_rx != NULL);

    /* All 4 I/Q formats supported */
    TEST_CHECK(iq_rx->formats & RIG_STREAM_FORMAT_IQ_CU8);
    TEST_CHECK(iq_rx->formats & RIG_STREAM_FORMAT_IQ_CS8);
    TEST_CHECK(iq_rx->formats & RIG_STREAM_FORMAT_IQ_CS16);
    TEST_CHECK(iq_rx->formats & RIG_STREAM_FORMAT_IQ_CF32);

    /* I/Q supports 1..4 coherent channels (interleaved); I+Q are the two
     * components of one channel, not separate channels. */
    TEST_CHECK(iq_rx->channels_min == 1);
    TEST_CHECK(iq_rx->channels_max == 4);

    /* Sample rates include 48000 and 192000 */
    int has_48k = 0, has_192k = 0;

    for (int i = 0; i < HAMLIB_MAX_STREAM_RATES && iq_rx->sample_rates[i]; i++)
    {
        if (iq_rx->sample_rates[i] == 48000) { has_48k = 1; }

        if (iq_rx->sample_rates[i] == 192000) { has_192k = 1; }
    }

    TEST_CHECK(has_48k);
    TEST_CHECK(has_192k);

    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Tone generation tests                                               */
/* ------------------------------------------------------------------ */

void test_audio_rx_tone_f32(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    /* Wait briefly for generator thread to produce data, then read */
    float buf[4800];  /* 100ms at 48kHz */
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify samples are non-zero (tone, not silence) */
    size_t sample_count = bytes_read / sizeof(float);
    int nonzero = 0;

    for (size_t i = 0; i < sample_count; i++)
    {
        if (fabsf(buf[i]) > 1e-6f)
        {
            nonzero++;
        }
    }

    TEST_CHECK(nonzero > (int)(sample_count / 2));
    TEST_MSG("Expected most samples non-zero, got %d/%zu", nonzero, sample_count);

    /* Verify samples are in valid range [-1.0, 1.0] */
    for (size_t i = 0; i < sample_count; i++)
    {
        TEST_CHECK(buf[i] >= -1.0f && buf[i] <= 1.0f);
    }

    rig_stream_close(rig, stream);
    close_dummy(rig);
}

void test_iq_rx_tone_cf32(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_IQ_CF32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    /* Read I/Q data — each sample is 2 floats (I, Q) */
    float buf[4800 * 2];  /* 100ms at 48kHz, complex */
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify I²+Q² ≈ constant (complex exponential has constant magnitude) */
    size_t pair_count = bytes_read / (2 * sizeof(float));
    TEST_CHECK(pair_count > 100);

    float first_mag = -1.0f;
    int constant_mag = 1;

    for (size_t i = 0; i < pair_count; i++)
    {
        float I = buf[2 * i];
        float Q = buf[2 * i + 1];
        float mag = I * I + Q * Q;

        if (first_mag < 0.0f)
        {
            first_mag = mag;
        }
        else if (fabsf(mag - first_mag) > 0.01f)
        {
            constant_mag = 0;
            break;
        }
    }

    TEST_CHECK(first_mag > 0.01f);
    TEST_MSG("Expected non-zero magnitude, got %f", first_mag);
    TEST_CHECK(constant_mag);
    TEST_MSG("I^2+Q^2 should be constant for complex exponential");

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* 4-channel coherent I/Q: verify frame-atomic packetization and that the
 * per-sample channel interleave carries distinct content per channel. */
void test_iq_rx_four_channel_interleave(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    const int channels = 4;
    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_IQ_CF32;
    config->sample_rate = 48000;
    config->channels = channels;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);
    TEST_ASSERT(stream != NULL);

    /* 100 ms, 4 channels, complex CF32 */
    static float buf[4800 * 2 * 4];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Frame = channels × one complex CF32 pair (8 B) = 32 B; reads are
     * whole frames. */
    size_t frame_bytes = (size_t)channels * 2 * sizeof(float);
    TEST_CHECK(bytes_read % frame_bytes == 0);
    TEST_MSG("bytes_read=%zu not a multiple of frame_bytes=%zu",
             bytes_read, frame_bytes);

    size_t frames = bytes_read / frame_bytes;
    TEST_CHECK(frames > 100);

    /* Each interleaved channel is a complex exponential (constant, non-zero
     * magnitude); channels spin at distinct rates, so channels 1-3 differ
     * from channel 0 somewhere in the buffer — proving per-sample interleave
     * (not a single channel repeated). */
    int ch_differs[4] = {0, 0, 0, 0};
    int mag_ok = 1;

    for (size_t f = 0; f < frames; f++)
    {
        const float *frame = &buf[f * (size_t)channels * 2];
        float i0 = frame[0], q0 = frame[1];

        for (int c = 0; c < channels; c++)
        {
            float I = frame[2 * c];
            float Q = frame[2 * c + 1];

            if (I * I + Q * Q < 0.01f)
            {
                mag_ok = 0;
            }

            if (c > 0 && (fabsf(I - i0) > 0.01f || fabsf(Q - q0) > 0.01f))
            {
                ch_differs[c] = 1;
            }
        }
    }

    TEST_CHECK(mag_ok);
    TEST_MSG("every interleaved channel must carry non-zero I/Q");
    TEST_CHECK(ch_differs[1] && ch_differs[2] && ch_differs[3]);
    TEST_MSG("channels 1-3 must differ from channel 0 (distinct per-channel content)");

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


void test_audio_rx_tone_frequency(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* Read enough samples for good frequency resolution.
     * 8192 samples at 48kHz = ~170ms, bin width = 48000/8192 ≈ 5.86 Hz */
    const size_t N = 8192;
    float *buf = malloc(N * sizeof(float));
    TEST_ASSERT(buf != NULL);

    size_t total_bytes = 0;

    while (total_bytes < N * sizeof(float))
    {
        size_t bytes_read = 0;
        ret = rig_stream_read(rig, stream,
                              (char *)buf + total_bytes,
                              N * sizeof(float) - total_bytes,
                              &bytes_read, 500, NULL);

        if (ret != RIG_OK && bytes_read == 0)
        {
            break;
        }

        total_bytes += bytes_read;
    }

    TEST_ASSERT(total_bytes == N * sizeof(float));

    /* FFT and find peak */
    float *mag = malloc(N * sizeof(float));
    TEST_ASSERT(mag != NULL);
    test_fft_magnitude(buf, mag, N);

    /* Only look at first half (positive frequencies) */
    size_t peak = test_fft_peak_bin(mag, N / 2);
    float peak_freq = test_fft_bin_to_freq(peak, N, 48000);

    /* Default tone is 1000 Hz.  Allow ±1 bin (±5.86 Hz). */
    TEST_CHECK(fabsf(peak_freq - 1000.0f) < 10.0f);
    TEST_MSG("Expected peak near 1000 Hz, got %.1f Hz (bin %zu)", peak_freq, peak);

    free(mag);
    free(buf);
    rig_stream_close(rig, stream);
    close_dummy(rig);
}

void test_iq_rx_cexp_frequency(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_IQ_CF32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* Read 8192 complex samples (16384 floats) */
    const size_t N = 8192;
    float *buf = malloc(N * 2 * sizeof(float));
    TEST_ASSERT(buf != NULL);

    size_t total_bytes = 0;
    size_t target_bytes = N * 2 * sizeof(float);

    while (total_bytes < target_bytes)
    {
        size_t bytes_read = 0;
        ret = rig_stream_read(rig, stream,
                              (char *)buf + total_bytes,
                              target_bytes - total_bytes,
                              &bytes_read, 500, NULL);

        if (ret != RIG_OK && bytes_read == 0)
        {
            break;
        }

        total_bytes += bytes_read;
    }

    TEST_ASSERT(total_bytes == target_bytes);

    /* Complex FFT and find peak */
    float *mag = malloc(N * sizeof(float));
    TEST_ASSERT(mag != NULL);
    test_fft_magnitude_complex(buf, mag, N);

    /* For complex FFT, positive offset frequency appears in first half,
     * negative in second half.  Default I/Q offset is 1000 Hz. */
    size_t peak = test_fft_peak_bin(mag, N);
    float peak_freq = test_fft_bin_to_freq(peak, N, 48000);

    /* Handle wrap-around: if peak is in upper half, it's a negative freq */
    if (peak > N / 2)
    {
        peak_freq = peak_freq - 48000.0f;
    }

    TEST_CHECK(fabsf(peak_freq - 1000.0f) < 10.0f);
    TEST_MSG("Expected peak near 1000 Hz, got %.1f Hz (bin %zu)", peak_freq, peak);

    free(mag);
    free(buf);
    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Tone generation — all audio formats                                 */
/* ------------------------------------------------------------------ */

void test_audio_rx_tone_s16(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    int16_t buf[4800];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    size_t sample_count = bytes_read / sizeof(int16_t);

    /* Verify amplitude: 0.5 * 32767 ≈ 16383. Peak should be near that. */
    int16_t max_val = 0, min_val = 0;

    for (size_t i = 0; i < sample_count; i++)
    {
        if (buf[i] > max_val) { max_val = buf[i]; }

        if (buf[i] < min_val) { min_val = buf[i]; }
    }

    TEST_CHECK(max_val > 14000 && max_val < 17000);
    TEST_MSG("S16LE: max %d, expected ~16383 (got %zu samples)", max_val,
             sample_count);
    TEST_CHECK(min_val < -14000 && min_val > -17000);
    TEST_MSG("S16LE: min %d, expected ~-16383", min_val);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}

void test_audio_rx_tone_s8(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S8;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    int8_t buf[4800];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    size_t sample_count = bytes_read / sizeof(int8_t);

    /* Verify amplitude range: 0.5 * 127 ≈ 63. Must swing both positive and negative. */
    int8_t max_val = 0, min_val = 0;

    for (size_t i = 0; i < sample_count; i++)
    {
        if (buf[i] > max_val) { max_val = buf[i]; }

        if (buf[i] < min_val) { min_val = buf[i]; }
    }

    TEST_CHECK(max_val > 50 && max_val < 70);
    TEST_MSG("S8: max %d, expected ~63 (got %zu samples)", max_val, sample_count);
    TEST_CHECK(min_val < -50 && min_val > -70);
    TEST_MSG("S8: min %d, expected ~-63", min_val);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}

void test_audio_rx_tone_u8(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_U8;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    uint8_t buf[4800];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* U8: center is 128, amplitude 0.5 → range ~[65, 191].
     * Must swing both above and below center. */
    size_t sample_count = bytes_read / sizeof(uint8_t);
    uint8_t min_val = 255, max_val = 0;

    for (size_t i = 0; i < sample_count; i++)
    {
        if (buf[i] < min_val) { min_val = buf[i]; }

        if (buf[i] > max_val) { max_val = buf[i]; }
    }

    TEST_CHECK(max_val > 180 && max_val < 200);
    TEST_MSG("U8: max %u, expected ~191 (got %zu samples)", max_val, sample_count);
    TEST_CHECK(min_val > 55 && min_val < 75);
    TEST_MSG("U8: min %u, expected ~65", min_val);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Tone generation — all I/Q formats                                   */
/* ------------------------------------------------------------------ */

void test_iq_rx_tone_cs16(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_IQ_CS16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* CS16LE: 2 x int16_t per sample (I, Q) = 4 bytes */
    int16_t buf[4800 * 2];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify I²+Q² ≈ constant (within int16 resolution) */
    size_t pair_count = bytes_read / (2 * sizeof(int16_t));
    TEST_CHECK(pair_count > 100);

    float first_mag = -1.0f;
    int constant_mag = 1;

    for (size_t i = 0; i < pair_count; i++)
    {
        float I = (float)buf[2 * i];
        float Q = (float)buf[2 * i + 1];
        float mag = I * I + Q * Q;

        if (first_mag < 0.0f)
        {
            first_mag = mag;
        }
        else if (fabsf(mag - first_mag) / first_mag > 0.05f)
        {
            constant_mag = 0;
            break;
        }
    }

    TEST_CHECK(first_mag > 1.0f);
    TEST_CHECK(constant_mag);
    TEST_MSG("CS16LE: I^2+Q^2 should be approximately constant");

    rig_stream_close(rig, stream);
    close_dummy(rig);
}

void test_iq_rx_tone_cs8(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_IQ_CS8;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* CS8: 2 x int8_t per sample (I, Q) = 2 bytes */
    int8_t buf[4800 * 2];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify I²+Q² ≈ constant (within int8 quantization) */
    size_t pair_count = bytes_read / 2;
    TEST_CHECK(pair_count > 100);

    float first_mag = -1.0f;
    int constant_count = 0;

    for (size_t i = 0; i < pair_count; i++)
    {
        float I = (float)buf[2 * i];
        float Q = (float)buf[2 * i + 1];
        float mag = I * I + Q * Q;

        if (first_mag < 0.0f)
        {
            first_mag = mag;
        }
        else if (fabsf(mag - first_mag) / first_mag < 0.2f)
        {
            constant_count++;
        }
    }

    TEST_CHECK(first_mag > 100.0f);
    TEST_MSG("CS8: I²+Q² magnitude = %.0f, expected > 100", first_mag);
    TEST_CHECK(constant_count > (int)(pair_count / 2));
    TEST_MSG("CS8: %d/%zu pairs with ~constant magnitude", constant_count,
             pair_count);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}

void test_iq_rx_tone_cu8(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_IQ_CU8;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* CU8: 2 x uint8_t per sample (I, Q) = 2 bytes */
    uint8_t buf[4800 * 2];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* CU8: center 128, amplitude 0.5 → I and Q vary in [65, 191].
     * Verify (I-128)²+(Q-128)² ≈ constant */
    size_t pair_count = bytes_read / 2;
    TEST_CHECK(pair_count > 100);

    float first_mag = -1.0f;
    int constant_count = 0;

    for (size_t i = 0; i < pair_count; i++)
    {
        float I = (float)buf[2 * i] - 128.0f;
        float Q = (float)buf[2 * i + 1] - 128.0f;
        float mag = I * I + Q * Q;

        if (first_mag < 0.0f)
        {
            first_mag = mag;
        }
        else if (fabsf(mag - first_mag) / first_mag < 0.2f)
        {
            constant_count++;
        }
    }

    TEST_CHECK(first_mag > 100.0f);
    TEST_MSG("CU8: (I-128)²+(Q-128)² = %.0f, expected > 100", first_mag);
    TEST_CHECK(constant_count > (int)(pair_count / 2));
    TEST_MSG("CU8: %d/%zu pairs with ~constant magnitude", constant_count,
             pair_count);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Stereo and sample rate variations                                   */
/* ------------------------------------------------------------------ */

void test_audio_rx_stereo(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 2;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* Stereo: interleaved L, R.  2 floats per sample. */
    float buf[4800 * 2];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    size_t frame_count = bytes_read / (2 * sizeof(float));
    TEST_CHECK(frame_count > 100);

    /* Verify L and R channels differ: R should be half the amplitude of L.
     * Check a non-zero sample where L is significant. */
    int found_diff = 0;

    for (size_t i = 0; i < frame_count; i++)
    {
        float L = buf[2 * i];
        float R = buf[2 * i + 1];

        if (fabsf(L) > 0.1f)
        {
            /* R should be approximately L * 0.5 */
            float expected_R = L * 0.5f;
            TEST_CHECK(fabsf(R - expected_R) < 0.01f);
            TEST_MSG("Frame %zu: L=%.4f, R=%.4f, expected R=%.4f",
                     i, L, R, expected_R);
            found_diff = 1;
            break;
        }
    }

    TEST_CHECK(found_diff);
    TEST_MSG("Should find at least one frame with significant L value");

    rig_stream_close(rig, stream);
    close_dummy(rig);
}

void test_sample_rate_selection(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Open at 8kHz, read some data */
    struct rig_stream_config *config_8k = rig_stream_config_alloc();
    TEST_ASSERT(config_8k != NULL);
    config_8k->type = RIG_STREAM_TYPE_AUDIO_RX;
    config_8k->format = RIG_STREAM_FORMAT_PCM_F32;
    config_8k->sample_rate = 8000;
    config_8k->channels = 1;
    config_8k->frame_samples = 0;
    config_8k->buffer_bytes = 0;
    rig_stream_t *stream_8k = NULL;
    int ret = rig_stream_open(rig, config_8k, &stream_8k);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config_8k);
    TEST_ASSERT(ret == RIG_OK);

    float buf_8k[800];  /* 100ms at 8kHz */
    size_t bytes_8k = 0;
    ret = rig_stream_read(rig, stream_8k, buf_8k, sizeof(buf_8k), &bytes_8k, 500,
                          NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_8k > 0);

    rig_stream_close(rig, stream_8k);

    /* Open at 48kHz, read some data */
    struct rig_stream_config *config_48k = rig_stream_config_alloc();
    TEST_ASSERT(config_48k != NULL);
    config_48k->type = RIG_STREAM_TYPE_AUDIO_RX;
    config_48k->format = RIG_STREAM_FORMAT_PCM_F32;
    config_48k->sample_rate = 48000;
    config_48k->channels = 1;
    config_48k->frame_samples = 0;
    config_48k->buffer_bytes = 0;
    rig_stream_t *stream_48k = NULL;
    ret = rig_stream_open(rig, config_48k, &stream_48k);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config_48k);
    TEST_ASSERT(ret == RIG_OK);

    float buf_48k[4800];  /* 100ms at 48kHz */
    size_t bytes_48k = 0;
    ret = rig_stream_read(rig, stream_48k, buf_48k, sizeof(buf_48k), &bytes_48k,
                          500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_48k > 0);

    rig_stream_close(rig, stream_48k);

    /* Both produce valid non-zero data */
    int nonzero_8k = 0, nonzero_48k = 0;

    for (size_t i = 0; i < bytes_8k / sizeof(float); i++)
    {
        if (fabsf(buf_8k[i]) > 1e-6f) { nonzero_8k++; }
    }

    for (size_t i = 0; i < bytes_48k / sizeof(float); i++)
    {
        if (fabsf(buf_48k[i]) > 1e-6f) { nonzero_48k++; }
    }

    TEST_CHECK(nonzero_8k > 0);
    TEST_CHECK(nonzero_48k > 0);
    TEST_MSG("8kHz: %d non-zero, 48kHz: %d non-zero", nonzero_8k, nonzero_48k);

    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Silence mode                                                        */
/* ------------------------------------------------------------------ */

void test_audio_rx_silence(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Set silence mode before opening stream */
    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_SILENCE;

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    float buf[4800];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* All samples should be zero */
    size_t sample_count = bytes_read / sizeof(float);
    int nonzero = 0;

    for (size_t i = 0; i < sample_count; i++)
    {
        if (buf[i] != 0.0f)
        {
            nonzero++;
        }
    }

    TEST_CHECK(nonzero == 0);
    TEST_MSG("Silence mode: expected all zeros, got %d non-zero samples", nonzero);

    rig_stream_close(rig, stream);

    /* Restore tone mode for other tests */
    priv->stream_mode = DUMMY_STREAM_TONE;

    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Loopback — matching formats                                         */
/* ------------------------------------------------------------------ */

void test_audio_loopback(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* Open TX first, then RX (RX thread needs the TX stream to read from) */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    tx_config->sample_rate = 48000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write a known pattern to TX */
    int16_t tx_data[480];

    for (int i = 0; i < 480; i++)
    {
        tx_data[i] = (int16_t)(i * 67);  /* deterministic pattern */
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_written == sizeof(tx_data));

    /* Read from RX — loopback thread should copy TX data */
    int16_t rx_data[480];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify at least some of the pattern matches */
    size_t rx_samples = bytes_read / sizeof(int16_t);
    int match_count = 0;

    for (size_t i = 0; i < rx_samples && i < 480; i++)
    {
        if (rx_data[i] == tx_data[i])
        {
            match_count++;
        }
    }

    TEST_CHECK(match_count > (int)(rx_samples / 2));
    TEST_MSG("Loopback: %d/%zu samples matched", match_count, rx_samples);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}

void test_iq_loopback(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_IQ_TX;
    tx_config->format = RIG_STREAM_FORMAT_IQ_CS16;
    tx_config->sample_rate = 48000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_IQ_RX;
    rx_config->format = RIG_STREAM_FORMAT_IQ_CS16;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write known I/Q pattern: 480 complex samples = 960 int16_t */
    int16_t tx_data[960];

    for (int i = 0; i < 960; i++)
    {
        tx_data[i] = (int16_t)(i * 31 - 500);
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_written == sizeof(tx_data));

    int16_t rx_data[960];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    size_t rx_count = bytes_read / sizeof(int16_t);
    int match_count = 0;

    for (size_t i = 0; i < rx_count && i < 960; i++)
    {
        if (rx_data[i] == tx_data[i])
        {
            match_count++;
        }
    }

    TEST_CHECK(match_count > (int)(rx_count / 2));
    TEST_MSG("I/Q Loopback: %d/%zu values matched", match_count, rx_count);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}


/* 4-channel coherent I/Q loopback: interleaved N-channel complex frames must
 * round-trip through the dummy's TX->RX loopback intact. Each channel carries a
 * distinct ramp, so a mono-scoped loopback (carrying only 1 of 4 channels)
 * would drop three channels' data and fail. */
void test_iq_loopback_four_channel(void)
{
    enum { INSTANTS = 480, CH = 4, N = INSTANTS * CH * 2 };

    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_IQ_TX;
    tx_config->format = RIG_STREAM_FORMAT_IQ_CS16;
    tx_config->sample_rate = 48000;
    tx_config->channels = CH;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_IQ_RX;
    rx_config->format = RIG_STREAM_FORMAT_IQ_CS16;
    rx_config->sample_rate = 48000;
    rx_config->channels = CH;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* One frame of interleaved complex samples; each channel a distinct ramp. */
    int16_t tx_data[N];

    for (int s = 0; s < INSTANTS; s++)
    {
        for (int c = 0; c < CH; c++)
        {
            int idx = (s * CH + c) * 2;
            tx_data[idx]     = (int16_t)(1000 * (c + 1) + s); /* I */
            tx_data[idx + 1] = (int16_t)(-1000 * (c + 1) - s); /* Q */
        }
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_written == sizeof(tx_data));

    int16_t rx_data[N];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    size_t rx_count = bytes_read / sizeof(int16_t);
    int match = 0;
    int ch_nonzero[CH] = {0};

    for (size_t i = 0; i < rx_count && i < (size_t)N; i++)
    {
        if (rx_data[i] == tx_data[i])
        {
            match++;
        }

        if (rx_data[i] != 0)
        {
            ch_nonzero[(i / 2) % CH]++;
        }
    }

    TEST_CHECK(match > (int)(rx_count / 2));
    TEST_MSG("4-ch I/Q loopback: %d/%zu values matched", match, rx_count);

    /* Every channel lane must carry data — mono-scoping would zero 3 of 4. */
    for (int c = 0; c < CH; c++)
    {
        TEST_CHECK(ch_nonzero[c] > 0);
        TEST_MSG("channel %d nonzero=%d", c, ch_nonzero[c]);
    }

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Loopback — format conversion                                        */
/* ------------------------------------------------------------------ */

void test_loopback_format_s16_to_f32(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX: S16LE, RX: F32LE */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    tx_config->sample_rate = 48000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write S16LE pattern: half-scale sine-ish values */
    int16_t tx_data[480];

    for (int i = 0; i < 480; i++)
    {
        tx_data[i] = (int16_t)(16383.0 * sin(2.0 * M_PI * i / 48.0));
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read F32LE from RX */
    float rx_data[480];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify conversion: S16LE 16383 should become ~0.5 in F32LE */
    size_t rx_samples = bytes_read / sizeof(float);
    int close_count = 0;

    for (size_t i = 0; i < rx_samples && i < 480; i++)
    {
        float expected = (float)tx_data[i] / 32768.0f;

        if (fabsf(rx_data[i] - expected) < 0.01f)
        {
            close_count++;
        }
    }

    TEST_CHECK(close_count > (int)(rx_samples / 2));
    TEST_MSG("S16->F32 loopback: %d/%zu samples close to expected",
             close_count, rx_samples);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}

void test_loopback_format_f32_to_s16(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX: F32LE, RX: S16LE */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    tx_config->sample_rate = 48000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write F32LE pattern */
    float tx_data[480];

    for (int i = 0; i < 480; i++)
    {
        tx_data[i] = 0.5f * sinf(2.0f * (float)M_PI * (float)i / 48.0f);
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read S16LE from RX */
    int16_t rx_data[480];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify conversion: F32 0.5 should become ~16383 in S16LE */
    size_t rx_samples = bytes_read / sizeof(int16_t);
    int close_count = 0;

    for (size_t i = 0; i < rx_samples && i < 480; i++)
    {
        int16_t expected = (int16_t)(tx_data[i] * 32767.0f);

        if (abs(rx_data[i] - expected) < 2)
        {
            close_count++;
        }
    }

    TEST_CHECK(close_count > (int)(rx_samples / 2));
    TEST_MSG("F32->S16 loopback: %d/%zu samples close to expected",
             close_count, rx_samples);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}

void test_loopback_format_iq_cs16_to_cf32(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX: CS16LE, RX: CF32LE */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_IQ_TX;
    tx_config->format = RIG_STREAM_FORMAT_IQ_CS16;
    tx_config->sample_rate = 48000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_IQ_RX;
    rx_config->format = RIG_STREAM_FORMAT_IQ_CF32;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write CS16LE I/Q pattern: 240 complex samples = 480 int16_t */
    int16_t tx_data[480];

    for (int i = 0; i < 240; i++)
    {
        tx_data[2 * i]     = (int16_t)(16383.0 * cos(2.0 * M_PI * i / 24.0));
        tx_data[2 * i + 1] = (int16_t)(16383.0 * sin(2.0 * M_PI * i / 24.0));
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read CF32LE from RX */
    float rx_data[480];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify conversion */
    size_t rx_pairs = bytes_read / (2 * sizeof(float));
    int close_count = 0;

    for (size_t i = 0; i < rx_pairs && i < 240; i++)
    {
        float exp_i = (float)tx_data[2 * i] / 32768.0f;
        float exp_q = (float)tx_data[2 * i + 1] / 32768.0f;

        if (fabsf(rx_data[2 * i] - exp_i) < 0.01f &&
                fabsf(rx_data[2 * i + 1] - exp_q) < 0.01f)
        {
            close_count++;
        }
    }

    TEST_CHECK(close_count > (int)(rx_pairs / 2));
    TEST_MSG("I/Q CS16->CF32 loopback: %d/%zu pairs close", close_count, rx_pairs);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Loopback — channel conversion                                       */
/* ------------------------------------------------------------------ */

void test_loopback_mono_to_stereo(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX: mono S16LE, RX: stereo S16LE */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    tx_config->sample_rate = 48000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    rx_config->sample_rate = 48000;
    rx_config->channels = 2;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write mono pattern */
    int16_t tx_data[480];

    for (int i = 0; i < 480; i++)
    {
        tx_data[i] = (int16_t)(10000.0 * sin(2.0 * M_PI * i / 48.0));
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read stereo from RX (2x samples) */
    int16_t rx_data[960];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify L == R == original for each frame */
    size_t frames = bytes_read / (2 * sizeof(int16_t));
    int match_count = 0;

    for (size_t i = 0; i < frames && i < 480; i++)
    {
        int16_t L = rx_data[2 * i];
        int16_t R = rx_data[2 * i + 1];

        if (L == tx_data[i] && R == tx_data[i])
        {
            match_count++;
        }
    }

    TEST_CHECK(match_count > (int)(frames / 2));
    TEST_MSG("Mono->stereo: %d/%zu frames matched (L==R==orig)", match_count,
             frames);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}

void test_loopback_stereo_to_mono(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX: stereo S16LE, RX: mono S16LE */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    tx_config->sample_rate = 48000;
    tx_config->channels = 2;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write stereo pattern: L=10000, R=6000 -> mono should be ~8000 */
    int16_t tx_data[960];  /* 480 stereo frames */

    for (int i = 0; i < 480; i++)
    {
        tx_data[2 * i]     = 10000;  /* L */
        tx_data[2 * i + 1] = 6000;   /* R */
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read mono from RX */
    int16_t rx_data[480];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify mono ≈ average of L+R = (10000+6000)/2 = 8000 */
    size_t rx_samples = bytes_read / sizeof(int16_t);
    int close_count = 0;

    for (size_t i = 0; i < rx_samples; i++)
    {
        if (abs(rx_data[i] - 8000) < 2)
        {
            close_count++;
        }
    }

    TEST_CHECK(close_count > (int)(rx_samples / 2));
    TEST_MSG("Stereo->mono: %d/%zu samples close to 8000", close_count, rx_samples);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}

/* ------------------------------------------------------------------ */
/* Loopback — combined format + channel conversion                     */
/* ------------------------------------------------------------------ */

void test_loopback_s16_mono_to_f32_stereo(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX: S16LE mono, RX: F32LE stereo */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_S16;
    tx_config->sample_rate = 48000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    rx_config->sample_rate = 48000;
    rx_config->channels = 2;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write S16LE mono: 16383 = ~0.5 in float */
    int16_t tx_data[480];

    for (int i = 0; i < 480; i++)
    {
        tx_data[i] = 16383;
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read F32LE stereo from RX */
    float rx_data[960];  /* 480 stereo frames */
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify: both L and R should be ~0.5 (mono duplicated to stereo) */
    size_t frames = bytes_read / (2 * sizeof(float));
    int close_count = 0;
    float expected = 16383.0f / 32768.0f;

    for (size_t i = 0; i < frames; i++)
    {
        float L = rx_data[2 * i];
        float R = rx_data[2 * i + 1];

        if (fabsf(L - expected) < 0.01f && fabsf(R - expected) < 0.01f)
        {
            close_count++;
        }
    }

    TEST_CHECK(close_count > (int)(frames / 2));
    TEST_MSG("S16 mono->F32 stereo: %d/%zu frames close to expected",
             close_count, frames);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Loopback — sample rate conversion                                   */
/* ------------------------------------------------------------------ */

#ifdef HAVE_SAMPLERATE
void test_loopback_resample_8k_to_48k(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX at 8kHz F32LE mono, RX at 48kHz F32LE mono */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    tx_config->sample_rate = 8000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write 1kHz sine at 8kHz (80 samples = 10ms) */
    float tx_data[80];

    for (int i = 0; i < 80; i++)
    {
        tx_data[i] = 0.5f * sinf(2.0f * (float)M_PI * 1000.0f
                                 * (float)i / 8000.0f);
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read from RX at 48kHz — expect ~480 samples for 10ms (6x upsample) */
    float rx_data[960];
    memset(rx_data, 0, sizeof(rx_data));
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 500, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify we got more samples than we wrote (upsampled) */
    size_t rx_samples = bytes_read / sizeof(float);
    TEST_CHECK(rx_samples > 80);
    TEST_MSG("Resample 8k->48k: wrote 80 samples, got %zu", rx_samples);

    /* Verify data is non-zero (actual signal came through) */
    int nonzero = 0;

    for (size_t i = 0; i < rx_samples; i++)
    {
        if (fabsf(rx_data[i]) > 0.01f)
        {
            nonzero++;
        }
    }

    TEST_CHECK(nonzero > (int)(rx_samples / 4));
    TEST_MSG("Resample: %d/%zu non-zero samples", nonzero, rx_samples);

    /* FFT verification: if we have enough samples, check 1kHz peak at 48kHz rate.
     * Need power-of-2 FFT size; use 512 if available. */
    if (rx_samples >= 512)
    {
        float *mag = malloc(512 * sizeof(float));

        if (mag)
        {
            test_fft_magnitude(rx_data, mag, 512);
            size_t peak = test_fft_peak_bin(mag, 256);
            float peak_freq = test_fft_bin_to_freq(peak, 512, 48000);
            TEST_CHECK(fabsf(peak_freq - 1000.0f) < 200.0f);
            TEST_MSG("Resample FFT: peak at %.1f Hz, expected ~1000", peak_freq);
            free(mag);
        }
    }

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}
#endif /* HAVE_SAMPLERATE */


/* ------------------------------------------------------------------ */
/* Configuration tokens                                                */
/* ------------------------------------------------------------------ */

void test_conf_tone_freq(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Set tone frequency to 440 Hz via conf token */
    int ret = rig_set_conf(rig, TOK_CFG_STREAM_TONE_FREQ, "440.0");
    TEST_CHECK(ret == RIG_OK);

    /* Verify via get_conf */
    char val[128];
    ret = rig_get_conf2(rig, TOK_CFG_STREAM_TONE_FREQ, val, sizeof(val));
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(fabsf((float)atof(val) - 440.0f) < 1.0f);
    TEST_MSG("get_conf returned: %s", val);

    /* Open audio RX, read 8192 samples, FFT, verify peak at 440 Hz */
    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    const size_t N = 8192;
    float *buf = malloc(N * sizeof(float));
    TEST_ASSERT(buf != NULL);

    size_t total_bytes = 0;

    while (total_bytes < N * sizeof(float))
    {
        size_t bytes_read = 0;
        ret = rig_stream_read(rig, stream,
                              (char *)buf + total_bytes,
                              N * sizeof(float) - total_bytes,
                              &bytes_read, 500, NULL);

        if (ret != RIG_OK && bytes_read == 0)
        {
            break;
        }

        total_bytes += bytes_read;
    }

    TEST_ASSERT(total_bytes == N * sizeof(float));

    float *mag = malloc(N * sizeof(float));
    TEST_ASSERT(mag != NULL);
    test_fft_magnitude(buf, mag, N);

    size_t peak = test_fft_peak_bin(mag, N / 2);
    float peak_freq = test_fft_bin_to_freq(peak, N, 48000);

    TEST_CHECK(fabsf(peak_freq - 440.0f) < 10.0f);
    TEST_MSG("Expected peak near 440 Hz, got %.1f Hz", peak_freq);

    free(mag);
    free(buf);
    rig_stream_close(rig, stream);

    /* Restore default */
    rig_set_conf(rig, TOK_CFG_STREAM_TONE_FREQ, "1000.0");
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Metadata                                                            */
/* ------------------------------------------------------------------ */

void test_dummy_metadata_freq(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Set VFO frequency to a known value */
    int ret = rig_set_freq(rig, RIG_VFO_A, (freq_t)14200000);
    TEST_CHECK(ret == RIG_OK);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* Get metadata — should reflect the VFO frequency from cache */
    struct rig_stream_metadata meta;
    ret = rig_stream_read_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(meta.field_mask & RIG_STREAM_META_VFO_FREQ);
    TEST_CHECK(meta.vfo_freq == 14200000);
    TEST_MSG("Expected vfo_freq=14200000, got %llu",
             (unsigned long long)meta.vfo_freq);

    /* Verify PTT field is present */
    TEST_CHECK(meta.field_mask & RIG_STREAM_META_PTT);
    TEST_CHECK(meta.ptt == 0);  /* RX by default */

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Edge cases                                                          */
/* ------------------------------------------------------------------ */

void test_open_unsupported_format(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Opus is not in dummy's I/Q stream_caps (the fabricated test codec
     * is audio-only), and a codec bit is outside the I/Q family, so this
     * is an outright-unsupported format. */
    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_OPUS;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_CHECK(ret == -RIG_EINVAL);
    TEST_CHECK(stream == NULL);

    close_dummy(rig);
}

void test_open_close_cycle(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Open, use, close, then open again — no leaks or corruption */
    for (int cycle = 0; cycle < 3; cycle++)
    {
        struct rig_stream_config *config = rig_stream_config_alloc();
        TEST_ASSERT(config != NULL);
        config->type = RIG_STREAM_TYPE_AUDIO_RX;
        config->format = RIG_STREAM_FORMAT_PCM_F32;
        config->sample_rate = 48000;
        config->channels = 1;
        config->frame_samples = 0;
        config->buffer_bytes = 0;
        rig_stream_t *stream = NULL;
        int ret = rig_stream_open(rig, config, &stream);

        /* the stream copied the config; releasing it here is safe */
        rig_stream_config_free(config);
        TEST_ASSERT(ret == RIG_OK);
        TEST_ASSERT(stream != NULL);

        float buf[480];
        size_t bytes_read = 0;
        ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 200, NULL);
        TEST_CHECK(ret == RIG_OK);
        TEST_CHECK(bytes_read > 0);

        rig_stream_close(rig, stream);
    }

    close_dummy(rig);
}

void test_multiple_streams(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Open 2 audio RX streams simultaneously */
    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;

    rig_stream_t *stream1 = NULL;
    int ret = rig_stream_open(rig, config, &stream1);
    TEST_ASSERT(ret == RIG_OK);

    rig_stream_t *stream2 = NULL;
    ret = rig_stream_open(rig, config, &stream2);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* Both produce data independently */
    float buf1[480], buf2[480];
    size_t bytes1 = 0, bytes2 = 0;
    ret = rig_stream_read(rig, stream1, buf1, sizeof(buf1), &bytes1, 200, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes1 > 0);

    ret = rig_stream_read(rig, stream2, buf2, sizeof(buf2), &bytes2, 200, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes2 > 0);

    rig_stream_close(rig, stream2);
    rig_stream_close(rig, stream1);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* PTT metadata test                                                   */
/* ------------------------------------------------------------------ */

void test_dummy_metadata_ptt(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* Default PTT is off */
    struct rig_stream_metadata meta;
    ret = rig_stream_read_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(meta.ptt == 0);

    /* Set PTT on */
    ret = rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_ON);
    TEST_CHECK(ret == RIG_OK);

    ret = rig_stream_read_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(meta.ptt == 1);
    TEST_MSG("Expected ptt=1 after PTT on, got %u", meta.ptt);

    /* Set PTT off */
    ret = rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_OFF);
    TEST_CHECK(ret == RIG_OK);

    ret = rig_stream_read_metadata(rig, stream, &meta);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(meta.ptt == 0);
    TEST_MSG("Expected ptt=0 after PTT off, got %u", meta.ptt);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Overrun detection                                                   */
/* ------------------------------------------------------------------ */

void test_overrun_detection(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Use a small buffer to trigger overruns quickly.
     * 4096 bytes at 48kHz mono F32LE = ~21ms to fill. */
    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_samples = 0;
    config->buffer_bytes = 4096;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);
    TEST_ASSERT(ret == RIG_OK);

    /* Don't read for 200ms — buffer will overflow multiple times */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 200000000L };
    nanosleep(&ts, NULL);

    /* Check overrun count */
    struct rig_stream_stats stats;
    memset(&stats, 0, sizeof(stats));
    rig_stream_get_stats(rig, stream, &stats);
    TEST_CHECK(stats.overruns > 0);
    TEST_MSG("Expected overruns > 0 after 200ms with 4KB buffer, got %u",
             stats.overruns);

    /* Now read to drain — data should still be readable */
    float buf[1024];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &bytes_read, 100, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Format negotiation                                                  */
/* ------------------------------------------------------------------ */

void test_format_negotiation(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    /* Open with S16LE, read, close */
    struct rig_stream_config *config_s16 = rig_stream_config_alloc();
    TEST_ASSERT(config_s16 != NULL);
    config_s16->type = RIG_STREAM_TYPE_AUDIO_RX;
    config_s16->format = RIG_STREAM_FORMAT_PCM_S16;
    config_s16->sample_rate = 48000;
    config_s16->channels = 1;
    config_s16->frame_samples = 0;
    config_s16->buffer_bytes = 0;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config_s16, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config_s16);
    TEST_ASSERT(ret == RIG_OK);

    int16_t s16_buf[480];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, stream, s16_buf, sizeof(s16_buf), &bytes_read, 500,
                          NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify valid S16LE data (amplitude check) */
    int16_t s16_max = 0;

    for (size_t i = 0; i < bytes_read / sizeof(int16_t); i++)
    {
        if (abs(s16_buf[i]) > abs(s16_max)) { s16_max = s16_buf[i]; }
    }

    TEST_CHECK(abs(s16_max) > 1000);
    TEST_MSG("S16LE negotiation: peak %d", s16_max);

    rig_stream_close(rig, stream);

    /* Open with F32LE, read, close */
    struct rig_stream_config *config_f32 = rig_stream_config_alloc();
    TEST_ASSERT(config_f32 != NULL);
    config_f32->type = RIG_STREAM_TYPE_AUDIO_RX;
    config_f32->format = RIG_STREAM_FORMAT_PCM_F32;
    config_f32->sample_rate = 48000;
    config_f32->channels = 1;
    config_f32->frame_samples = 0;
    config_f32->buffer_bytes = 0;
    stream = NULL;
    ret = rig_stream_open(rig, config_f32, &stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config_f32);
    TEST_ASSERT(ret == RIG_OK);

    float f32_buf[480];
    bytes_read = 0;
    ret = rig_stream_read(rig, stream, f32_buf, sizeof(f32_buf), &bytes_read, 500,
                          NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(bytes_read > 0);

    /* Verify valid F32LE data (amplitude check) */
    float f32_max = 0.0f;

    for (size_t i = 0; i < bytes_read / sizeof(float); i++)
    {
        if (fabsf(f32_buf[i]) > f32_max) { f32_max = fabsf(f32_buf[i]); }
    }

    TEST_CHECK(f32_max > 0.1f && f32_max <= 1.0f);
    TEST_MSG("F32LE negotiation: peak %.3f", f32_max);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* ------------------------------------------------------------------ */
/* Resample unavailable (no libsamplerate)                             */
/* ------------------------------------------------------------------ */

#ifndef HAVE_SAMPLERATE
void test_loopback_resample_unavailable(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)HAMLIB_STATE(rig)->priv;
    priv->stream_mode = DUMMY_STREAM_LOOPBACK;

    /* TX at 8kHz, RX at 48kHz — should fail without libsamplerate.
     * The loopback thread will silently fail to resample, producing
     * no output. We just verify data flow still works without crashing. */
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    TEST_ASSERT(tx_config != NULL);
    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    tx_config->sample_rate = 8000;
    tx_config->channels = 1;
    tx_config->frame_samples = 0;
    tx_config->buffer_bytes = 0;
    rig_stream_t *tx_stream = NULL;
    int ret = rig_stream_open(rig, tx_config, &tx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(tx_config);
    TEST_ASSERT(ret == RIG_OK);

    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    TEST_ASSERT(rx_config != NULL);
    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    rx_config->sample_rate = 48000;
    rx_config->channels = 1;
    rx_config->frame_samples = 0;
    rx_config->buffer_bytes = 0;
    rig_stream_t *rx_stream = NULL;
    ret = rig_stream_open(rig, rx_config, &rx_stream);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(rx_config);
    TEST_ASSERT(ret == RIG_OK);

    /* Write data — resample will fail, no data on RX side */
    float tx_data[80];

    for (int i = 0; i < 80; i++)
    {
        tx_data[i] = 0.5f * sinf(2.0f * (float)M_PI * (float)i / 80.0f);
    }

    size_t bytes_written = 0;
    ret = rig_stream_write(rig, tx_stream, tx_data, sizeof(tx_data),
                           &bytes_written, 100, NULL);
    TEST_CHECK(ret == RIG_OK);

    /* Read with short timeout — expect timeout (no resampled data) */
    float rx_data[480];
    size_t bytes_read = 0;
    ret = rig_stream_read(rig, rx_stream, rx_data, sizeof(rx_data),
                          &bytes_read, 100, NULL);

    /* Without libsamplerate, the resample step fails and no data arrives */
    TEST_CHECK(bytes_read == 0 || ret == -RIG_ETIMEOUT);
    TEST_MSG("Without libsamplerate: bytes_read=%zu, ret=%d", bytes_read, ret);

    rig_stream_close(rig, rx_stream);
    rig_stream_close(rig, tx_stream);
    priv->stream_mode = DUMMY_STREAM_TONE;
    close_dummy(rig);
}
#endif /* !HAVE_SAMPLERATE */


/* ------------------------------------------------------------------ */
/* Time model tests: anchors, synthetic gaps, timed TX                 */
/* ------------------------------------------------------------------ */

void test_rx_time_info(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);

    /* The generator pushes a host anchor before its first write, so time
     * becomes valid within a few reads. */
    struct rig_stream_read_info info;
    int16_t buf[2048];
    size_t got = 0;
    int valid = 0;
    uint64_t last_index = 0;

    for (int i = 0; i < 50 && !valid; i++)
    {
        if (rig_stream_read(rig, stream, buf, sizeof(buf), &got, 200,
                            &info) == RIG_OK && got > 0)
        {
            valid = info.time_valid;
            last_index = info.sample_index;
        }
    }

    TEST_CHECK_(valid, "time never became valid");
    TEST_CHECK(info.time_source == RIG_STREAM_TIME_SRC_HOST);
    TEST_CHECK(info.time_accuracy == RIG_STREAM_TIME_ACC_MS);
    /* Plausible wall-clock: after 2020, before 2100 */
    TEST_CHECK_(info.seconds > 1577836800LL && info.seconds < 4102444800LL,
                "seconds=%lld", (long long)info.seconds);

    /* Sample position advances monotonically across reads */
    if (rig_stream_read(rig, stream, buf, sizeof(buf), &got, 200,
                        &info) == RIG_OK && got > 0)
    {
        TEST_CHECK_(info.sample_index >= last_index,
                    "index went backwards");
    }

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


void test_synthetic_gap_reporting(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    TEST_ASSERT(rig_set_conf(rig,
                             rig_token_lookup(rig, "stream_synth_gap"),
                             "960") == RIG_OK);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);

    /* Read until the injected gap surfaces (it fires at frame 5, ~50 ms) */
    struct rig_stream_read_info info;
    int16_t buf[4096];
    size_t got = 0;
    uint32_t dropped = 0;
    uint8_t flags = 0;

    for (int i = 0; i < 100 && dropped == 0; i++)
    {
        if (rig_stream_read(rig, stream, buf, sizeof(buf), &got, 200,
                            &info) == RIG_OK && got > 0
                && info.dropped_samples > 0)
        {
            dropped = info.dropped_samples;
            flags = info.drop_flags;
        }
    }

    TEST_CHECK_(dropped == 960, "dropped=%u", dropped);
    TEST_CHECK(flags & RIG_STREAM_DROP_GAP);
    TEST_CHECK(!(flags & RIG_STREAM_DROP_OVERRUN));

    struct rig_stream_stats stats;
    rig_stream_get_stats(rig, stream, &stats);
    TEST_CHECK(stats.gaps >= 1);
    TEST_CHECK(stats.dropped_samples_gap == 960);

    rig_stream_close(rig, stream);

    /* Reset the conf token so later tests aren't affected */
    rig_set_conf(rig, rig_token_lookup(rig, "stream_synth_gap"), "0");
    close_dummy(rig);
}


void test_timed_tx_late_counter(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_TX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);

    /* A target 2 s in the past must transmit immediately and count late */
    struct rig_stream_write_info winfo;
    memset(&winfo, 0, sizeof(winfo));
    winfo.time_valid = 1;
    stream_time_now(&winfo.seconds, &winfo.picoseconds);
    winfo.seconds -= 2;
    winfo.flags = RIG_STREAM_TIME_FLAG_SOB | RIG_STREAM_TIME_FLAG_EOB;

    int16_t buf[480];
    memset(buf, 0, sizeof(buf));
    size_t written = 0;
    TEST_ASSERT(rig_stream_write(rig, stream, buf, sizeof(buf), &written,
                                 100, &winfo) == RIG_OK);

    struct rig_stream_stats stats;
    int late = 0;

    for (int i = 0; i < 50 && !late; i++)
    {
        struct timespec ts = { 0, 20000000L };  /* 20 ms */
        nanosleep(&ts, NULL);
        rig_stream_get_stats(rig, stream, &stats);
        late = stats.tx_late >= 1;
    }

    TEST_CHECK_(late, "tx_late never incremented");

    /* Direct-mode event channel: the same late burst is delivered as a
     * write-status event with real lateness and no REMOTE flag. */
    struct rig_stream_write_status ev;
    memset(&ev, 0, sizeof(ev));
    int rc = rig_stream_wait_write_status(rig, stream, &ev, 500);
    TEST_CHECK_(rc == RIG_OK, "wait_write_status rc=%d", rc);
    TEST_CHECK(ev.event == RIG_STREAM_WRITE_EVENT_LATE);
    TEST_CHECK_(ev.lateness > 0, "lateness=%lld", (long long)ev.lateness);
    TEST_CHECK((ev.flags & RIG_STREAM_WRITE_STATUS_REMOTE) == 0);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* Direct-mode OVERRUN: a write far larger than the ring overwrites unread data;
 * the frontend reports a local (non-REMOTE) OVERRUN write-status event and the
 * aggregate stat counts it locally. */
void test_tx_write_overrun_direct(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_TX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    config->buffer_bytes = 1024;   /* tiny ring to force an overrun */
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);

    /* Pause the consumer so unread data accumulates, then write past the ring
     * capacity — later writes overwrite unread bytes => overrun. */
    rig_stream_pause(rig, stream);
    struct timespec settle = { 0, 50000000L };  /* 50 ms for the TX thread to park */
    nanosleep(&settle, NULL);

    int16_t burst[512];   /* 1024 bytes = the whole ring */
    memset(burst, 0, sizeof(burst));

    for (int i = 0; i < 4; i++)
    {
        size_t written = 0;
        TEST_ASSERT(rig_stream_write(rig, stream, burst, sizeof(burst),
                                     &written, 100, NULL) == RIG_OK);
    }

    rig_stream_resume(rig, stream);

    int got_overrun = 0;

    for (int i = 0; i < 10 && !got_overrun; i++)
    {
        struct rig_stream_write_status ev;
        memset(&ev, 0, sizeof(ev));

        if (rig_stream_wait_write_status(rig, stream, &ev, 300) != RIG_OK)
        {
            break;
        }

        if (ev.event == RIG_STREAM_WRITE_EVENT_OVERRUN)
        {
            got_overrun = 1;
            TEST_CHECK((ev.flags & RIG_STREAM_WRITE_STATUS_REMOTE) == 0);
        }
    }

    TEST_CHECK_(got_overrun, "no local OVERRUN event surfaced");

    struct rig_stream_stats stats;
    rig_stream_get_stats(rig, stream, &stats);
    TEST_CHECK_(stats.overruns >= 1, "overruns=%u", stats.overruns);
    TEST_CHECK(stats.remote_overruns == 0);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


void test_timed_tx_gates_and_keys_ptt(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_TX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);

    /* Schedule a burst 300 ms out with SOB; PTT must key around then */
    struct rig_stream_write_info winfo;
    memset(&winfo, 0, sizeof(winfo));
    winfo.time_valid = 1;
    stream_time_now(&winfo.seconds, &winfo.picoseconds);
    winfo.picoseconds += 300ULL * 1000000000ULL;   /* +300 ms */
    stream_time_normalize(&winfo.seconds, &winfo.picoseconds);
    winfo.flags = RIG_STREAM_TIME_FLAG_SOB;

    int16_t buf[480];
    memset(buf, 0, sizeof(buf));
    size_t written = 0;
    TEST_ASSERT(rig_stream_write(rig, stream, buf, sizeof(buf), &written,
                                 100, &winfo) == RIG_OK);

    ptt_t ptt = RIG_PTT_OFF;
    int keyed = 0;

    for (int i = 0; i < 100 && !keyed; i++)
    {
        struct timespec ts = { 0, 20000000L };  /* 20 ms */
        nanosleep(&ts, NULL);
        rig_get_ptt(rig, RIG_VFO_CURR, &ptt);
        keyed = ptt != RIG_PTT_OFF;
    }

    TEST_CHECK_(keyed, "PTT never keyed for a timed SOB burst");

    /* End the burst: a bare EOB marker unkeys after play-out reaches it */
    memset(&winfo, 0, sizeof(winfo));
    winfo.flags = RIG_STREAM_TIME_FLAG_EOB;
    TEST_ASSERT(rig_stream_write(rig, stream, buf, sizeof(buf), &written,
                                 100, &winfo) == RIG_OK);

    int unkeyed = 0;

    for (int i = 0; i < 150 && !unkeyed; i++)
    {
        struct timespec ts = { 0, 20000000L };
        nanosleep(&ts, NULL);
        rig_get_ptt(rig, RIG_VFO_CURR, &ptt);
        unkeyed = ptt == RIG_PTT_OFF;
    }

    TEST_CHECK_(unkeyed, "PTT never released after EOB");

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


void test_timed_tx_horizon_rejected(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_AUDIO_TX;
    config->format = RIG_STREAM_FORMAT_PCM_S16;
    config->sample_rate = 48000;
    config->channels = 1;
    rig_stream_t *stream = NULL;
    TEST_ASSERT(rig_stream_open(rig, config, &stream) == RIG_OK);

    /* the stream copied the config; releasing it here is safe */
    rig_stream_config_free(config);

    /* Dummy declares a 30 s horizon: a target 60 s out is rejected */
    struct rig_stream_write_info winfo;
    memset(&winfo, 0, sizeof(winfo));
    winfo.time_valid = 1;
    stream_time_now(&winfo.seconds, &winfo.picoseconds);
    winfo.seconds += 60;
    winfo.flags = RIG_STREAM_TIME_FLAG_SOB;

    int16_t buf[480];
    size_t written = 0;
    TEST_CHECK(rig_stream_write(rig, stream, buf, sizeof(buf), &written,
                                100, &winfo) == -RIG_EINVAL);

    rig_stream_close(rig, stream);
    close_dummy(rig);
}


/* --- Frontend conversion against the native-F32 dummy --- */

/* Helper: open an AUDIO_RX stream with the given format/rate, returning
 * the stream (or NULL) and the open return code in *ret_out. */
static rig_stream_t *open_audio_rx(RIG *rig, rig_stream_format_t fmt,
                                   int rate, int require_native,
                                   int *ret_out)
{
    struct rig_stream_config *config = rig_stream_config_alloc();

    if (!config)
    {
        *ret_out = -RIG_ENOMEM;
        return NULL;
    }

    config->type = RIG_STREAM_TYPE_AUDIO_RX;
    config->format = fmt;
    config->sample_rate = rate;
    config->channels = 1;
    config->require_native = require_native;
    rig_stream_t *stream = NULL;
    *ret_out = rig_stream_open(rig, config, &stream);
    rig_stream_config_free(config);
    return stream;
}

/* The conversions bitmask names exactly the stages between the F32-native
 * dummy hardware and the client's request. */
void test_conversions_bitmask(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    int ret;
    rig_stream_t *st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_F32, 48000,
                                     0, &ret);
    TEST_ASSERT(ret == RIG_OK && st != NULL);
    TEST_CHECK(rig_stream_get_conversions(st) == RIG_STREAM_CONV_NONE);
    rig_stream_close(rig, st);

    st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_S16, 48000, 0, &ret);
    TEST_ASSERT(ret == RIG_OK && st != NULL);
    TEST_CHECK(rig_stream_get_conversions(st) == RIG_STREAM_CONV_FORMAT);
    rig_stream_close(rig, st);

#ifdef HAVE_SAMPLERATE
    st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_S16, 44100, 0, &ret);
    TEST_ASSERT(ret == RIG_OK && st != NULL);
    TEST_CHECK(rig_stream_get_conversions(st)
               == (RIG_STREAM_CONV_FORMAT | RIG_STREAM_CONV_RATE));
    rig_stream_close(rig, st);
#endif

    /* Above the largest native rate: outside the effective set. */
    st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_F32, 500000, 0, &ret);
    TEST_CHECK(ret == -RIG_EINVAL && st == NULL);

    close_dummy(rig);
}

/* require_native: native requests open with a hard guarantee, convertible
 * ones are refused with the distinct -RIG_ENAVAIL. */
void test_require_native(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    int ret;
    rig_stream_t *st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_F32, 48000,
                                     1, &ret);
    TEST_ASSERT(ret == RIG_OK && st != NULL);
    TEST_CHECK(rig_stream_get_conversions(st) == RIG_STREAM_CONV_NONE);
    rig_stream_close(rig, st);

    st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_S16, 48000, 1, &ret);
    TEST_CHECK(ret == -RIG_ENAVAIL && st == NULL);
    TEST_MSG("expected -RIG_ENAVAIL, got %d", ret);

#ifdef HAVE_SAMPLERATE
    st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_F32, 44100, 1, &ret);
    TEST_CHECK(ret == -RIG_ENAVAIL && st == NULL);
#endif

    close_dummy(rig);
}

/* Replicates the dummy's fabricated codec-frame generator: LCG frame
 * lengths (seed 0) and a continuing byte counter. */
static size_t next_fab_frame(uint32_t *lcg, uint32_t *counter,
                             uint8_t *out)
{
    *lcg = *lcg * 1103515245u + 12345u;
    size_t len = 16 + (size_t)((*lcg >> 16) % 241);

    for (size_t i = 0; i < len; i++)
    {
        out[i] = (uint8_t)((*counter)++ & 0xFF);
    }

    return len;
}

/* Fabricated codec RX through the dummy generator: every read returns
 * exactly one codec frame whose size and bytes reproduce the generator's
 * deterministic sequence, with the fixed 480-sample duration and a start
 * index advancing by it. */
void test_codec_rx_frame_sequence(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    int ret;
    rig_stream_t *st = open_audio_rx(rig, RIG_STREAM_FORMAT_OPUS, 48000,
                                     0, &ret);
    TEST_ASSERT(ret == RIG_OK && st != NULL);
    TEST_CHECK(rig_stream_get_conversions(st) == RIG_STREAM_CONV_NONE);

    uint32_t lcg = 0, counter = 0;
    uint8_t expect[512], buf[2048];

    for (int k = 0; k < 8; k++)
    {
        size_t elen = next_fab_frame(&lcg, &counter, expect);
        size_t got = 0;
        struct rig_stream_read_info info;
        int rret = rig_stream_read(rig, st, buf, sizeof(buf), &got,
                                   500, &info);

        TEST_CHECK(rret == RIG_OK && got > 0);
        TEST_MSG("frame %d: read returned %d (%lu bytes)", k, rret,
                 (unsigned long)got);

        if (rret != RIG_OK || got == 0)
        {
            break;
        }

        TEST_CHECK(got == elen);
        TEST_MSG("frame %d: got %lu bytes, expected %lu", k,
                 (unsigned long)got, (unsigned long)elen);
        TEST_CHECK(got == elen && memcmp(buf, expect, elen) == 0);
        TEST_MSG("frame %d: payload bytes altered", k);
        TEST_CHECK(info.codec_frame_samples == 480);
        TEST_CHECK(info.sample_index == (uint64_t)k * 480);
        TEST_MSG("frame %d: index %llu, expected %llu", k,
                 (unsigned long long)info.sample_index,
                 (unsigned long long)k * 480);
    }

    rig_stream_close(rig, st);
    close_dummy(rig);
}

/* Codec loopback round-trip: frames written to AUDIO_TX come back on
 * AUDIO_RX byte-identical, with durations and start indexes preserved
 * (records copied verbatim). */
void test_codec_loopback_roundtrip(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    token_t tok = rig_token_lookup(rig, "stream_mode");
    TEST_CHECK(tok != 0);
    TEST_CHECK(rig_set_conf(rig, tok, "loopback") == RIG_OK);

    int ret;
    rig_stream_t *tx = NULL, *rx = NULL;
    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);

    cfg->type = RIG_STREAM_TYPE_AUDIO_TX;
    cfg->format = RIG_STREAM_FORMAT_OPUS;
    cfg->sample_rate = 48000;
    cfg->channels = 1;
    ret = rig_stream_open(rig, cfg, &tx);
    TEST_ASSERT(ret == RIG_OK && tx != NULL);

    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    ret = rig_stream_open(rig, cfg, &rx);
    TEST_ASSERT(ret == RIG_OK && rx != NULL);
    rig_stream_config_free(cfg);

    struct rig_stream_write_info winfo;
    memset(&winfo, 0, sizeof(winfo));
    winfo.codec_frame_samples = 480;

    uint8_t frames[6][256], buf[2048];
    size_t flen[6];
    uint32_t seed = 7;

    for (int k = 0; k < 6; k++)
    {
        seed = seed * 1103515245u + 12345u;
        flen[k] = 20 + (seed >> 16) % 200;

        for (size_t i = 0; i < flen[k]; i++)
        {
            frames[k][i] = (uint8_t)(seed + k + i);
        }

        size_t written = 0;
        ret = rig_stream_write(rig, tx, frames[k], flen[k], &written,
                               200, &winfo);
        TEST_CHECK(ret == RIG_OK && written == flen[k]);
        TEST_MSG("frame %d write: ret %d written %lu", k, ret,
                 (unsigned long)written);
    }

    for (int k = 0; k < 6; k++)
    {
        size_t got = 0;
        struct rig_stream_read_info info;
        ret = rig_stream_read(rig, rx, buf, sizeof(buf), &got, 1000, &info);

        TEST_CHECK(ret == RIG_OK && got > 0);
        TEST_MSG("frame %d read: ret %d got %lu", k, ret,
                 (unsigned long)got);

        if (ret != RIG_OK || got == 0)
        {
            break;
        }

        TEST_CHECK(got == flen[k] && memcmp(buf, frames[k], flen[k]) == 0);
        TEST_MSG("frame %d altered in loopback (got %lu, expected %lu)",
                 k, (unsigned long)got, (unsigned long)flen[k]);
        TEST_CHECK(info.codec_frame_samples == 480);
        TEST_CHECK(info.sample_index == (uint64_t)k * 480);
        TEST_MSG("frame %d index %llu", k,
                 (unsigned long long)info.sample_index);
    }

    rig_stream_close(rig, rx);
    rig_stream_close(rig, tx);
    close_dummy(rig);
}


#ifdef HAVE_SAMPLERATE
/* The stream_resample_quality conf token is honored at pipeline creation:
 * a resampled open succeeds at every advertised quality level. */
void test_resample_quality_open(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    token_t tok = rig_token_lookup(rig, "stream_resample_quality");
    TEST_CHECK(tok != 0);

    static const char *levels[] = { "best", "medium", "fast", NULL };

    for (int i = 0; levels[i] != NULL; i++)
    {
        int ret;
        TEST_CHECK(rig_set_conf(rig, tok, levels[i]) == RIG_OK);

        rig_stream_t *st = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_F32,
                                         44100, 0, &ret);
        TEST_CHECK(ret == RIG_OK && st != NULL);
        TEST_MSG("open at quality '%s' returned %d", levels[i], ret);

        if (st)
        {
            TEST_CHECK(rig_stream_get_conversions(st)
                       == RIG_STREAM_CONV_RATE);
            rig_stream_close(rig, st);
        }
    }

    close_dummy(rig);
}


/* Rate conversion end to end: a 44.1 kHz client stream from the 48 kHz
 * native source must still carry the 1 kHz test tone (resampler filter
 * and rate mapping verified via FFT peak). */
void test_audio_rx_tone_resampled(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    int ret;
    rig_stream_t *stream = open_audio_rx(rig, RIG_STREAM_FORMAT_PCM_F32,
                                         44100, 0, &ret);
    TEST_ASSERT(ret == RIG_OK && stream != NULL);
    TEST_CHECK(rig_stream_get_conversions(stream) == RIG_STREAM_CONV_RATE);

    const size_t N = 8192;
    float *buf = malloc(N * sizeof(float));
    TEST_ASSERT(buf != NULL);

    size_t total_bytes = 0;

    while (total_bytes < N * sizeof(float))
    {
        size_t bytes_read = 0;
        ret = rig_stream_read(rig, stream,
                              (char *)buf + total_bytes,
                              N * sizeof(float) - total_bytes,
                              &bytes_read, 500, NULL);

        if (ret != RIG_OK && bytes_read == 0)
        {
            break;
        }

        total_bytes += bytes_read;
    }

    TEST_ASSERT(total_bytes == N * sizeof(float));

    float *mag = malloc(N * sizeof(float));
    TEST_ASSERT(mag != NULL);
    test_fft_magnitude(buf, mag, N);

    size_t peak = test_fft_peak_bin(mag, N / 2);
    float peak_freq = test_fft_bin_to_freq(peak, N, 44100);

    /* Default tone is 1000 Hz; bin width 44100/8192 = 5.4 Hz. */
    TEST_CHECK(fabsf(peak_freq - 1000.0f) < 11.0f);
    TEST_MSG("Expected peak near 1000 Hz, got %.1f Hz (bin %lu)",
             peak_freq, (unsigned long)peak);

    free(mag);
    free(buf);
    rig_stream_close(rig, stream);
    close_dummy(rig);
}
#endif /* HAVE_SAMPLERATE */

/* I/Q rates are downward-only: above-native is rejected outright. */
void test_iq_upsample_rejected(void)
{
    RIG *rig = open_dummy();
    TEST_ASSERT(rig != NULL);

    struct rig_stream_config *config = rig_stream_config_alloc();
    TEST_ASSERT(config != NULL);
    config->type = RIG_STREAM_TYPE_IQ_RX;
    config->format = RIG_STREAM_FORMAT_IQ_CF32;
    config->sample_rate = 250000;   /* above the 192 kHz native maximum */
    config->channels = 1;
    rig_stream_t *stream = NULL;
    int ret = rig_stream_open(rig, config, &stream);
    rig_stream_config_free(config);

    TEST_CHECK(ret == -RIG_EINVAL && stream == NULL);
    TEST_MSG("expected -RIG_EINVAL, got %d", ret);

    close_dummy(rig);
}


TEST_LIST =
{
    { "caps_query",                   test_caps_query },
    { "caps_audio_all_formats",       test_caps_audio_all_formats },
    { "caps_iq_all_formats",          test_caps_iq_all_formats },
    { "audio_rx_tone_f32",          test_audio_rx_tone_f32 },
    { "iq_rx_tone_cf32",            test_iq_rx_tone_cf32 },
    { "iq_rx_four_channel_interleave",      test_iq_rx_four_channel_interleave },
    { "audio_rx_tone_frequency",      test_audio_rx_tone_frequency },
    { "iq_rx_cexp_frequency",         test_iq_rx_cexp_frequency },
    { "audio_rx_tone_s16",          test_audio_rx_tone_s16 },
    { "audio_rx_tone_s8",             test_audio_rx_tone_s8 },
    { "conversions_bitmask",          test_conversions_bitmask },
    { "require_native",               test_require_native },
    { "codec_rx_frame_sequence",      test_codec_rx_frame_sequence },
    { "codec_loopback_roundtrip",     test_codec_loopback_roundtrip },
#ifdef HAVE_SAMPLERATE
    { "resample_quality_open",        test_resample_quality_open },
    { "audio_rx_tone_resampled",      test_audio_rx_tone_resampled },
#endif
    { "iq_upsample_rejected",         test_iq_upsample_rejected },
    { "audio_rx_tone_u8",             test_audio_rx_tone_u8 },
    { "iq_rx_tone_cs16",            test_iq_rx_tone_cs16 },
    { "iq_rx_tone_cs8",               test_iq_rx_tone_cs8 },
    { "iq_rx_tone_cu8",               test_iq_rx_tone_cu8 },
    { "audio_rx_stereo",              test_audio_rx_stereo },
    { "sample_rate_selection",        test_sample_rate_selection },
    { "audio_rx_silence",             test_audio_rx_silence },
    { "audio_loopback",               test_audio_loopback },
    { "iq_loopback",                  test_iq_loopback },
    { "iq_loopback_four_channel",      test_iq_loopback_four_channel },
    { "loopback_format_s16_to_f32",           test_loopback_format_s16_to_f32 },
    { "loopback_format_f32_to_s16",           test_loopback_format_f32_to_s16 },
    { "loopback_format_iq_cs16_to_cf32",      test_loopback_format_iq_cs16_to_cf32 },
    { "loopback_mono_to_stereo",              test_loopback_mono_to_stereo },
    { "loopback_stereo_to_mono",              test_loopback_stereo_to_mono },
    { "loopback_s16_mono_to_f32_stereo",      test_loopback_s16_mono_to_f32_stereo },
#ifdef HAVE_SAMPLERATE
    { "loopback_resample_8k_to_48k",          test_loopback_resample_8k_to_48k },
#else
    { "loopback_resample_unavailable",        test_loopback_resample_unavailable },
#endif
    { "conf_tone_freq",                        test_conf_tone_freq },
    { "dummy_metadata_freq",                   test_dummy_metadata_freq },
    { "dummy_metadata_ptt",                    test_dummy_metadata_ptt },
    { "open_unsupported_format",               test_open_unsupported_format },
    { "open_close_cycle",                      test_open_close_cycle },
    { "multiple_streams",                      test_multiple_streams },
    { "overrun_detection",                     test_overrun_detection },
    { "format_negotiation",                    test_format_negotiation },
    { "rx_time_info",                          test_rx_time_info },
    { "synthetic_gap_reporting",               test_synthetic_gap_reporting },
    { "timed_tx_late_counter",                 test_timed_tx_late_counter },
    { "tx_write_overrun_direct",               test_tx_write_overrun_direct },
    { "timed_tx_gates_and_keys_ptt",           test_timed_tx_gates_and_keys_ptt },
    { "timed_tx_horizon_rejected",             test_timed_tx_horizon_rejected },
    { NULL, NULL }
};
