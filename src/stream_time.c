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

/* Time conversion and sample-clock arithmetic for the streaming time model. */
/* Exact integer math between ns/timespec/sample counts and sec+picoseconds. */

#include "stream_time.h"

#define NS_PER_SEC 1000000000LL


void stream_time_normalize(int64_t *seconds, uint64_t *picoseconds)
{
    if (*picoseconds >= RIG_STREAM_PS_PER_SEC)
    {
        *seconds += (int64_t)(*picoseconds / RIG_STREAM_PS_PER_SEC);
        *picoseconds %= RIG_STREAM_PS_PER_SEC;
    }
}


void stream_time_from_ns(int64_t ns, int64_t *seconds, uint64_t *picoseconds)
{
    int64_t sec = ns / NS_PER_SEC;
    int64_t rem = ns % NS_PER_SEC;

    if (rem < 0)
    {
        sec--;
        rem += NS_PER_SEC;
    }

    *seconds = sec;
    *picoseconds = (uint64_t)rem * RIG_STREAM_PS_PER_NS;
}


int64_t stream_time_to_ns(int64_t seconds, uint64_t picoseconds)
{
    return seconds * NS_PER_SEC
           + (int64_t)(picoseconds / RIG_STREAM_PS_PER_NS);
}


void stream_time_from_timespec(const struct timespec *ts,
                               int64_t *seconds, uint64_t *picoseconds)
{
    *seconds = (int64_t)ts->tv_sec;
    *picoseconds = (uint64_t)ts->tv_nsec * RIG_STREAM_PS_PER_NS;
}


void stream_time_to_timespec(int64_t seconds, uint64_t picoseconds,
                             struct timespec *ts)
{
    ts->tv_sec = (time_t)seconds;
    ts->tv_nsec = (long)(picoseconds / RIG_STREAM_PS_PER_NS);
}


void stream_time_now(int64_t *seconds, uint64_t *picoseconds)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    stream_time_from_timespec(&ts, seconds, picoseconds);
}


void stream_time_add_samples(int64_t *seconds, uint64_t *picoseconds,
                             uint64_t samples, uint32_t sample_rate)
{
    if (sample_rate == 0 || samples == 0)
    {
        return;
    }

    uint64_t whole_sec = samples / sample_rate;
    uint64_t rem = samples % sample_rate;
    uint64_t ps_add;

#ifdef __SIZEOF_INT128__
    ps_add = (uint64_t)((unsigned __int128)rem * RIG_STREAM_PS_PER_SEC
                        / sample_rate);
#else
    /* Exact two-stage floor division: rem*10^12/rate without overflow.
     * rem < rate <= 2^32, so rem*10^6 and the partial terms fit in 64 bits. */
    {
        uint64_t t = rem * 1000000ULL;
        uint64_t q1 = t / sample_rate;
        uint64_t r1 = t % sample_rate;
        ps_add = q1 * 1000000ULL + r1 * 1000000ULL / sample_rate;
    }
#endif

    *seconds += (int64_t)whole_sec;
    *picoseconds += ps_add;
    stream_time_normalize(seconds, picoseconds);
}


int64_t stream_time_diff_ms(int64_t a_sec, uint64_t a_ps,
                            int64_t b_sec, uint64_t b_ps)
{
    /* Each ps operand truncates to whole ms, so the result is exact to
     * +/-1 ms — sufficient for watchdog staleness and timed-TX gating. */
    return (a_sec - b_sec) * 1000
           + (int64_t)(a_ps / RIG_STREAM_PS_PER_MS)
           - (int64_t)(b_ps / RIG_STREAM_PS_PER_MS);
}
