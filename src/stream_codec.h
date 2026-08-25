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
/* Converts between device-link codecs (mu-law, A-law, ADPCM) and PCM. */

#ifndef HAMLIB_STREAM_CODEC_H
#define HAMLIB_STREAM_CODEC_H

#include <hamlib/rig.h>
#include <stddef.h>


/* Device audio codecs carried over a radio's device link. These are NOT
 * stream formats (rig_stream_format_t): they are never advertised in
 * stream_caps and never appear on the application-facing wire. A backend
 * terminates the device codec here and presents PCM upward.
 *
 * Each enum value uniquely identifies a bitstream (algorithm + framing).
 * Distinct flavors are distinct values, not a parameter — channel count is
 * the only orthogonal axis and is a separate open() argument. */
typedef enum
{
    RIG_AUDIO_CODEC_NONE = 0,   /* passthrough / no codec */
    RIG_AUDIO_CODEC_MULAW,      /* ITU-T G.711 mu-law (PCMU) */
    RIG_AUDIO_CODEC_ALAW,       /* ITU-T G.711 A-law */
    RIG_AUDIO_CODEC_ADPCM_IMA,  /* IMA/DVI 4-bit ADPCM */
} rig_audio_codec_t;

/* Opaque per-direction codec state. Stateful codecs (ADPCM) hold their running
 * step index here; stateless codecs allocate a trivial state so backends do
 * not branch on whether a codec is stateful. */
struct rig_audio_codec_state;

/* Open a codec state for one direction.
 * channels is the codec's interleave count (1 or 2); the layer preserves it
 * through the codec hop so a stateful stereo codec can hold per-channel
 * state. Returns NULL on an unsupported codec or invalid channel count. */
struct rig_audio_codec_state *
rig_audio_codec_open(rig_audio_codec_t codec, int channels);

/* Reset a codec state to its open-time condition. Call at a stream gap
 * before decoding post-gap data so a predictor does not carry across the
 * discontinuity. Stateless codecs ignore the reset. */
void rig_audio_codec_reset(struct rig_audio_codec_state *st);

/* Free a codec state. NULL is accepted. */
void rig_audio_codec_close(struct rig_audio_codec_state *st);


/* Decode device codec bytes to the caller's PCM format. RX path.
 * src holds src_bytes of codec data; pcm holds up to pcm_cap bytes, written
 * in pcm_format (any PCM rig_stream_format_t). *pcm_bytes receives the bytes
 * written. pcm_format must be a PCM format; I/Q and Opus are rejected.
 * Returns RIG_OK on success, a negative rig_errcode on error. */
int rig_audio_convert_to_pcm(struct rig_audio_codec_state *st,
                             const void *src, size_t src_bytes,
                             rig_stream_format_t pcm_format,
                             void *pcm, size_t pcm_cap, size_t *pcm_bytes);

/* Encode the caller's PCM format to device codec bytes. TX path.
 * pcm holds pcm_bytes in pcm_format; dst holds up to dst_cap codec bytes.
 * *dst_bytes receives the bytes written.
 * Returns RIG_OK on success, a negative rig_errcode on error. */
int rig_audio_convert_from_pcm(struct rig_audio_codec_state *st,
                               rig_stream_format_t pcm_format,
                               const void *pcm, size_t pcm_bytes,
                               void *dst, size_t dst_cap, size_t *dst_bytes);


/* Worst-case output bound for rig_audio_convert_to_pcm(): the largest
 * pcm-byte count src_bytes of codec data can produce in pcm_format.
 * Returns 0 on invalid arguments. */
size_t rig_audio_codec_max_pcm_bytes(struct rig_audio_codec_state *st,
                                     size_t src_bytes,
                                     rig_stream_format_t pcm_format);

/* Worst-case output bound for rig_audio_convert_from_pcm(): the largest
 * codec-byte count pcm_bytes in pcm_format can produce.
 * Returns 0 on invalid arguments. */
size_t rig_audio_codec_max_encoded_bytes(struct rig_audio_codec_state *st,
        size_t pcm_bytes,
        rig_stream_format_t pcm_format);

#endif /* HAMLIB_STREAM_CODEC_H */
