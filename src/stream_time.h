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

/* Time conversion helpers for the streaming time model. */
/* Converts between ns/timespec and (UTC seconds, picoseconds) anchor time. */

#ifndef HAMLIB_STREAM_TIME_H
#define HAMLIB_STREAM_TIME_H

#include <stdint.h>
#include <time.h>

#define RIG_STREAM_PS_PER_SEC 1000000000000ULL
#define RIG_STREAM_PS_PER_NS  1000ULL
#define RIG_STREAM_PS_PER_MS  1000000000ULL

/* Carry overflowing picoseconds into seconds so 0 <= *ps < PS_PER_SEC. */
void stream_time_normalize(int64_t *seconds, uint64_t *picoseconds);

/* Nanoseconds since the Unix epoch <-> (seconds, picoseconds).
 * Negative ns use floor semantics: -1 ns -> (-1 s, 999999999000 ps). */
void stream_time_from_ns(int64_t ns, int64_t *seconds, uint64_t *picoseconds);
int64_t stream_time_to_ns(int64_t seconds, uint64_t picoseconds);

/* struct timespec <-> (seconds, picoseconds) */
void stream_time_from_timespec(const struct timespec *ts,
                               int64_t *seconds, uint64_t *picoseconds);
void stream_time_to_timespec(int64_t seconds, uint64_t picoseconds,
                             struct timespec *ts);

/* Current host CLOCK_REALTIME. */
void stream_time_now(int64_t *seconds, uint64_t *picoseconds);

/* Advance a time by `samples` periods of `sample_rate` Hz.
 * Exact integer math (floor to 1 ps); rate 0 leaves the time unchanged. */
void stream_time_add_samples(int64_t *seconds, uint64_t *picoseconds,
                             uint64_t samples, uint32_t sample_rate);

/* Signed difference (a - b) in milliseconds, truncated toward zero. */
int64_t stream_time_diff_ms(int64_t a_sec, uint64_t a_ps,
                            int64_t b_sec, uint64_t b_ps);

#endif /* HAMLIB_STREAM_TIME_H */
