/*
 *  Hamlib streaming subsystem
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

/* Sample format conversion for the Hamlib streaming subsystem. */
/* Fast path: direct integer converters; fallback: float-intermediate path. */
/* Converters operate on samples in host byte order. The wire payload is
 * little-endian, so a little-endian host needs no byte swap and the in-memory
 * layout matches the wire directly. A little-endian host is required, enforced
 * by the WORDS_BIGENDIAN guard below. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "stream_convert.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The sample converters dereference multi-byte samples in host byte order and
 * perform no byte swapping, so the little-endian wire payload is only correct
 * on a little-endian host. Fail the build loudly on a big-endian host rather
 * than emit corrupt audio. */
#ifdef WORDS_BIGENDIAN
#error "Hamlib streaming requires a little-endian host"
#endif

/* Float scratch-buffer size (samples) for the float-intermediate fallback. */
#define STREAM_CONVERT_FLOAT_CHUNK 4096


/* ------------------------------------------------------------------ */
/* Format classification helpers                                       */
/* ------------------------------------------------------------------ */

/* True if format is an I/Q format (complex pairs). */
static int is_iq_format(rig_stream_format_t fmt)
{
    return (fmt & (RIG_STREAM_FORMAT_IQ_CS8
                   | RIG_STREAM_FORMAT_IQ_CS16
                   | RIG_STREAM_FORMAT_IQ_CF32
                   | RIG_STREAM_FORMAT_IQ_CU8)) != 0;
}


/* ------------------------------------------------------------------ */
/* rig_stream_format_sample_size                                       */
/* ------------------------------------------------------------------ */

int rig_stream_format_sample_size(rig_stream_format_t format)
{
    switch (format)
    {
    case RIG_STREAM_FORMAT_PCM_S8:
    case RIG_STREAM_FORMAT_PCM_U8:
        return 1;

    case RIG_STREAM_FORMAT_PCM_S16:
        return 2;

    case RIG_STREAM_FORMAT_PCM_F32:
        return 4;

    case RIG_STREAM_FORMAT_IQ_CU8:
    case RIG_STREAM_FORMAT_IQ_CS8:
        return 2;   /* 1 byte I + 1 byte Q */

    case RIG_STREAM_FORMAT_IQ_CS16:
        return 4;   /* 2 bytes I + 2 bytes Q */

    case RIG_STREAM_FORMAT_IQ_CF32:
        return 8;   /* 4 bytes I + 4 bytes Q */

    default:
        return 0;   /* Opus, ADPCM, unknown */
    }
}


/* Scale a float sample by a symmetric power-of-two factor, round to
 * nearest and clamp. The symmetric factor (the same 2^n used by the
 * integer -> float direction) plus round-to-nearest makes
 * integer -> float -> integer round-trips value-exact, which the
 * conversion-pipeline tests rely on. */
static inline long scale_round_clamp(float v, float scale, long lo, long hi)
{
    long r = lrintf(v * scale);

    if (r < lo)
    {
        return lo;
    }

    if (r > hi)
    {
        return hi;
    }

    return r;
}


/* ------------------------------------------------------------------ */
/* Direct integer converters (fast path)                               */
/* ------------------------------------------------------------------ */

/* S8 -> S16: sign-extend and shift left 8 */
static void convert_s8_to_s16(const void *src, void *dst, size_t n)
{
    const int8_t *s = src;
    int16_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (int16_t)s[i] << 8;
    }
}

/* U8 -> S16: subtract 128 offset, shift left 8 */
static void convert_u8_to_s16(const void *src, void *dst, size_t n)
{
    const uint8_t *s = src;
    int16_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (int16_t)((int)s[i] - 128) << 8;
    }
}

/* S16 -> S8: arithmetic shift right 8 */
static void convert_s16_to_s8(const void *src, void *dst, size_t n)
{
    const int16_t *s = src;
    int8_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (int8_t)(s[i] >> 8);
    }
}

