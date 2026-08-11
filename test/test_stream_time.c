/*
 *  Hamlib streaming time-model tests
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

/* Unit tests for streaming time conversion and interpolation helpers. */
/* Covers ns/timespec round-trips, normalization, and sample interpolation. */

#include "acutest.h"
#include "test_debug.h"

#include <stdint.h>
#include <time.h>

#include "stream_time.h"


static void test_from_ns_positive(void)
{
    int64_t sec;
    uint64_t ps;

    stream_time_from_ns(1500000000LL, &sec, &ps);
    TEST_CHECK(sec == 1);
    TEST_CHECK(ps == 500000000000ULL);

    stream_time_from_ns(0, &sec, &ps);
    TEST_CHECK(sec == 0);
    TEST_CHECK(ps == 0);

    stream_time_from_ns(999999999LL, &sec, &ps);
    TEST_CHECK(sec == 0);
    TEST_CHECK(ps == 999999999000ULL);
}


static void test_from_ns_negative(void)
{
    int64_t sec;
    uint64_t ps;

    /* Floor semantics: -1 ns is 1 ns before the epoch */
    stream_time_from_ns(-1, &sec, &ps);
    TEST_CHECK(sec == -1);
    TEST_CHECK(ps == 999999999000ULL);

    stream_time_from_ns(-1500000000LL, &sec, &ps);
    TEST_CHECK(sec == -2);
    TEST_CHECK(ps == 500000000000ULL);
}


static void test_to_ns_round_trip(void)
{
    static const int64_t cases[] =
    {
        0, 1, -1, 999999999, 1000000000, -1000000000,
        1500000000, -1500000000, 1736000000123456789LL,
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int64_t sec;
        uint64_t ps;
        stream_time_from_ns(cases[i], &sec, &ps);
        TEST_CHECK_(stream_time_to_ns(sec, ps) == cases[i],
                    "round trip %lld", (long long)cases[i]);
    }
}


static void test_timespec_round_trip(void)
{
    struct timespec in = { .tv_sec = 1736000000, .tv_nsec = 123456789 };
    struct timespec out;
    int64_t sec;
    uint64_t ps;

    stream_time_from_timespec(&in, &sec, &ps);
    TEST_CHECK(sec == 1736000000);
    TEST_CHECK(ps == 123456789000ULL);

    stream_time_to_timespec(sec, ps, &out);
    TEST_CHECK(out.tv_sec == in.tv_sec);
    TEST_CHECK(out.tv_nsec == in.tv_nsec);
}


static void test_normalize_carry(void)
{
    int64_t sec = 10;
    uint64_t ps = 2500000000000ULL;   /* 2.5 s of picoseconds */

    stream_time_normalize(&sec, &ps);
    TEST_CHECK(sec == 12);
    TEST_CHECK(ps == 500000000000ULL);

    sec = 0;
    ps = 999999999999ULL;             /* just below 1 s — unchanged */
    stream_time_normalize(&sec, &ps);
    TEST_CHECK(sec == 0);
    TEST_CHECK(ps == 999999999999ULL);
}


static void test_add_samples_exact_second(void)
{
    int64_t sec = 100;
    uint64_t ps = 0;

    stream_time_add_samples(&sec, &ps, 48000, 48000);
    TEST_CHECK(sec == 101);
    TEST_CHECK(ps == 0);

    stream_time_add_samples(&sec, &ps, 24000, 48000);   /* +0.5 s */
    TEST_CHECK(sec == 101);
    TEST_CHECK(ps == 500000000000ULL);
}


static void test_add_samples_fractional(void)
{
    int64_t sec = 0;
    uint64_t ps = 0;

    /* 1 sample at 192 kHz = 1/192000 s = 5208333.33... ps*10^6
     * → floor(10^12/192000) = 5208333 ps */
    stream_time_add_samples(&sec, &ps, 1, 192000);
    TEST_CHECK(sec == 0);
    TEST_CHECK_(ps == 5208333ULL, "ps=%llu", (unsigned long long)ps);
}


static void test_add_samples_carry(void)
{
    int64_t sec = 5;
    uint64_t ps = 900000000000ULL;    /* 0.9 s */

    stream_time_add_samples(&sec, &ps, 9600, 48000);    /* +0.2 s */
    TEST_CHECK(sec == 6);
    TEST_CHECK(ps == 100000000000ULL);
}


static void test_add_samples_large(void)
{
    int64_t sec = 0;
    uint64_t ps = 0;

    /* 1 hour of samples at 192 kHz: 691,200,000 samples = exactly 3600 s */
    stream_time_add_samples(&sec, &ps, 691200000ULL, 192000);
    TEST_CHECK_(sec == 3600, "sec=%lld", (long long)sec);
    TEST_CHECK(ps == 0);

    /* A week at 2 MHz: 1,209,600,000,000 samples = exactly 604800 s */
    sec = 0;
    ps = 0;
    stream_time_add_samples(&sec, &ps, 1209600000000ULL, 2000000);
    TEST_CHECK_(sec == 604800, "sec=%lld", (long long)sec);
    TEST_CHECK(ps == 0);
}


static void test_add_samples_zero_rate(void)
{
    int64_t sec = 7;
    uint64_t ps = 42;

    /* Rate 0 must not divide by zero; time unchanged */
    stream_time_add_samples(&sec, &ps, 1000, 0);
    TEST_CHECK(sec == 7);
    TEST_CHECK(ps == 42);
}


static void test_diff_ms(void)
{
    /* 1.5 s vs 0.2 s → 1300 ms */
    TEST_CHECK(stream_time_diff_ms(1, 500000000000ULL,
                                   0, 200000000000ULL) == 1300);

    /* negative direction */
    TEST_CHECK(stream_time_diff_ms(0, 200000000000ULL,
                                   1, 500000000000ULL) == -1300);

    /* sub-millisecond differences truncate toward zero */
    TEST_CHECK(stream_time_diff_ms(10, 999000000ULL,
                                   10, 0) == 0);
}


static void test_now_wallclock_sanity(void)
{
    int64_t sec;
    uint64_t ps;

    stream_time_now(&sec, &ps);

    /* After 2020-01-01 (1577836800) and before 2100 (4102444800) */
    TEST_CHECK_(sec > 1577836800LL && sec < 4102444800LL,
                "sec=%lld", (long long)sec);
    TEST_CHECK(ps < 1000000000000ULL);
}


TEST_LIST =
{
    { "from_ns_positive",        test_from_ns_positive },
    { "from_ns_negative",        test_from_ns_negative },
    { "to_ns_round_trip",        test_to_ns_round_trip },
    { "timespec_round_trip",     test_timespec_round_trip },
    { "normalize_carry",         test_normalize_carry },
    { "add_samples_exact_second", test_add_samples_exact_second },
    { "add_samples_fractional",  test_add_samples_fractional },
    { "add_samples_carry",       test_add_samples_carry },
    { "add_samples_large",       test_add_samples_large },
    { "add_samples_zero_rate",   test_add_samples_zero_rate },
    { "diff_ms",                 test_diff_ms },
    { "now_sanity",              test_now_wallclock_sanity },
    { NULL, NULL }
};
