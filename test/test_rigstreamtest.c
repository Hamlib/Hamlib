/*
 *  Hamlib rigstreamtest helper tests
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

/* Unit tests for the rigstreamtest tool's hardware-independent helpers. */
/* Covers the issue-tally/exit-code contract, tone synthesis, and WAV output. */

#include "acutest.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "rigstreamtest_util.h"


/* --- Issue tally / exit-code contract --- */

static void test_issues_all_zero(void)
{
    struct dir_stats rx, tx;
    struct err_tally e;
    memset(&rx, 0, sizeof(rx));
    memset(&tx, 0, sizeof(tx));
    memset(&e, 0, sizeof(e));

    TEST_CHECK(dir_issues(&rx) == 0);
    TEST_CHECK(total_issues(&rx, &tx, &e) == 0);
}


static void test_dir_issues_counted_fields(void)
{
    /* Each of these means data was lost or a deadline missed; all count. */
    struct dir_stats d;

#define CHECK_COUNTS(field)                    \
    do {                                       \
        memset(&d, 0, sizeof(d));              \
        d.field = 3;                           \
        TEST_CHECK(dir_issues(&d) == 3);       \
        TEST_MSG("field " #field " should count");  \
    } while (0)

    CHECK_COUNTS(phase_failures);
    CHECK_COUNTS(gaps);
    CHECK_COUNTS(overruns);
    CHECK_COUNTS(underruns);
    CHECK_COUNTS(link_loss);
    CHECK_COUNTS(tx_late);
    CHECK_COUNTS(remote_overruns);
    CHECK_COUNTS(remote_underruns);
    CHECK_COUNTS(write_events_dropped);
#undef CHECK_COUNTS
}


static void test_dir_issues_uncounted_fields(void)
{
    /* bytes/phases are volume, gaps_unknown is a subset of gaps, and dropped_*
     * are sample totals of already-counted events: none add to the tally. */
    struct dir_stats d;

#define CHECK_IGNORED(field)                   \
    do {                                       \
        memset(&d, 0, sizeof(d));              \
        d.field = 7;                           \
        TEST_CHECK(dir_issues(&d) == 0);       \
        TEST_MSG("field " #field " must not count");  \
    } while (0)

    CHECK_IGNORED(phases);
    CHECK_IGNORED(bytes);
    CHECK_IGNORED(gaps_unknown);
    CHECK_IGNORED(dropped_gap);
    CHECK_IGNORED(dropped_overrun);
    CHECK_IGNORED(dropped_link);
#undef CHECK_IGNORED
}


static void test_total_issues_err_fields(void)
{
    struct dir_stats rx, tx;
    struct err_tally e;

#define CHECK_ERR_COUNTS(field, expect)        \
    do {                                       \
        memset(&rx, 0, sizeof(rx));            \
        memset(&tx, 0, sizeof(tx));            \
        memset(&e, 0, sizeof(e));             \
        e.field = 5;                           \
        TEST_CHECK(total_issues(&rx, &tx, &e) == (expect)); \
        TEST_MSG("err field " #field);         \
    } while (0)

    CHECK_ERR_COUNTS(open_fail, 5);
    CHECK_ERR_COUNTS(read_err, 5);
    CHECK_ERR_COUNTS(write_err, 5);
    CHECK_ERR_COUNTS(ptt_fail, 5);
    CHECK_ERR_COUNTS(power_fail, 5);
    CHECK_ERR_COUNTS(starvation, 5);
    CHECK_ERR_COUNTS(short_write, 5);
    /* A retried-then-succeeded open is a recovered transient, not an issue. */
    CHECK_ERR_COUNTS(open_retry, 0);
#undef CHECK_ERR_COUNTS
}


static void test_total_issues_sums_both_directions(void)
{
    struct dir_stats rx, tx;
    struct err_tally e;
    memset(&rx, 0, sizeof(rx));
    memset(&tx, 0, sizeof(tx));
    memset(&e, 0, sizeof(e));

    rx.gaps = 2;
    tx.underruns = 3;
    e.short_write = 4;

    TEST_CHECK(total_issues(&rx, &tx, &e) == 9);
}


/* --- Tone synthesis --- */

static void test_generate_tone_phase_and_channels(void)
{
    float buf[8];
    uint64_t phase = 0;

    generate_tone(buf, 4, 48000, 2, &phase);

    TEST_CHECK(phase == 4);            /* advanced by frame count */
    TEST_CHECK(buf[0] == 0.0f);        /* sin(0) at phase 0 */

    for (int i = 0; i < 4; i++)
    {
        /* Same value written across all channels of a frame. */
        TEST_CHECK(buf[i * 2] == buf[i * 2 + 1]);
        /* Amplitude is 0.5. */
        TEST_CHECK(fabsf(buf[i * 2]) <= 0.5f + 1e-6f);
    }
}


static void test_generate_iq_tone_magnitude(void)
{
    float buf[8];
    uint64_t phase = 0;

    generate_iq_tone(buf, 4, 48000, &phase);

    TEST_CHECK(phase == 4);
    TEST_CHECK(fabsf(buf[0] - 0.0f) < 1e-6f);   /* I = 0.1*sin(0) */
    TEST_CHECK(fabsf(buf[1] - 0.1f) < 1e-6f);   /* Q = 0.1*cos(0) */

    for (int i = 0; i < 4; i++)
    {
        float mag_sq = buf[i * 2] * buf[i * 2]
                       + buf[i * 2 + 1] * buf[i * 2 + 1];
        /* Analytic tone has constant magnitude 0.1 -> power 0.01. */
        TEST_CHECK(fabsf(mag_sq - 0.01f) < 1e-6f);
    }
}


/* --- WAV output --- */

static uint32_t read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16le(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}


static void test_wav_header_fields(void)
{
    FILE *fp = tmpfile();
    TEST_ASSERT(fp != NULL);

    wav_write_header(fp, 44100, 2);

    unsigned char hdr[44];
    rewind(fp);
    TEST_CHECK(fread(hdr, 1, sizeof(hdr), fp) == sizeof(hdr));

    TEST_CHECK(memcmp(hdr + 0, "RIFF", 4) == 0);
    TEST_CHECK(memcmp(hdr + 8, "WAVE", 4) == 0);
    TEST_CHECK(memcmp(hdr + 12, "fmt ", 4) == 0);
    TEST_CHECK(memcmp(hdr + 36, "data", 4) == 0);
    TEST_CHECK(read_u32le(hdr + 16) == 16);     /* fmt chunk size */
    TEST_CHECK(read_u16le(hdr + 20) == 1);      /* PCM */
    TEST_CHECK(read_u16le(hdr + 22) == 2);      /* channels */
    TEST_CHECK(read_u32le(hdr + 24) == 44100);  /* sample rate */
    TEST_CHECK(read_u16le(hdr + 32) == 4);      /* block align = ch*2 */
    TEST_CHECK(read_u16le(hdr + 34) == 16);     /* bits per sample */

    fclose(fp);
}


static void test_wav_append_clamps_and_scales(void)
{
    FILE *fp = tmpfile();
    TEST_ASSERT(fp != NULL);

    const float samples[] = { 0.0f, 0.5f, 2.0f, -2.0f };
    uint32_t data_bytes = 0;

    wav_append_f32(fp, samples, 4, &data_bytes);

    TEST_CHECK(data_bytes == 8);   /* 4 samples * 2 bytes */

    unsigned char raw[8];
    rewind(fp);
    TEST_CHECK(fread(raw, 1, sizeof(raw), fp) == sizeof(raw));

    TEST_CHECK((int16_t)read_u16le(raw + 0) == 0);
    TEST_CHECK((int16_t)read_u16le(raw + 2) == (int16_t)(0.5f * 32767.0f));
    TEST_CHECK((int16_t)read_u16le(raw + 4) == 32767);    /* +2.0 clamped */
    TEST_CHECK((int16_t)read_u16le(raw + 6) == -32767);   /* -2.0 clamped */

    fclose(fp);
}


static void test_wav_finalize_patches_sizes(void)
{
    FILE *fp = tmpfile();
    TEST_ASSERT(fp != NULL);

    wav_write_header(fp, 48000, 1);
    uint32_t data_bytes = 200;
    wav_finalize(fp, data_bytes);

    unsigned char hdr[44];
    rewind(fp);
    TEST_CHECK(fread(hdr, 1, sizeof(hdr), fp) == sizeof(hdr));

    TEST_CHECK(read_u32le(hdr + 4) == 36 + data_bytes);   /* RIFF size */
    TEST_CHECK(read_u32le(hdr + 40) == data_bytes);       /* data size */

    fclose(fp);
}


TEST_LIST =
{
    { "issues_all_zero",              test_issues_all_zero },
    { "dir_issues_counted_fields",    test_dir_issues_counted_fields },
    { "dir_issues_uncounted_fields",  test_dir_issues_uncounted_fields },
    { "total_issues_err_fields",      test_total_issues_err_fields },
    { "total_issues_sums_both",       test_total_issues_sums_both_directions },
    { "generate_tone_phase_channels", test_generate_tone_phase_and_channels },
    { "generate_iq_tone_magnitude",   test_generate_iq_tone_magnitude },
    { "wav_header_fields",            test_wav_header_fields },
    { "wav_append_clamps_scales",     test_wav_append_clamps_and_scales },
    { "wav_finalize_patches_sizes",   test_wav_finalize_patches_sizes },
    { NULL, NULL }
};