/* S16 -> U8: shift right 8, add 128 */
static void convert_s16_to_u8(const void *src, void *dst, size_t n)
{
    const int16_t *s = src;
    uint8_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (uint8_t)((s[i] >> 8) + 128);
    }
}

/* CU8 -> CS8: subtract 128 per component */
static void convert_cu8_to_cs8(const void *src, void *dst, size_t n)
{
    const uint8_t *s = src;
    int8_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (int8_t)((int)s[i] - 128);
    }
}

/* CS8 -> CU8: add 128 per component */
static void convert_cs8_to_cu8(const void *src, void *dst, size_t n)
{
    const int8_t *s = src;
    uint8_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (uint8_t)((int)s[i] + 128);
    }
}

/* CU8 -> CS16: subtract 128, shift left 8 per component */
static void convert_cu8_to_cs16(const void *src, void *dst, size_t n)
{
    const uint8_t *s = src;
    int16_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (int16_t)((int)s[i] - 128) << 8;
    }
}

/* CS8 -> CS16: sign-extend and shift left 8 per component */
static void convert_cs8_to_cs16(const void *src, void *dst, size_t n)
{
    const int8_t *s = src;
    int16_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (int16_t)s[i] << 8;
    }
}

/* CS16 -> CS8: arithmetic shift right 8 per component */
static void convert_cs16_to_cs8(const void *src, void *dst, size_t n)
{
    const int16_t *s = src;
    int8_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (int8_t)(s[i] >> 8);
    }
}

/* CS16 -> CU8: shift right 8, add 128 per component */
static void convert_cs16_to_cu8(const void *src, void *dst, size_t n)
{
    const int16_t *s = src;
    uint8_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (uint8_t)((s[i] >> 8) + 128);
    }
}


/* S8 -> U8: add 128 offset */
static void convert_s8_to_u8(const void *src, void *dst, size_t n)
{
    const int8_t *s = src;
    uint8_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (uint8_t)((int)s[i] + 128);
    }
}

/* U8 -> S8: subtract 128 offset */
static void convert_u8_to_s8(const void *src, void *dst, size_t n)
{
    const uint8_t *s = src;
    int8_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (int8_t)((int)s[i] - 128);
    }
}


/* ------------------------------------------------------------------ */
/* Direct float converters                                             */
/* ------------------------------------------------------------------ */

/* S16 -> F32 */
static void convert_s16_to_f32(const void *src, void *dst, size_t n)
{
    const int16_t *s = src;
    float *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (float)s[i] / 32768.0f;
    }
}

/* F32 -> S16 with clamping */
static void convert_f32_to_s16(const void *src, void *dst, size_t n)
{
    const float *s = src;
    int16_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (int16_t)scale_round_clamp(s[i], 32768.0f, -32768, 32767);
    }
}

/* S8 -> F32 */
static void convert_s8_to_f32(const void *src, void *dst, size_t n)
{
    const int8_t *s = src;
    float *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (float)s[i] / 128.0f;
    }
}

/* U8 -> F32: center at 128, scale to [-1, ~1] */
static void convert_u8_to_f32(const void *src, void *dst, size_t n)
{
    const uint8_t *s = src;
    float *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = ((float)s[i] - 128.0f) / 128.0f;
    }
}

/* CS16 -> CF32: same as S16->F32 but for I/Q components */
static void convert_cs16_to_cf32(const void *src, void *dst, size_t n)
{
    convert_s16_to_f32(src, dst, 2 * n);
}

/* CF32 -> CS16 */
static void convert_cf32_to_cs16(const void *src, void *dst, size_t n)
{
    convert_f32_to_s16(src, dst, 2 * n);
}

/* CU8 -> CF32: subtract 128 offset, scale to [-1, ~1] per component */
static void convert_cu8_to_cf32(const void *src, void *dst, size_t n)
{
    const uint8_t *s = src;
    float *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = ((float)s[i] - 128.0f) / 128.0f;
    }
}

/* CS8 -> CF32 */
static void convert_cs8_to_cf32(const void *src, void *dst, size_t n)
{
    const int8_t *s = src;
    float *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (float)s[i] / 128.0f;
    }
}

