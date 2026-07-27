/*
 *  Hamlib streaming test tool helpers
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

/* Hardware-independent helpers for the rigstreamtest tool: test-tone
 * synthesis, WAV file output, and issue-tally accounting. */
/* Declared separately so unit tests can exercise them without a live rig. */

#ifndef RIGSTREAMTEST_UTIL_H
#define RIGSTREAMTEST_UTIL_H

#include <stdint.h>
#include <stdio.h>

/* Per-direction backend problem counters accumulated over a run. gaps_unknown
 * is a subset of gaps, and dropped_* are sample totals of the same events. */
struct dir_stats
{
    uint64_t phases;
    uint64_t phase_failures;
    uint64_t bytes;
    uint64_t gaps, gaps_unknown, overruns, underruns, link_loss;
    uint64_t tx_late, remote_overruns, remote_underruns, write_events_dropped;
    uint64_t dropped_gap, dropped_overrun, dropped_link;
};

/* Categorised tool-level issue counters (separate from backend stats). */
struct err_tally
{
    uint64_t open_fail;
    uint64_t open_retry;
    uint64_t read_err;
    uint64_t write_err;
    uint64_t ptt_fail;
    uint64_t power_fail;
    uint64_t starvation;
    uint64_t short_write;
};

/* Stream-side problem events for one direction: gaps, ring over/underruns,
 * app-link loss, late TX bursts, and server-reported losses. gaps_unknown is a
 * subset of gaps, and dropped_* are sample totals of the same events, so neither
 * is added again. Every one of these means data was lost or a deadline missed. */
uint64_t dir_issues(const struct dir_stats *d);

/* Combined issue count across both directions plus the tool-level tally; the
 * value that drives the process exit code (non-zero means something went
 * wrong). open_retry is a recovered transient, so it is not counted. */
uint64_t total_issues(const struct dir_stats *rx, const struct dir_stats *tx,
                      const struct err_tally *e);

/* Generate a 1kHz sine wave, the same value across all `channels` channels. */
void generate_tone(float *buf, int frames, int sample_rate,
                   int channels, uint64_t *phase_counter);

/* Interleaved I/Q float32: analytic tone at 1 kHz (low level). */
void generate_iq_tone(float *buf, int iq_pairs, int sample_rate,
                      uint64_t *phase_counter);

/* Write a 16-bit PCM WAV header. Data size is updated at close. */
int wav_write_header(FILE *fp, int sample_rate, int channels);

/* Update the RIFF and data size fields after writing is complete. */
void wav_finalize(FILE *fp, uint32_t data_bytes);

/* Convert a float32 sample buffer to clamped int16 and append it to a WAV
 * file, advancing the running data-byte count. */
void wav_append_f32(FILE *fp, const float *buf, int num_samples,
                    uint32_t *data_bytes);

#endif /* RIGSTREAMTEST_UTIL_H */
