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

/* Device audio codec conversion for the Hamlib streaming subsystem. */
/* Decodes device-link codecs (mu-law, A-law, ADPCM) to PCM and encodes back. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "stream_codec.h"
#include "stream_convert.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/* ------------------------------------------------------------------ */
/* G.711 mu-law / A-law (ITU-T), clean-room from the published spec.   */
/* Both pivot on host-order 16-bit linear, matching stream_convert's   */
/* S16LE handling.                                                     */
/* ------------------------------------------------------------------ */

#define G711_BIAS 0x84   /* 16-bit-scale add-in bias for mu-law */
#define G711_CLIP 32635

/* First segment-end value table search: index of the first entry the
 * value does not exceed, or size if it exceeds all of them. */
static int g711_segment(int value, const int *seg_end, int size)
{
    int i;

    for (i = 0; i < size; i++)
    {
        if (value <= seg_end[i])
        {
            return i;
        }
    }

    return size;
}

static int16_t mulaw_decode_sample(uint8_t code)
{
    code = (uint8_t)~code;

    int t = ((code & 0x0F) << 3) + G711_BIAS;
    t <<= (code & 0x70) >> 4;

    return (int16_t)((code & 0x80) ? (G711_BIAS - t) : (t - G711_BIAS));
}

static uint8_t mulaw_encode_sample(int16_t pcm)
{
    static const int seg_end[8] =
    {
        0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF
    };

    int value = pcm;
    int mask;

    if (value < 0)
    {
        value = G711_BIAS - value;
        mask = 0x7F;
    }
    else
    {
        value = value + G711_BIAS;
        mask = 0xFF;
    }

    if (value > G711_CLIP)
    {
        value = G711_CLIP;
    }

    int seg = g711_segment(value, seg_end, 8);

    if (seg >= 8)
    {
        return (uint8_t)(0x7F ^ mask);
    }

    uint8_t uval = (uint8_t)((seg << 4) | ((value >> (seg + 3)) & 0x0F));
    return (uint8_t)(uval ^ mask);
}

static int16_t alaw_decode_sample(uint8_t code)
{
    code ^= 0x55;

    int t = (code & 0x0F) << 4;
    int seg = (code & 0x70) >> 4;

    switch (seg)
    {
    case 0:
        t += 8;
        break;

    case 1:
        t += 0x108;
        break;

    default:
        t += 0x108;
        t <<= seg - 1;
        break;
    }

    return (int16_t)((code & 0x80) ? t : -t);
}

static uint8_t alaw_encode_sample(int16_t pcm)
{
    static const int seg_end[8] =
    {
        0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF
    };

    int value = pcm >> 3;   /* A-law operates on a 13-bit magnitude */
    int mask;

    if (value >= 0)
    {
        mask = 0xD5;
    }
    else
    {
        mask = 0x55;
        value = -value - 1;
    }

    int seg = g711_segment(value, seg_end, 8);

    if (seg >= 8)
    {
        return (uint8_t)(0x7F ^ mask);
    }

    uint8_t aval = (uint8_t)(seg << 4);

    if (seg < 2)
    {
        aval |= (value >> 1) & 0x0F;
    }
    else
    {
        aval |= (value >> seg) & 0x0F;
    }

    return (uint8_t)(aval ^ mask);
}


/* ------------------------------------------------------------------ */
/* IMA/DVI ADPCM (4-bit), clean-room from the published IMA spec.       */
/* One network audio packet carries one self-contained block: a 4-byte  */
/* preamble (first sample int16 LE, step index, reserved) followed by   */
/* 4-bit nibbles, low nibble first. Mono only (Icom uses 1-channel      */
/* ADPCM). The step index runs continuously across encoded blocks so    */
/* the radio's decoder stays in step.                                   */
/* ------------------------------------------------------------------ */