/* CF32 -> CU8 */
static void convert_cf32_to_cu8(const void *src, void *dst, size_t n)
{
    const float *s = src;
    uint8_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (uint8_t)(scale_round_clamp(s[i], 128.0f, -128, 127) + 128);
    }
}

/* CF32 -> CS8 */
static void convert_cf32_to_cs8(const void *src, void *dst, size_t n)
{
    const float *s = src;
    int8_t *d = dst;

    for (size_t i = 0; i < 2 * n; i++)
    {
        d[i] = (int8_t)scale_round_clamp(s[i], 128.0f, -128, 127);
    }
}

/* F32 -> S8 with clamping */
static void convert_f32_to_s8(const void *src, void *dst, size_t n)
{
    const float *s = src;
    int8_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (int8_t)scale_round_clamp(s[i], 128.0f, -128, 127);
    }
}

/* F32 -> U8 with clamping */
static void convert_f32_to_u8(const void *src, void *dst, size_t n)
{
    const float *s = src;
    uint8_t *d = dst;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (uint8_t)(scale_round_clamp(s[i], 128.0f, -128, 127) + 128);
    }
}

/* ------------------------------------------------------------------ */
/* Dispatch table                                                      */
/* ------------------------------------------------------------------ */

/* Format index for the dispatch table. */
static int format_index(rig_stream_format_t fmt)
{
    switch (fmt)
    {
    case RIG_STREAM_FORMAT_PCM_S8:      return 0;

    case RIG_STREAM_FORMAT_PCM_U8:      return 1;

    case RIG_STREAM_FORMAT_PCM_S16:     return 2;

    case RIG_STREAM_FORMAT_PCM_F32:     return 3;

    case RIG_STREAM_FORMAT_IQ_CS8:      return 4;

    case RIG_STREAM_FORMAT_IQ_CU8:      return 5;

    case RIG_STREAM_FORMAT_IQ_CS16:     return 6;

    case RIG_STREAM_FORMAT_IQ_CF32:     return 7;

    default:                            return -1;
    }
}

#define NUM_FORMATS 8

/* Converter function: takes src, dst, sample_count (not counting channels). */
typedef void (*convert_fn)(const void *src, void *dst, size_t n);

/* Direct converter table. NULL entries use float fallback. */
static const convert_fn direct_table[NUM_FORMATS][NUM_FORMATS] =
{
    /* From S8 (index 0) */
    [0] = {
        [1] = convert_s8_to_u8,                     /* S8 -> U8 */
        [2] = convert_s8_to_s16,                    /* S8 -> S16 */
        [3] = convert_s8_to_f32,                    /* S8 -> F32 */
    },
    /* From U8 (index 1) */
    [1] = {
        [0] = convert_u8_to_s8,                     /* U8 -> S8 */
        [2] = convert_u8_to_s16,                    /* U8 -> S16 */
        [3] = convert_u8_to_f32,                    /* U8 -> F32 */
    },
    /* From S16 (index 2) */
    [2] = {
        [0] = convert_s16_to_s8,                    /* S16 -> S8 */
        [1] = convert_s16_to_u8,                    /* S16 -> U8 */
        [3] = convert_s16_to_f32,                   /* S16 -> F32 */
    },
    /* From F32 (index 3) */
    [3] = {
        [0] = convert_f32_to_s8,                    /* F32 -> S8 */
        [1] = convert_f32_to_u8,                    /* F32 -> U8 */
        [2] = convert_f32_to_s16,                   /* F32 -> S16 */
    },
    /* From CS8 (index 4) */
    [4] = {
        [5] = convert_cs8_to_cu8,                   /* CS8 -> CU8 */
        [6] = convert_cs8_to_cs16,                  /* CS8 -> CS16 */
        [7] = convert_cs8_to_cf32,                  /* CS8 -> CF32 */
    },
    /* From CU8 (index 5) */
    [5] = {
        [4] = convert_cu8_to_cs8,                   /* CU8 -> CS8 */
        [6] = convert_cu8_to_cs16,                  /* CU8 -> CS16 */
        [7] = convert_cu8_to_cf32,                  /* CU8 -> CF32 */
    },
    /* From CS16 (index 6) */
    [6] = {
        [4] = convert_cs16_to_cs8,                  /* CS16 -> CS8 */
        [5] = convert_cs16_to_cu8,                  /* CS16 -> CU8 */
        [7] = convert_cs16_to_cf32,                 /* CS16 -> CF32 */
    },
    /* From CF32 (index 7) */
    [7] = {
        [4] = convert_cf32_to_cs8,                  /* CF32 -> CS8 */
        [5] = convert_cf32_to_cu8,                  /* CF32 -> CU8 */
        [6] = convert_cf32_to_cs16,                 /* CF32 -> CS16 */
    },
};


