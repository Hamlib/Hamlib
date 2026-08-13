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

/* Sample format conversion utilities for the Hamlib streaming subsystem. */
/* Handles PCM and I/Q sample-format, channel, and rate conversions; */
/* compressed codecs are handled separately in stream_codec. */
/* Samples are processed in host byte order; the little-endian wire payload
 * requires a little-endian host. */

#ifndef HAMLIB_STREAM_CONVERT_H
#define HAMLIB_STREAM_CONVERT_H

#include <hamlib/rig.h>
#include <stddef.h>
#include <sys/types.h>  /* ssize_t (provided by mingw-w64 as well) */


/* Returns the size in bytes of a single sample for the given format.
 * For I/Q formats, one sample is one complex pair (I+Q).
 * Returns 0 for unknown/compressed formats (Opus). */
int rig_stream_format_sample_size(rig_stream_format_t format);

/* Convert between audio/I/Q sample formats.
 * src and dst may not overlap.
 * sample_count is samples per channel (or complex pairs for I/Q).
 * channels is independent streams (usually 1 for I/Q, 1-2 for audio).
 * Returns 0 on success, -1 on NULL src/dst, bad channels, or an
 * unsupported conversion. */
int rig_stream_convert(const void *src, rig_stream_format_t src_format,
                       void *dst, rig_stream_format_t dst_format,
                       size_t sample_count, int channels);

/* Convert between mono and stereo audio.
 * sample_count is samples per channel in the source.
 * format determines sample size and averaging method.
 * Mono->stereo: duplicates each sample.
 * Stereo->mono: averages L and R with overflow protection.
 * Returns 0 on success, -1 if channels match or unsupported count. */
int rig_stream_convert_channels(const void *src, int src_channels,
                                void *dst, int dst_channels,
                                size_t sample_count,
                                rig_stream_format_t format);

/* Resample quality levels for rig_stream_resample(). */
#define RIG_RESAMPLE_BEST    0  /* Highest quality SINC interpolation */
#define RIG_RESAMPLE_MEDIUM  1  /* Balanced quality/speed SINC */
#define RIG_RESAMPLE_FAST    2  /* Fastest SINC interpolation */

/* Resample audio data (F32LE only).
 * src_samples is per channel. *dst_samples set to actual output count.
 * quality is one of RIG_RESAMPLE_BEST, RIG_RESAMPLE_MEDIUM, RIG_RESAMPLE_FAST.
 * Returns 0 on success, -1 if libsamplerate not compiled in. */
int rig_stream_resample(const float *src, int src_rate,
                        float *dst, int dst_rate,
                        size_t src_samples, size_t *dst_samples,
                        int channels, int quality);

/* --- Persistent per-stream conversion context (frontend data path) --- */

/* Opaque context carrying the stream's conversion pipeline state: scratch
 * buffers and, when rates differ, a STATEFUL resampler (src_process), so
 * consecutive calls are seam-free — unlike rig_stream_resample(), whose
 * one-shot src_simple() produces boundary artifacts on continuous data. */
struct stream_conv;

/* Sink for converted output. Returns the number of bytes accepted;
 * returning less than len stops processing (used for ring-full/timeout). */
typedef size_t (*stream_conv_sink_fn)(void *ctx, const void *buf,
                                      size_t len);

/* Create a conversion context from the source (producer-side) stream
 * parameters to the destination (consumer-side) ones. Channel conversion:
 * audio maps 1<->2 (duplicate / average) and selects the first dst_ch
 * channels when narrowing from more; I/Q only selects a subset. Rate
 * conversion requires libsamplerate (fails otherwise) and resamples I/Q
 * as interleaved I/Q float pairs; quality is the RIG_RESAMPLE_* sinc
 * converter to use (ignored without rate conversion). Returns 0 and
 * sets *out, or -1. */
int stream_conv_init(struct stream_conv **out,
                     rig_stream_format_t src_fmt, int src_rate, int src_ch,
                     rig_stream_format_t dst_fmt, int dst_rate, int dst_ch,
                     int is_iq, int quality);

/* Feed whole source-side frames through the pipeline; converted output is
 * delivered to sink in chunks. Returns the number of source bytes
 * consumed (all of len unless the sink stopped early — then a
 * proportional count), or -1 on error. */
ssize_t stream_conv_process(struct stream_conv *c, const void *buf,
                            size_t len, stream_conv_sink_fn sink,
                            void *ctx);

void stream_conv_free(struct stream_conv *c);

#endif /* HAMLIB_STREAM_CONVERT_H */