static const int ima_index_table[16] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const int ima_step_table[89] =
{
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static int ima_clamp_index(int index)
{
    if (index < 0)
    {
        return 0;
    }

    if (index > 88)
    {
        return 88;
    }

    return index;
}

static int16_t ima_clamp_sample(int sample)
{
    if (sample > 32767)
    {
        return 32767;
    }

    if (sample < -32768)
    {
        return -32768;
    }

    return (int16_t)sample;
}

/* Decode one 4-bit code, advancing the predictor and step index. */
static int16_t ima_decode_nibble(int *predictor, int *index, int nibble)
{
    int step = ima_step_table[*index];
    int diff = step >> 3;

    if (nibble & 1)
    {
        diff += step >> 2;
    }

    if (nibble & 2)
    {
        diff += step >> 1;
    }

    if (nibble & 4)
    {
        diff += step;
    }

    if (nibble & 8)
    {
        diff = -diff;
    }

    *predictor = ima_clamp_sample(*predictor + diff);
    *index = ima_clamp_index(*index + ima_index_table[nibble]);
    return (int16_t) * predictor;
}

/* Encode one sample to a 4-bit code, advancing the predictor and index. */
static int ima_encode_sample(int *predictor, int *index, int16_t sample)
{
    int step = ima_step_table[*index];
    int diff = sample - *predictor;
    int nibble = 0;
    int vpdiff = step >> 3;

    if (diff < 0)
    {
        nibble = 8;
        diff = -diff;
    }

    if (diff >= step)
    {
        nibble |= 4;
        diff -= step;
        vpdiff += step;
    }

    if (diff >= (step >> 1))
    {
        nibble |= 2;
        diff -= step >> 1;
        vpdiff += step >> 1;
    }

    if (diff >= (step >> 2))
    {
        nibble |= 1;
        vpdiff += step >> 2;
    }

    *predictor = ima_clamp_sample(*predictor + ((nibble & 8) ? -vpdiff : vpdiff));
    *index = ima_clamp_index(*index + ima_index_table[nibble & 0x0F]);
    return nibble & 0x0F;
}


/* ------------------------------------------------------------------ */
/* Codec state                                                         */
/* ------------------------------------------------------------------ */

struct rig_audio_codec_state
{
    rig_audio_codec_t codec;
    int channels;
    int16_t *scratch;      /* S16 pivot scratch, grown on demand */
    size_t scratch_cap;    /* capacity in samples */
    int adpcm_index;       /* running ADPCM step index (encode side) */
};

/* True for block codecs (ADPCM) whose byte/sample ratio is not constant. */
static int is_adpcm_codec(rig_audio_codec_t codec)
{
    return codec == RIG_AUDIO_CODEC_ADPCM_IMA;
}

/* Bytes of device-codec data per decoded/encoded sample. Block codecs (ADPCM)
 * have no constant ratio and return 0; they are handled separately. */
static int codec_bytes_per_sample(rig_audio_codec_t codec)
{
    switch (codec)
    {
    case RIG_AUDIO_CODEC_NONE:
        return 2;   /* device link carries the S16 pivot directly */

    case RIG_AUDIO_CODEC_MULAW:
    case RIG_AUDIO_CODEC_ALAW:
        return 1;

    default:
        return 0;   /* unsupported / reserved / block codec */
    }
}

static int ensure_scratch(struct rig_audio_codec_state *st, size_t samples);

/* Samples decoded from an ADPCM block: header sample + 2 per following byte. */
static long adpcm_decode_block(struct rig_audio_codec_state *st,
                               const uint8_t *src, size_t src_bytes)
{
    int predictor, index;
    size_t samples, count, i;

    if (src_bytes < 4)
    {
        return -1;
    }

    predictor = (int16_t)(src[0] | (src[1] << 8));
    index = ima_clamp_index(src[2]);
    samples = 1 + (src_bytes - 4) * 2;

    if (ensure_scratch(st, samples) != 0)
    {
        return -1;
    }

    st->scratch[0] = (int16_t)predictor;
    count = 1;

    for (i = 4; i < src_bytes; i++)
    {
        int byte = src[i];
        st->scratch[count++] = ima_decode_nibble(&predictor, &index, byte & 0x0F);
        st->scratch[count++] = ima_decode_nibble(&predictor, &index,
                               (byte >> 4) & 0x0F);
    }

    return (long)count;
}

/* Encode the S16 scratch into one ADPCM block. Returns bytes written, or -1. */
static long adpcm_encode_block(struct rig_audio_codec_state *st,
                               size_t samples, uint8_t *dst, size_t dst_cap)
{
    int predictor, index, hold = 0, have = 0;
    size_t need, out, i;

    if (samples == 0)
    {
        return 0;
    }

    need = 4 + samples / 2;   /* preamble + ceil((samples - 1) / 2) nibble bytes */

    if (dst_cap < need)
    {
        return -1;
    }

    predictor = st->scratch[0];
    index = st->adpcm_index;

    dst[0] = (uint8_t)(predictor & 0xFF);
    dst[1] = (uint8_t)((predictor >> 8) & 0xFF);
    dst[2] = (uint8_t)index;
    dst[3] = 0;
    out = 4;

    for (i = 1; i < samples; i++)
    {
        int nib = ima_encode_sample(&predictor, &index, st->scratch[i]);

        if (!have)
        {
            hold = nib;
            have = 1;
        }
        else
        {
            dst[out++] = (uint8_t)(hold | (nib << 4));
            have = 0;
        }
    }

    if (have)
    {
        dst[out++] = (uint8_t)hold;   /* trailing nibble, high nibble padded */
    }

    st->adpcm_index = index;   /* carry the step index into the next block */
    return (long)out;
}

/* True for a PCM rig_stream_format_t (not I/Q, not Opus, not reserved). */
static int is_pcm_format(rig_stream_format_t fmt)
{
    if (fmt & (RIG_STREAM_FORMAT_IQ_CS8
               | RIG_STREAM_FORMAT_IQ_CS16
               | RIG_STREAM_FORMAT_IQ_CF32
               | RIG_STREAM_FORMAT_IQ_CU8))
    {
        return 0;
    }

    return rig_stream_format_sample_size(fmt) > 0;
}

static int ensure_scratch(struct rig_audio_codec_state *st, size_t samples)
{
    if (samples <= st->scratch_cap)
    {
        return 0;
    }

    int16_t *grown = realloc(st->scratch, samples * sizeof(int16_t));

    if (grown == NULL)
    {
        return -1;
    }

    st->scratch = grown;
    st->scratch_cap = samples;
    return 0;
}


struct rig_audio_codec_state *
rig_audio_codec_open(rig_audio_codec_t codec, int channels)
{
    if (channels != 1 && channels != 2)
    {
        return NULL;
    }

    if (codec_bytes_per_sample(codec) == 0 && !is_adpcm_codec(codec))
    {
        return NULL;   /* unsupported / reserved codec */
    }

    if (is_adpcm_codec(codec) && channels != 1)
    {
        return NULL;   /* IMA ADPCM is implemented for a single channel only */
    }

    struct rig_audio_codec_state *st = calloc(1, sizeof(*st));

    if (st == NULL)
    {
        return NULL;
    }

    st->codec = codec;
    st->channels = channels;
    return st;
}

void rig_audio_codec_reset(struct rig_audio_codec_state *st)
{
    /* mu-law/A-law/NONE are stateless; ADPCM keeps a running step index. */
    if (st != NULL)
    {
        st->adpcm_index = 0;
    }
}

void rig_audio_codec_close(struct rig_audio_codec_state *st)
{
    if (st == NULL)
    {
        return;
    }

    free(st->scratch);
    free(st);
}


/* Decode codec bytes into the S16 scratch. Returns sample count, or -1. */
static long decode_to_scratch(struct rig_audio_codec_state *st,
                              const void *src, size_t src_bytes)
{
    int unit;

    if (is_adpcm_codec(st->codec))
    {
        return adpcm_decode_block(st, src, src_bytes);
    }

    unit = codec_bytes_per_sample(st->codec);

    if (src_bytes % (size_t)unit != 0)
    {
        return -1;
    }

    size_t samples = src_bytes / (size_t)unit;

    if (ensure_scratch(st, samples) != 0)
    {
        return -1;
    }

    const uint8_t *in = src;

    switch (st->codec)
    {
    case RIG_AUDIO_CODEC_NONE:
        memcpy(st->scratch, src, src_bytes);
        break;

    case RIG_AUDIO_CODEC_MULAW:
        for (size_t i = 0; i < samples; i++)
        {
            st->scratch[i] = mulaw_decode_sample(in[i]);
        }

        break;

    case RIG_AUDIO_CODEC_ALAW:
        for (size_t i = 0; i < samples; i++)
        {
            st->scratch[i] = alaw_decode_sample(in[i]);
        }

        break;

    default:
        return -1;
    }

    return (long)samples;
}

/* Encode the S16 scratch (samples) into dst. Returns bytes written, or -1. */
static long encode_from_scratch(struct rig_audio_codec_state *st,
                                size_t samples, void *dst, size_t dst_cap)
{
    int unit;
    size_t need;

    if (is_adpcm_codec(st->codec))
    {
        return adpcm_encode_block(st, samples, dst, dst_cap);
    }

    unit = codec_bytes_per_sample(st->codec);
    need = samples * (size_t)unit;

    if (dst_cap < need)
    {
        return -1;
    }

    uint8_t *out = dst;

    switch (st->codec)
    {
    case RIG_AUDIO_CODEC_NONE:
        memcpy(dst, st->scratch, need);
        break;

    case RIG_AUDIO_CODEC_MULAW:
        for (size_t i = 0; i < samples; i++)
        {
            out[i] = mulaw_encode_sample(st->scratch[i]);
        }

        break;

    case RIG_AUDIO_CODEC_ALAW:
        for (size_t i = 0; i < samples; i++)
        {
            out[i] = alaw_encode_sample(st->scratch[i]);
        }

        break;

    default:
        return -1;
    }

    return (long)need;
}


int rig_audio_convert_to_pcm(struct rig_audio_codec_state *st,
                             const void *src, size_t src_bytes,
                             rig_stream_format_t pcm_format,
                             void *pcm, size_t pcm_cap, size_t *pcm_bytes)
{
    if (st == NULL || src == NULL || pcm == NULL || pcm_bytes == NULL)
    {
        return -RIG_EINVAL;
    }

    if (!is_pcm_format(pcm_format))
    {
        return -RIG_EINVAL;
    }

    long samples = decode_to_scratch(st, src, src_bytes);

    if (samples < 0)
    {
        return -RIG_EINVAL;
    }

    if ((size_t)samples % (size_t)st->channels != 0)
    {
        return -RIG_EINVAL;
    }

    int sample_size = rig_stream_format_sample_size(pcm_format);
    size_t out_bytes = (size_t)samples * (size_t)sample_size;

    if (pcm_cap < out_bytes)
    {
        return -RIG_EINVAL;
    }

    if (rig_stream_convert(st->scratch, RIG_STREAM_FORMAT_PCM_S16,
                           pcm, pcm_format,
                           (size_t)samples / (size_t)st->channels,
                           st->channels) != 0)
    {
        return -RIG_EINVAL;
    }

    *pcm_bytes = out_bytes;
    return RIG_OK;
}

int rig_audio_convert_from_pcm(struct rig_audio_codec_state *st,
                               rig_stream_format_t pcm_format,
                               const void *pcm, size_t pcm_bytes,
                               void *dst, size_t dst_cap, size_t *dst_bytes)
{
    if (st == NULL || pcm == NULL || dst == NULL || dst_bytes == NULL)
    {
        return -RIG_EINVAL;
    }

    if (!is_pcm_format(pcm_format))
    {
        return -RIG_EINVAL;
    }

    int sample_size = rig_stream_format_sample_size(pcm_format);

    if (pcm_bytes % (size_t)sample_size != 0)
    {
        return -RIG_EINVAL;
    }

    size_t samples = pcm_bytes / (size_t)sample_size;

    if (samples % (size_t)st->channels != 0)
    {
        return -RIG_EINVAL;
    }

    if (ensure_scratch(st, samples) != 0)
    {
        return -RIG_EINVAL;
    }

    if (rig_stream_convert(pcm, pcm_format,
                           st->scratch, RIG_STREAM_FORMAT_PCM_S16,
                           samples / (size_t)st->channels,
                           st->channels) != 0)
    {
        return -RIG_EINVAL;
    }

    long written = encode_from_scratch(st, samples, dst, dst_cap);

    if (written < 0)
    {
        return -RIG_EINVAL;
    }

    *dst_bytes = (size_t)written;
    return RIG_OK;
}


size_t rig_audio_codec_max_pcm_bytes(struct rig_audio_codec_state *st,
                                     size_t src_bytes,
                                     rig_stream_format_t pcm_format)
{
    if (st == NULL || !is_pcm_format(pcm_format))
    {
        return 0;
    }

    if (is_adpcm_codec(st->codec))
    {
        size_t samples = src_bytes < 4 ? 0 : 1 + (src_bytes - 4) * 2;
        return samples * (size_t)rig_stream_format_sample_size(pcm_format);
    }

    int unit = codec_bytes_per_sample(st->codec);

    if (unit == 0)
    {
        return 0;
    }

    size_t samples = src_bytes / (size_t)unit;
    return samples * (size_t)rig_stream_format_sample_size(pcm_format);
}

size_t rig_audio_codec_max_encoded_bytes(struct rig_audio_codec_state *st,
        size_t pcm_bytes,
        rig_stream_format_t pcm_format)
{
    if (st == NULL || !is_pcm_format(pcm_format))
    {
        return 0;
    }

    if (is_adpcm_codec(st->codec))
    {
        size_t samples = pcm_bytes
                         / (size_t)rig_stream_format_sample_size(pcm_format);
        return samples == 0 ? 0 : 4 + samples / 2;
    }

    int unit = codec_bytes_per_sample(st->codec);

    if (unit == 0)
    {
        return 0;
    }

    size_t samples = pcm_bytes / (size_t)rig_stream_format_sample_size(pcm_format);
    return samples * (size_t)unit;
}