/* Two-hop fallback: src -> float intermediate -> dst.
 * Audio uses F32 (index 3), I/Q uses CF32 (index 7). */
static int convert_via_float(const void *src, rig_stream_format_t src_fmt,
                             void *dst, rig_stream_format_t dst_fmt,
                             size_t sample_count, int channels)
{
    int si = format_index(src_fmt);
    int di = format_index(dst_fmt);

    if (si < 0 || di < 0)
    {
        return -1;
    }

    /* Pick float intermediate based on format family */
    int float_idx;

    if (is_iq_format(src_fmt))
    {
        float_idx = 7;   /* CF32 */
    }
    else
    {
        float_idx = 3;   /* F32 */
    }

    convert_fn to_float = direct_table[si][float_idx];
    convert_fn from_float = direct_table[float_idx][di];

    if (!to_float || !from_float)
    {
        return -1;
    }

    /* Process in chunks using a stack buffer to avoid heap allocation. */
    size_t total = sample_count * channels;

    /* I/Q samples produce 2 floats each; audio samples produce 1 */
    int iq_mult = is_iq_format(src_fmt) ? 2 : 1;
    size_t chunk_samples = STREAM_CONVERT_FLOAT_CHUNK / iq_mult;

    int src_bps = rig_stream_format_sample_size(src_fmt);
    int dst_bps = rig_stream_format_sample_size(dst_fmt);

    if (src_bps <= 0 || dst_bps <= 0)
    {
        return -1;
    }

    float tmp[STREAM_CONVERT_FLOAT_CHUNK];
    const unsigned char *sp = (const unsigned char *)src;
    unsigned char *dp = (unsigned char *)dst;

    while (total > 0)
    {
        size_t batch = (total < chunk_samples) ? total : chunk_samples;
        to_float(sp, tmp, batch);
        from_float(tmp, dp, batch);
        sp += batch * src_bps;
        dp += batch * dst_bps;
        total -= batch;
    }

    return 0;
}


/* ------------------------------------------------------------------ */
/* rig_stream_convert — main dispatch                                  */
/* ------------------------------------------------------------------ */

int rig_stream_convert(const void *src, rig_stream_format_t src_format,
                       void *dst, rig_stream_format_t dst_format,
                       size_t sample_count, int channels)
{
    if (src == NULL || dst == NULL)
    {
        return -1;
    }

    if (channels <= 0)
    {
        return -1;
    }

    /* Guard against size_t overflow in total sample calculation */
    if (sample_count > 0 && (size_t)channels > SIZE_MAX / sample_count)
    {
        return -1;
    }

    /* Reject audio <-> I/Q cross-conversion */
    if (is_iq_format(src_format) != is_iq_format(dst_format))
    {
        return -1;
    }

    /* Same format: memcpy */
    if (src_format == dst_format)
    {
        int ss = rig_stream_format_sample_size(src_format);

        if (ss == 0)
        {
            return -1;
        }

        memcpy(dst, src, sample_count * channels * ss);
        return 0;
    }

    int si = format_index(src_format);
    int di = format_index(dst_format);

    if (si < 0 || di < 0)
    {
        return -1;
    }

    /* Check direct converter */
    convert_fn fn = direct_table[si][di];

    if (fn)
    {
        fn(src, dst, sample_count * channels);
        return 0;
    }

    /* Fallback: two-hop via float intermediate */
    return convert_via_float(src, src_format, dst, dst_format,
                             sample_count, channels);
}


