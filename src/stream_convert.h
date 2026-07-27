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

/* Run the channel -> format -> optional-resample conversion pipeline.
 * work_a initially holds src_samples (per-channel) of src_fmt/src_channels
 * data; work_a and work_b are caller-provided non-overlapping scratch
 * buffers each at least work_size bytes. is_iq selects I/Q sample geometry.
 * On success returns a pointer to whichever buffer holds the result and sets
 * *out_samples to the per-channel output sample count; returns NULL on any
 * conversion error or if a stage would exceed work_size. Resampling I/Q
 * (is_iq with src_rate != dst_rate) is unsupported and returns NULL; open I/Q
 * streams at a matched rate. */
const void *rig_stream_convert_pipeline(
    void *work_a, void *work_b, size_t work_size,
    rig_stream_format_t src_fmt, int src_channels, int src_rate,
    size_t src_samples, int is_iq,
    rig_stream_format_t dst_fmt, int dst_channels, int dst_rate,
    int resample_quality, size_t *out_samples);

#endif /* HAMLIB_STREAM_CONVERT_H */