/* ------------------------------------------------------------------ */
/* rig_stream_convert_channels — mono/stereo conversion                */
/* ------------------------------------------------------------------ */

int rig_stream_convert_channels(const void *src, int src_channels,
                                void *dst, int dst_channels,
                                size_t sample_count,
                                rig_stream_format_t format)
{
    if (src == NULL || dst == NULL)
    {
        return -1;
    }

    if (src_channels == dst_channels)
    {
        return -1;
    }

    if ((src_channels != 1 && src_channels != 2)
            || (dst_channels != 1 && dst_channels != 2))
    {
        return -1;
    }

    int ss = rig_stream_format_sample_size(format);

    if (ss == 0)
    {
        return -1;
    }

    /* Mono -> Stereo: duplicate each sample */
    if (src_channels == 1 && dst_channels == 2)
    {
        const uint8_t *s = src;
        uint8_t *d = dst;

        for (size_t i = 0; i < sample_count; i++)
        {
            memcpy(d + 2 * i * ss, s + i * ss, ss);
            memcpy(d + (2 * i + 1) * ss, s + i * ss, ss);
        }

        return 0;
    }

    /* Stereo -> Mono: average L and R */
    if (format == RIG_STREAM_FORMAT_PCM_S16)
    {
        const int16_t *s = src;
        int16_t *d = dst;

        for (size_t i = 0; i < sample_count; i++)
        {
            int32_t l = s[2 * i];
            int32_t r = s[2 * i + 1];
            d[i] = (int16_t)((l + r) >> 1);
        }
    }
    else if (format == RIG_STREAM_FORMAT_PCM_S8)
    {
        const int8_t *s = src;
        int8_t *d = dst;

        for (size_t i = 0; i < sample_count; i++)
        {
            int16_t l = s[2 * i];
            int16_t r = s[2 * i + 1];
            d[i] = (int8_t)((l + r) >> 1);
        }
    }
    else if (format == RIG_STREAM_FORMAT_PCM_U8)
    {
        const uint8_t *s = src;
        uint8_t *d = dst;

        for (size_t i = 0; i < sample_count; i++)
        {
            uint16_t l = s[2 * i];
            uint16_t r = s[2 * i + 1];
            d[i] = (uint8_t)((l + r) >> 1);
        }
    }
    else if (format == RIG_STREAM_FORMAT_PCM_F32)
    {
        const float *s = src;
        float *d = dst;

        for (size_t i = 0; i < sample_count; i++)
        {
            d[i] = (s[2 * i] + s[2 * i + 1]) * 0.5f;
        }
    }
    else
    {
        return -1;  /* Unsupported format for channel conversion */
    }

    return 0;
}


/* ------------------------------------------------------------------ */
/* rig_stream_resample — sample rate conversion (requires libsamplerate) */
/* ------------------------------------------------------------------ */

#ifdef HAVE_SAMPLERATE
#include <samplerate.h>

/* Map Hamlib quality constants to libsamplerate converter types. */
static int resample_converter_type(int quality)
{
    switch (quality)
    {
    case RIG_RESAMPLE_BEST:   return SRC_SINC_BEST_QUALITY;

    case RIG_RESAMPLE_MEDIUM: return SRC_SINC_MEDIUM_QUALITY;

    case RIG_RESAMPLE_FAST:   return SRC_SINC_FASTEST;

    default:                  return SRC_SINC_FASTEST;
    }
}

int rig_stream_resample(const float *src, int src_rate,
                        float *dst, int dst_rate,
                        size_t src_samples, size_t *dst_samples,
                        int channels, int quality)
{
    double ratio = (double)dst_rate / (double)src_rate;
    SRC_DATA data;

    data.data_in = (float *)(uintptr_t)src;  /* cast for libsamplerate < 0.2.0 */
    data.input_frames = (long)src_samples;
    data.data_out = dst;
    data.output_frames = (long)(*dst_samples);
    data.src_ratio = ratio;
    data.end_of_input = 0;

    int err = src_simple(&data, resample_converter_type(quality), channels);

    if (err)
    {
        return -1;
    }

    *dst_samples = (size_t)data.output_frames_gen;
    return 0;
}

#else /* !HAVE_SAMPLERATE */

int rig_stream_resample(const float *src, int src_rate,
                        float *dst, int dst_rate,
                        size_t src_samples, size_t *dst_samples,
                        int channels, int quality)
{
    (void)src;
    (void)src_rate;
    (void)dst;
    (void)dst_rate;
    (void)src_samples;
    (void)dst_samples;
    (void)channels;
    (void)quality;
    return -1;  /* libsamplerate not available */
}

#endif /* HAVE_SAMPLERATE */


/* ------------------------------------------------------------------ */
/* Persistent per-stream conversion context (stream_conv)              */
/* ------------------------------------------------------------------ */

/* Frames processed per pipeline pass; bounds the scratch buffers while
 * arbitrarily large writes stream through in slices. */
#define STREAM_CONV_CHUNK_FRAMES 1024

struct stream_conv
{
    rig_stream_format_t src_fmt, dst_fmt;
    int src_rate, dst_rate;
    int src_ch, dst_ch;
    int is_iq;
    int src_frame_bytes;            /* src_ch x src sample size */
    int dst_frame_bytes;            /* dst_ch x dst sample size */
    rig_stream_format_t pivot_fmt;  /* PCM_F32 or IQ_CF32 when resampling */
    size_t out_cap_frames;          /* scratch capacity in output frames */
    unsigned char *buf_a;           /* ping-pong scratch */
    unsigned char *buf_b;
    size_t buf_bytes;
#ifdef HAVE_SAMPLERATE
    SRC_STATE *src_state;           /* stateful resampler, NULL if no rate conv */
#endif
};

/* Select the first dst_ch of src_ch interleaved per-frame elements
 * (audio samples or I/Q complex pairs), element size from format. */
static void conv_channels_subset(const unsigned char *src, int src_ch,
                                 unsigned char *dst, int dst_ch,
                                 size_t frames, int elem_size)
{
    size_t src_stride = (size_t)src_ch * elem_size;
    size_t dst_stride = (size_t)dst_ch * elem_size;

    for (size_t i = 0; i < frames; i++)
    {
        memcpy(dst + i * dst_stride, src + i * src_stride, dst_stride);
    }
}

/* Channel-map frames of fmt data from src_ch to dst_ch per the design
 * rules (audio: 1<->2 map, first-N subset; I/Q: subset only).
 * Returns 0 on success. */
static int conv_channel_map(const struct stream_conv *c,
                            const unsigned char *src, unsigned char *dst,
                            size_t frames)
{
    int elem = rig_stream_format_sample_size(c->src_fmt);

    if (!c->is_iq && c->src_ch == 1 && c->dst_ch == 2)
    {
        return rig_stream_convert_channels(src, 1, dst, 2, frames,
                                           c->src_fmt);
    }

    if (!c->is_iq && c->src_ch == 2 && c->dst_ch == 1)
    {
        return rig_stream_convert_channels(src, 2, dst, 1, frames,
                                           c->src_fmt);
    }

    if (c->dst_ch < c->src_ch)
    {
        conv_channels_subset(src, c->src_ch, dst, c->dst_ch, frames, elem);
        return 0;
    }

    return -1;
}

int stream_conv_init(struct stream_conv **out,
                     rig_stream_format_t src_fmt, int src_rate, int src_ch,
                     rig_stream_format_t dst_fmt, int dst_rate, int dst_ch,
                     int is_iq, int quality)
{
    if (!out || src_rate <= 0 || dst_rate <= 0 || src_ch <= 0 || dst_ch <= 0)
    {
        return -1;
    }

    int src_ss = rig_stream_format_sample_size(src_fmt);
    int dst_ss = rig_stream_format_sample_size(dst_fmt);

    if (src_ss <= 0 || dst_ss <= 0)
    {
        return -1;  /* compressed/unknown formats are not convertible */
    }

#ifndef HAVE_SAMPLERATE

    if (src_rate != dst_rate)
    {
        return -1;  /* rate conversion requires libsamplerate */
    }

#endif

    struct stream_conv *c = calloc(1, sizeof(*c));

    if (!c)
    {
        return -1;
    }

    c->src_fmt = src_fmt;
    c->dst_fmt = dst_fmt;
    c->src_rate = src_rate;
    c->dst_rate = dst_rate;
    c->src_ch = src_ch;
    c->dst_ch = dst_ch;
    c->is_iq = is_iq;
    c->src_frame_bytes = src_ch * src_ss;
    c->dst_frame_bytes = dst_ch * dst_ss;
    c->pivot_fmt = is_iq ? RIG_STREAM_FORMAT_IQ_CF32
                   : RIG_STREAM_FORMAT_PCM_F32;

    /* Output frames a full input chunk can produce (+margin for the
     * resampler's internal state). Source selection keeps dst <= src rate,
     * but size for either direction to stay safe. */
    double ratio = (double)dst_rate / (double)src_rate;
    c->out_cap_frames = (size_t)((double)STREAM_CONV_CHUNK_FRAMES
                                 * (ratio > 1.0 ? ratio : 1.0)) + 64;

    /* Scratch sized for the widest stage across the whole pipeline. */
    int pivot_ss = rig_stream_format_sample_size(c->pivot_fmt);
    size_t worst_frame = (size_t)src_ch * src_ss;

    if ((size_t)src_ch * pivot_ss > worst_frame)
    {
        worst_frame = (size_t)src_ch * pivot_ss;
    }

    if ((size_t)dst_ch * pivot_ss > worst_frame)
    {
        worst_frame = (size_t)dst_ch * pivot_ss;
    }

    if ((size_t)c->dst_frame_bytes > worst_frame)
    {
        worst_frame = c->dst_frame_bytes;
    }

    size_t worst_frames = STREAM_CONV_CHUNK_FRAMES > c->out_cap_frames
                          ? STREAM_CONV_CHUNK_FRAMES : c->out_cap_frames;
    c->buf_bytes = worst_frames * worst_frame;
    c->buf_a = malloc(c->buf_bytes);
    c->buf_b = malloc(c->buf_bytes);

    if (!c->buf_a || !c->buf_b)
    {
        stream_conv_free(c);
        return -1;
    }

#ifdef HAVE_SAMPLERATE

    if (src_rate != dst_rate)
    {
        /* Interleaved I/Q resamples as 2 float channels per I/Q channel:
         * I and Q pass through identical filters, preserving the complex
         * signal. */
        int rs_channels = is_iq ? 2 * dst_ch : dst_ch;
        int err = 0;

        c->src_state = src_new(resample_converter_type(quality),
                               rs_channels, &err);

        if (!c->src_state)
        {
            stream_conv_free(c);
            return -1;
        }
    }

#endif

    *out = c;
    return 0;
}

void stream_conv_free(struct stream_conv *c)
{
    if (!c)
    {
        return;
    }

#ifdef HAVE_SAMPLERATE

    if (c->src_state)
    {
        src_delete(c->src_state);
    }

#endif
    free(c->buf_a);
    free(c->buf_b);
    free(c);
}

/* Convert one slice of in_frames source frames and deliver the result to
 * sink. Returns output bytes accepted by the sink, sets *out_bytes_total
 * to the bytes offered, or returns (size_t)-1 on conversion error. */
static size_t conv_run_slice(struct stream_conv *c, const unsigned char *in,
                             size_t in_frames, stream_conv_sink_fn sink,
                             void *ctx, size_t *out_bytes_total)
{
    /* Ping-pong: each stage reads cur and writes other, then swaps. The
     * external input is never written, so the first stage lands in buf_a. */
    const unsigned char *cur = in;
    unsigned char *other = c->buf_a;
    rig_stream_format_t cur_fmt = c->src_fmt;
    int cur_ch = c->src_ch;
    size_t frames = in_frames;

    /* Stage 1: channel map (in source format). */
    if (c->dst_ch != c->src_ch)
    {
        if (conv_channel_map(c, cur, other, frames) != 0)
        {
            return (size_t)-1;
        }

        cur = other;
        other = (other == c->buf_a) ? c->buf_b : c->buf_a;
        cur_ch = c->dst_ch;
    }

    int resampling = c->src_rate != c->dst_rate;

    /* Stage 2: convert to the pivot (resampling) or the destination. */
    rig_stream_format_t target = resampling ? c->pivot_fmt : c->dst_fmt;

    if (cur_fmt != target)
    {
        /* I/Q converts as a flat run of complex samples. */
        size_t nsamp = c->is_iq ? frames * cur_ch : frames;
        int conv_ch = c->is_iq ? 1 : cur_ch;

        if (rig_stream_convert(cur, cur_fmt, other, target,
                               nsamp, conv_ch) != 0)
        {
            return (size_t)-1;
        }

        cur = other;
        other = (other == c->buf_a) ? c->buf_b : c->buf_a;
        cur_fmt = target;
    }

#ifdef HAVE_SAMPLERATE

    /* Stage 3: stateful resample on the float pivot (I/Q runs as 2 float
     * channels per I/Q channel, fixed at src_new time). */
    if (resampling)
    {
        SRC_DATA data;

        data.data_in = (float *)(uintptr_t)cur;
        data.input_frames = (long)frames;
        data.data_out = (float *)other;
        data.output_frames = (long)c->out_cap_frames;
        data.src_ratio = (double)c->dst_rate / (double)c->src_rate;
        data.end_of_input = 0;

        if (src_process(c->src_state, &data) != 0)
        {
            return (size_t)-1;
        }

        cur = other;
        other = (other == c->buf_a) ? c->buf_b : c->buf_a;
        frames = (size_t)data.output_frames_gen;

        /* Stage 4: pivot -> destination format. */
        if (c->dst_fmt != cur_fmt)
        {
            size_t nsamp = c->is_iq ? frames * cur_ch : frames;
            int conv_ch = c->is_iq ? 1 : cur_ch;

            if (rig_stream_convert(cur, cur_fmt, other, c->dst_fmt,
                                   nsamp, conv_ch) != 0)
            {
                return (size_t)-1;
            }

            cur = other;
        }
    }

#endif /* HAVE_SAMPLERATE */

    size_t out_bytes = frames * c->dst_frame_bytes;
    *out_bytes_total = out_bytes;

    if (out_bytes == 0)
    {
        return 0;  /* resampler primed but produced nothing yet */
    }

    return sink(ctx, cur, out_bytes);
}

ssize_t stream_conv_process(struct stream_conv *c, const void *buf,
                            size_t len, stream_conv_sink_fn sink, void *ctx)
{
    if (!c || !buf || !sink || len % c->src_frame_bytes != 0)
    {
        return -1;
    }

    const unsigned char *in = buf;
    size_t frames_left = len / c->src_frame_bytes;
    size_t consumed_frames = 0;

    while (frames_left > 0)
    {
        size_t slice = frames_left > STREAM_CONV_CHUNK_FRAMES
                       ? STREAM_CONV_CHUNK_FRAMES : frames_left;
        size_t offered = 0;
        size_t accepted = conv_run_slice(c, in, slice, sink, ctx, &offered);

        if (accepted == (size_t)-1)
        {
            return -1;
        }

        if (accepted < offered)
        {
            /* Sink stopped early (ring full / timeout): credit the input
             * proportionally so the caller can report partial progress. */
            consumed_frames += offered != 0
                               ? slice * accepted / offered : 0;
            return (ssize_t)(consumed_frames * c->src_frame_bytes);
        }

        consumed_frames += slice;
        in += slice * c->src_frame_bytes;
        frames_left -= slice;
    }

    return (ssize_t)(consumed_frames * c->src_frame_bytes);
}


