/*
 *  Hamlib SmartSDR VITA-49 data path tests
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

/* The sample path, driven by the mock radio rather than by hardware: what
 * arrives from the radio reaching a reader, what a missing packet looks like,
 * and what happens when two streams are open at once. */

#include "acutest.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>

#include "smartsdr_mock.h"

#define MOCK_TIMEOUT_MS   "300"
#define MOCK_LIVENESS_MS  "5000"

#define AUDIO_STREAM_ID   0x84000000u
#define IQ_STREAM_ID      0x84000001u
#define PCC_AUDIO_F32     0x03E3
#define PCC_IQ_48K        0x02E4

#define AUDIO_RATE        24000
#define AUDIO_BYTES_PER_S (AUDIO_RATE * 2 * 4)

/* 256 floats a packet, which is 128 stereo pairs as the radio sends. */
#define PKT_FLOATS        256
#define PKT_BYTES         (PKT_FLOATS * 4)


static RIG *open_on_mock(struct smartsdr_mock *m)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR);
    char path[64];

    if (!rig)
    {
        TEST_ASSERT(0 && "rig_init failed");
    }

    smartsdr_mock_pathname(m, path, sizeof(path));
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), path);
    rig_set_conf(rig, rig_token_lookup(rig, "timeout"), MOCK_TIMEOUT_MS);
    rig_set_conf(rig, rig_token_lookup(rig, "retry"), "0");
    rig_set_conf(rig, rig_token_lookup(rig, "timeout_retry"), "0");
    rig_set_conf(rig, rig_token_lookup(rig, "liveness_timeout"),
                 MOCK_LIVENESS_MS);

    /* Whichever port the mock got: 4991 unless the host already had it. The
     * client sends transmit data to this port and prefers it for its own
     * socket, so mock and client have to agree on it. */
    {
        char vita[8];

        TEST_ASSERT(smartsdr_mock_vita_port(m) != 0);
        SNPRINTF(vita, sizeof(vita), "%u",
                 (unsigned)smartsdr_mock_vita_port(m));
        rig_set_conf(rig, rig_token_lookup(rig, "vita_port"), vita);
    }

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    return rig;
}


static rig_stream_t *open_stream(RIG *rig, rig_stream_type_t type, int rate,
                                 int channels, rig_stream_format_t fmt)
{
    struct rig_stream_config *cfg = rig_stream_config_alloc();
    rig_stream_t *st = NULL;
    int rc;

    cfg->type = type;
    cfg->format = fmt;
    cfg->sample_rate = rate;
    cfg->channels = channels;

    rc = rig_stream_open(rig, cfg, &st);
    free(cfg);

    if (rc != RIG_OK)
    {
        TEST_MSG("rig_stream_open returned %s", rigerror(rc));
        return NULL;
    }

    return st;
}


/* Drain whatever has arrived, returning the byte count. */
static size_t drain(RIG *rig, rig_stream_t *st, int rounds)
{
    static char buf[65536];
    size_t total = 0;
    int i;

    for (i = 0; i < rounds; i++)
    {
        size_t got = 0;

        if (rig_stream_read(rig, st, buf, sizeof(buf), &got, 20, NULL)
                == RIG_OK)
        {
            total += got;
        }
    }

    return total;
}


static uint64_t gaps_of(RIG *rig, rig_stream_t *st)
{
    struct rig_stream_stats s;

    memset(&s, 0, sizeof(s));
    rig_stream_get_stats(rig, st, &s);
    return s.gaps;
}


/* ================================================================== */
/* Delivery                                                            */
/* ================================================================== */

void test_audio_samples_reach_the_reader(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;
    size_t total;
    int i;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);
    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, AUDIO_RATE, 2,
                     RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(st != NULL);
    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    for (i = 0; i < 20; i++)
    {
        smartsdr_mock_send_samples(&m, AUDIO_STREAM_ID, PCC_AUDIO_F32,
                                   (uint8_t)(i & 0x0F), PKT_FLOATS);
        usleep(2000);
    }

    total = drain(rig, st, 20);

    TEST_CHECK(total >= 20 * PKT_BYTES);
    TEST_MSG("read %lu bytes, expected at least %d",
             (unsigned long)total, 20 * PKT_BYTES);

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* The radio says nothing about a packet it failed to deliver, so a break in
 * the sequence counter is the only evidence there was one. */
void test_a_skipped_counter_is_a_sized_gap(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;
    uint64_t before;
    uint64_t after;
    int i;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);
    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, AUDIO_RATE, 2,
                     RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(st != NULL);
    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    for (i = 0; i < 4; i++)
    {
        smartsdr_mock_send_samples(&m, AUDIO_STREAM_ID, PCC_AUDIO_F32,
                                   (uint8_t)i, PKT_FLOATS);
        usleep(3000);
    }

    drain(rig, st, 6);
    before = gaps_of(rig, st);

    /* Counter 4 is due next; 7 means three never arrived. */
    smartsdr_mock_send_samples(&m, AUDIO_STREAM_ID, PCC_AUDIO_F32, 7,
                               PKT_FLOATS);
    usleep(30 * 1000);
    drain(rig, st, 6);
    after = gaps_of(rig, st);

    TEST_CHECK(after > before);
    TEST_MSG("gaps %llu then %llu; a break in the counter should be reported",
             (unsigned long long)before, (unsigned long long)after);

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* The counter is four bits wide, so losing exactly sixteen packets leaves it
 * reading what it would have read anyway. Nothing can see that from the
 * sequence; only the byte rate gives it away. The limitation is asserted so
 * it stays a known one. */
void test_losing_exactly_sixteen_cannot_be_seen(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;
    uint64_t before;
    uint64_t after;
    int i;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);
    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, AUDIO_RATE, 2,
                     RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(st != NULL);
    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    for (i = 0; i < 4; i++)
    {
        smartsdr_mock_send_samples(&m, AUDIO_STREAM_ID, PCC_AUDIO_F32,
                                   (uint8_t)i, PKT_FLOATS);
        usleep(3000);
    }

    drain(rig, st, 6);
    before = gaps_of(rig, st);

    /* Sixteen packets after counter 3 is counter 4 again. */
    smartsdr_mock_send_samples(&m, AUDIO_STREAM_ID, PCC_AUDIO_F32, 4,
                               PKT_FLOATS);
    usleep(30 * 1000);
    drain(rig, st, 6);
    after = gaps_of(rig, st);

    TEST_CHECK(after == before);
    TEST_MSG("gaps %llu then %llu; a multiple of sixteen is invisible to a "
             "four-bit counter", (unsigned long long)before,
             (unsigned long long)after);

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* ================================================================== */
/* Two streams at once                                                 */
/* ================================================================== */

/* Every VITA datagram arrives on the one endpoint the client registered, so
 * the streams are told apart by their stream id alone. Giving each its own
 * socket once meant the second to open took the endpoint and the first fell
 * silent -- audio dropped to nothing the moment an I/Q stream opened. */
void test_two_streams_both_receive(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *audio;
    rig_stream_t *iq;
    size_t audio_bytes;
    size_t iq_bytes;
    int i;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);

    smartsdr_mock_reply(&m, "stream create", "0x84000000");
    audio = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, AUDIO_RATE, 2,
                        RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(audio != NULL);

    smartsdr_mock_reply(&m, "stream create", "0x84000001");
    iq = open_stream(rig, RIG_STREAM_TYPE_IQ_RX, 48000, 1,
                     RIG_STREAM_FORMAT_IQ_CF32);

    if (!iq)
    {
        rig_stream_close(rig, audio);
        rig_close(rig);
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "IQ_RX stream did not open");
    }

    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    for (i = 0; i < 16; i++)
    {
        smartsdr_mock_send_samples(&m, AUDIO_STREAM_ID, PCC_AUDIO_F32,
                                   (uint8_t)(i & 0x0F), PKT_FLOATS);
        smartsdr_mock_send_samples(&m, IQ_STREAM_ID, PCC_IQ_48K,
                                   (uint8_t)(i & 0x0F), PKT_FLOATS);
        usleep(2000);
    }

    audio_bytes = drain(rig, audio, 15);
    iq_bytes = drain(rig, iq, 15);

    TEST_CHECK(audio_bytes > 0);
    TEST_MSG("audio received nothing while an I/Q stream was open");
    TEST_CHECK(iq_bytes > 0);
    TEST_MSG("I/Q received nothing while an audio stream was open");

    rig_stream_close(rig, iq);
    rig_stream_close(rig, audio);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* Every I/Q test above weighs the stream by the byte, which is how DAX I/Q went
 * so long delivering denormals: the packets were the right size, arrived at the
 * right rate and lost nothing, and only the sample values were wrong. This one
 * looks at the values. It fails if the payload is byte-swapped, because DAX I/Q
 * is little-endian where the rest is network order, and it fails if the scale
 * is left alone, because the radio sends counts against a full scale of 32768
 * and the API promises +/-1.0. */
void test_iq_samples_are_little_endian_and_normalised(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *iq;
    float buf[PKT_FLOATS * 4];
    size_t got = 0;
    int i, checked = 0, denormal = 0, out_of_range = 0;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000001");

    rig = open_on_mock(&m);
    iq = open_stream(rig, RIG_STREAM_TYPE_IQ_RX, 48000, 1,
                     RIG_STREAM_FORMAT_IQ_CF32);

    if (!iq)
    {
        rig_close(rig);
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "IQ_RX stream did not open");
    }

    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    for (i = 0; i < 8; i++)
    {
        smartsdr_mock_send_samples(&m, IQ_STREAM_ID, PCC_IQ_48K,
                                   (uint8_t)(i & 0x0F), PKT_FLOATS);
        hl_usleep(2000);
    }

    for (i = 0; i < 40 && got == 0; i++)
    {
        if (rig_stream_read(rig, iq, buf, sizeof(buf), &got, 100, NULL)
                != RIG_OK)
        {
            got = 0;
        }

        if (got == 0) { hl_usleep(10000); }
    }

    TEST_CHECK(got > 0);
    TEST_MSG("no I/Q samples arrived");

    for (i = 0; i < (int)(got / sizeof(float)); i++)
    {
        float v = buf[i];

        checked++;

        if (v != 0.0f && fabsf(v) < 1.0e-37f) { denormal++; }

        if (fabsf(v) > 1.0f) { out_of_range++; }
    }

    TEST_CHECK(checked > 0);

    /* Byte-swapping the payload is what produces denormals. */
    TEST_CHECK(denormal == 0);
    TEST_MSG("%d of %d samples were denormal -- I/Q was byte-swapped",
             denormal, checked);

    /* Unnormalised counts run to +/-16384 here, far outside the promised range. */
    TEST_CHECK(out_of_range == 0);
    TEST_MSG("%d of %d samples exceeded +/-1.0 -- I/Q was not normalised",
             out_of_range, checked);

    /* The mock sends +/-16384 counts, so full scale 32768 puts them at +/-0.5. */
    TEST_CHECK(fabsf(fabsf(buf[0]) - 0.5f) < 1.0e-6f);
    TEST_MSG("first sample %.9f, expected magnitude 0.5", buf[0]);

    rig_stream_close(rig, iq);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* ================================================================== */
/* Transmit                                                            */
/* ================================================================== */

/* The backend keeps no clock of its own: it sends what the application gives
 * it, when it gives it. So what has to hold is that it keeps up -- an overrun
 * means the ring filled and audio was discarded before it could be sent. The
 * producer here writes on an absolute deadline so that it genuinely runs at
 * real time, leaving the backend's ability to keep pace the only variable. */
void test_transmit_loses_nothing_at_real_time(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;
    struct rig_stream_stats stats;
    static float samples[512];      /* 256 stereo pairs */
    struct timeval start;
    int64_t deadline_us;
    const int64_t step_us = (int64_t)(sizeof(samples) / 8) * 1000000
                            / AUDIO_RATE;
    int i;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);
    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_TX, AUDIO_RATE, 2,
                     RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(st != NULL);
    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));
    smartsdr_mock_capture_tx(&m);

    memset(samples, 0, sizeof(samples));
    gettimeofday(&start, NULL);
    deadline_us = (int64_t)start.tv_sec * 1000000 + start.tv_usec;

    /* Two seconds of audio, each block written when it is due rather than
     * after a sleep, so the schedule cannot drift. */
    for (i = 0; i < 2 * AUDIO_BYTES_PER_S / (int)sizeof(samples); i++)
    {
        struct timeval now;
        int64_t now_us;
        size_t wrote = 0;

        deadline_us += step_us;
        gettimeofday(&now, NULL);
        now_us = (int64_t)now.tv_sec * 1000000 + now.tv_usec;

        if (deadline_us > now_us)
        {
            usleep((useconds_t)(deadline_us - now_us));
        }

        rig_stream_write(rig, st, samples, sizeof(samples), &wrote, 500, NULL);
    }

    memset(&stats, 0, sizeof(stats));
    rig_stream_get_stats(rig, st, &stats);

    TEST_CHECK(stats.overruns == 0);
    TEST_MSG("%llu overruns: audio was written and then discarded unsent",
             (unsigned long long)stats.overruns);

    TEST_CHECK(smartsdr_mock_tx_count(&m) > 100);
    TEST_MSG("only %d datagrams reached the radio in two seconds",
             smartsdr_mock_tx_count(&m));

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}



/* ================================================================== */
/* Meters and spectrum                                                 */
/* ================================================================== */

/* The radio names its meters in status lines that carry several at a time,
 * and assigns the indices itself. A reader that took only the first meter on
 * each line bound the wrong index to a name, which showed up as a meter that
 * would never report. */
void test_meters_resolve_and_scale(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    value_t v;
    uint16_t ids[3];
    int16_t raw[3];
    int i;
    int rc;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);

    /* Three meters on one line, as the radio sends them. */
    smartsdr_mock_push_status(&m,
                              "meter 10.src=TX-#10.num=3#10.nam=SWR#10.unit=SWR#10.fps=20#"
                              "11.src=TX-#11.num=4#11.nam=PATEMP#11.unit=degC#11.fps=0#"
                              "6.src=RAD#6.num=0#6.nam=+13.8B#6.unit=Volts#6.fps=0#");

    /* A first read subscribes and starts the meter stream. */
    rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SWR, &v);
    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    /* Readings whose scaling is known: 128 is an SWR of exactly 1.00, 1767 is
     * 27.6 degC, 3591 is 14.03 V. */
    ids[0] = 10; raw[0] = 128;
    ids[1] = 11; raw[1] = 1767;
    ids[2] = 6;  raw[2] = 3591;

    for (i = 0; i < 10; i++)
    {
        smartsdr_mock_send_meters(&m, 0x08000000u, ids, raw, 3);
        usleep(10 * 1000);
    }

    rc = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SWR, &v);
    TEST_CHECK(rc == RIG_OK && v.f > 0.99f && v.f < 1.01f);
    TEST_MSG("SWR read %f, expected 1.00", (double)v.f);

    /* Reported with fps=0, so the radio sends it only as it changes; it must
     * still arrive. */
    rc = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_TEMP_METER, &v);
    TEST_CHECK(rc == RIG_OK && v.f > 27.0f && v.f < 28.2f);
    TEST_MSG("PA temperature read %f, expected about 27.6", (double)v.f);

    rc = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_VD_METER, &v);
    TEST_CHECK(rc == RIG_OK && v.f > 13.9f && v.f < 14.2f);
    TEST_MSG("supply voltage read %f, expected about 14.03", (double)v.f);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


static int spectrum_lines;
static int spectrum_bins;
static unsigned char spectrum_peak;

static int on_spectrum(RIG *rig, struct rig_spectrum_line *line, rig_ptr_t arg)
{
    size_t i;

    spectrum_lines++;
    spectrum_bins = (int)line->spectrum_data_length;

    for (i = 0; i < line->spectrum_data_length; i++)
    {
        if (line->spectrum_data[i] > spectrum_peak)
        {
            spectrum_peak = line->spectrum_data[i];
        }
    }

    return 0;
}


/* A frame wider than one datagram arrives in pieces sharing a frame index,
 * each carrying the bins from its own start. Nothing is delivered until the
 * whole frame is there. */
void test_a_split_fft_frame_reassembles(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    int waited;

    spectrum_lines = 0;
    spectrum_bins = 0;
    spectrum_peak = 0;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    /* The panadapter the slice already uses has to be known before the rig
     * opens, because that is when the backend goes looking for it. */
    smartsdr_mock_push_status(&m,
                              "display pan 0x40000000 center=14.100000 "
                              "bandwidth=0.200000 min_dbm=-135.00 max_dbm=-40.00 "
                              "x_pixels=64 y_pixels=255");

    rig = rig_init(RIG_MODEL_SMARTSDR);
    TEST_ASSERT(rig != NULL);
    {
        char path[64];

        smartsdr_mock_pathname(&m, path, sizeof(path));
        rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), path);
        rig_set_conf(rig, rig_token_lookup(rig, "timeout"), MOCK_TIMEOUT_MS);
        rig_set_conf(rig, rig_token_lookup(rig, "retry"), "0");
        rig_set_conf(rig, rig_token_lookup(rig, "timeout_retry"), "0");
        rig_set_conf(rig, rig_token_lookup(rig, "liveness_timeout"),
                     MOCK_LIVENESS_MS);
        /* Spectrum is set up while opening, so it must be asked for first. */
        rig_set_conf(rig, rig_token_lookup(rig, "spectrum"), "1");
    }

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    if (rig_set_spectrum_callback(rig, on_spectrum, NULL) != RIG_OK)
    {
        rig_close(rig);
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "the core would not take the spectrum callback");
    }

    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    /* One frame of 64 bins delivered as two halves. */
    smartsdr_mock_send_fft(&m, 0x40000000u, 0, 0, 32, 64, 1, 0);
    usleep(5000);
    smartsdr_mock_send_fft(&m, 0x40000000u, 1, 32, 32, 64, 1, 0);

    for (waited = 0; waited < 1000 && spectrum_lines == 0; waited += 20)
    {
        usleep(20 * 1000);
    }

    TEST_CHECK(spectrum_lines > 0);
    TEST_MSG("no spectrum line was delivered from two half frames");
    TEST_CHECK(spectrum_bins == 64);
    TEST_MSG("line carried %d bins, expected the whole frame of 64",
             spectrum_bins);

    /* Bins are heights, zero being the top of the panadapter and so the
     * strongest, which Hamlib reports the other way up. */
    TEST_CHECK(spectrum_peak == 255);
    TEST_MSG("a bin of zero should be reported as full strength, got %u",
             (unsigned)spectrum_peak);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* The backend's own entry, which is the hardware-native truth. Not
 * rig_stream_caps_at(): that serves the wider effective set the frontend
 * derives, which includes formats no SmartSDR radio puts on the wire. */
static const struct rig_stream_caps *native_entry(RIG *rig,
        rig_stream_type_t type)
{
    const struct rig_stream_caps *c = rig->caps->stream_caps;
    int i;

    for (i = 0; c != NULL && c[i].formats != 0; i++)
    {
        if (c[i].type == type)
        {
            return &c[i];
        }
    }

    return NULL;
}


/* VITA-49 carries float32 audio and complex float I/Q, and that is the whole
 * of what this radio produces. Declaring the integer formats too would label
 * a frontend conversion as native and break require_native for anyone asking
 * for it. */
void test_caps_declare_only_the_wire_formats(void)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR);
    const struct rig_stream_caps *audio, *iq;

    TEST_ASSERT(rig != NULL);

    audio = native_entry(rig, RIG_STREAM_TYPE_AUDIO_RX);
    TEST_ASSERT(audio != NULL);
    TEST_CHECK(audio->formats == RIG_STREAM_FORMAT_PCM_F32);
    TEST_MSG("audio RX native formats = 0x%x", (unsigned)audio->formats);

    iq = native_entry(rig, RIG_STREAM_TYPE_IQ_RX);
    TEST_ASSERT(iq != NULL);
    TEST_CHECK(iq->formats == RIG_STREAM_FORMAT_IQ_CF32);
    TEST_MSG("I/Q native formats = 0x%x", (unsigned)iq->formats);

    /* The channel list is exact and 0-terminated: DAX audio is a stereo pair
     * and nothing else, and I/Q is one complex stream. A mono reader is the
     * frontend's business, not a count this radio produces. */
    TEST_CHECK(audio->channels[0] == 2 && audio->channels[1] == 0);
    TEST_MSG("audio channels: %d then %d", audio->channels[0],
             audio->channels[1]);
    TEST_CHECK(iq->channels[0] == 1 && iq->channels[1] == 0);
    TEST_MSG("I/Q channels: %d then %d", iq->channels[0], iq->channels[1]);

    rig_cleanup(rig);
}


/* Asking for exactly what the radio produces must engage no stage at all. */
void test_a_native_request_reports_no_conversion(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);
    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, AUDIO_RATE, 2,
                     RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(st != NULL);

    TEST_CHECK(rig_stream_get_conversions(st) == RIG_STREAM_CONV_NONE);
    TEST_MSG("a native open must report no stages, got 0x%x",
             rig_stream_get_conversions(st));

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* The backend produces float32 only, so an integer request is the frontend's
 * to serve. Draining it proves the samples survive the whole path: the
 * backend delivers native bytes and the conversion runs on them before the
 * reader sees anything. */
void test_a_converted_request_still_delivers_samples(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;
    size_t total;
    int i;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);
    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, AUDIO_RATE, 2,
                     RIG_STREAM_FORMAT_PCM_S16);
    TEST_ASSERT(st != NULL);

    TEST_CHECK((rig_stream_get_conversions(st) & RIG_STREAM_CONV_FORMAT) != 0);
    TEST_MSG("S16 is not native here, so a format stage must be active");

    TEST_CHECK(smartsdr_mock_wait_udp(&m, 2000));

    for (i = 0; i < 20; i++)
    {
        smartsdr_mock_send_samples(&m, AUDIO_STREAM_ID, PCC_AUDIO_F32,
                                   (uint8_t)(i & 0x0F), PKT_FLOATS);
        usleep(2000);
    }

    total = drain(rig, st, 20);

    /* S16 is half the width of the float32 on the wire, so the reader sees
     * half the bytes for the same samples. The upper bound is what makes this
     * a test: bytes handed over without the conversion would arrive at full
     * float32 width and still satisfy a lower bound on its own. */
    TEST_CHECK(total >= 20 * PKT_BYTES / 2);
    TEST_CHECK(total < 20 * PKT_BYTES);
    TEST_MSG("read %lu bytes, expected about %d and certainly under %d",
             (unsigned long)total, 20 * PKT_BYTES / 2, 20 * PKT_BYTES);

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* DAX audio is a stereo pair on the wire, so a mono reader needs the channel
 * stage. */
void test_a_mono_request_maps_channels(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);
    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, AUDIO_RATE, 1,
                     RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(st != NULL);

    TEST_CHECK((rig_stream_get_conversions(st) & RIG_STREAM_CONV_CHANNELS)
               != 0);
    TEST_MSG("mono against a stereo wire must map channels, got 0x%x",
             rig_stream_get_conversions(st));

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* Two stream types share one UDP socket, so cycling both against each other
 * drives the reference count and the teardown from two threads at once.
 *
 * What this covers: that opening and closing concurrently keeps working, and
 * in particular that waiting out a teardown neither deadlocks nor loses a
 * wakeup -- a lost wakeup hangs here rather than failing.
 *
 * What it does not cover: the race the wait exists for. That window is a few
 * instructions wide and this does not reproduce it -- removing the wait
 * leaves the case passing. The serialisation rests on reading the code, not
 * on this failing without it. */
struct cycler
{
    RIG *rig;
    rig_stream_type_t type;
    int rate;
    int channels;
    rig_stream_format_t format;
    int rounds;
    int failures;
};

static void *cycle_streams(void *arg)
{
    struct cycler *c = arg;
    int i;

    for (i = 0; i < c->rounds; i++)
    {
        rig_stream_t *st = open_stream(c->rig, c->type, c->rate, c->channels,
                                       c->format);

        if (st == NULL)
        {
            c->failures++;
            continue;
        }

        rig_stream_close(c->rig, st);
    }

    return NULL;
}


void test_streams_can_be_cycled_from_two_threads(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    pthread_t audio_thread, iq_thread;
    struct cycler audio, iq;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");

    rig = open_on_mock(&m);

    audio.rig = rig;
    audio.type = RIG_STREAM_TYPE_AUDIO_RX;
    audio.rate = AUDIO_RATE;
    audio.channels = 2;
    audio.format = RIG_STREAM_FORMAT_PCM_F32;
    audio.rounds = 12;
    audio.failures = 0;

    iq = audio;
    iq.type = RIG_STREAM_TYPE_IQ_RX;
    iq.rate = 48000;
    iq.channels = 1;
    iq.format = RIG_STREAM_FORMAT_IQ_CF32;

    TEST_ASSERT(pthread_create(&audio_thread, NULL, cycle_streams, &audio) == 0);
    TEST_ASSERT(pthread_create(&iq_thread, NULL, cycle_streams, &iq) == 0);
    pthread_join(audio_thread, NULL);
    pthread_join(iq_thread, NULL);

    TEST_CHECK(audio.failures == 0);
    TEST_MSG("%d of %d audio opens failed", audio.failures, audio.rounds);
    TEST_CHECK(iq.failures == 0);
    TEST_MSG("%d of %d I/Q opens failed", iq.failures, iq.rounds);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* Declaring RIG_STREAM_CAP_HW_TIME is a promise that the radio can be asked
 * what time it is, so the hook has to exist to answer. What it answers depends
 * on the hardware, and the mock radio reports neither a GPSDO nor a TCXO: the
 * only honest answer is the host clock, named as the host clock rather than
 * dressed up as the radio's. */
void test_hardware_time_names_its_source(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    rig_stream_t *st;
    struct rig_stream_time_anchor now;
    int i;

    smartsdr_mock_start(&m);
    smartsdr_mock_reply(&m, "stream create", "0x84000000");
    rig = open_on_mock(&m);

    /* Every model that makes the promise must carry the hook, which is the
     * pairing dump_caps checks. */
    for (i = 0; rig->caps->stream_caps[i].formats != 0; i++)
    {
        if (rig->caps->stream_caps[i].caps_flags & RIG_STREAM_CAP_HW_TIME)
        {
            TEST_CHECK(rig->caps->stream_hardware_time != NULL);
            TEST_MSG("HW_TIME declared without stream_hardware_time");
        }
    }

    st = open_stream(rig, RIG_STREAM_TYPE_AUDIO_RX, 24000, 2,
                     RIG_STREAM_FORMAT_PCM_F32);
    TEST_ASSERT(st != NULL);

    memset(&now, 0, sizeof(now));
    TEST_CHECK(rig_stream_get_hardware_time(rig, st, &now) == RIG_OK);
    TEST_CHECK(now.source == RIG_STREAM_TIME_SRC_HOST);
    TEST_MSG("time source %d, expected host (%d)", (int)now.source,
             (int)RIG_STREAM_TIME_SRC_HOST);
    TEST_CHECK(now.accuracy == RIG_STREAM_TIME_ACC_MS);
    TEST_CHECK(now.seconds > 0);
    TEST_MSG("seconds=%lld", (long long)now.seconds);

    rig_stream_close(rig, st);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


TEST_LIST =
{
    { "iq_samples_are_little_endian_and_normalised", test_iq_samples_are_little_endian_and_normalised },
    { "audio_samples_reach_the_reader",     test_audio_samples_reach_the_reader },
    { "a_skipped_counter_is_a_sized_gap",   test_a_skipped_counter_is_a_sized_gap },
    { "losing_exactly_sixteen_cannot_be_seen", test_losing_exactly_sixteen_cannot_be_seen },
    { "two_streams_both_receive",           test_two_streams_both_receive },
    { "transmit_loses_nothing_at_real_time", test_transmit_loses_nothing_at_real_time },
    { "meters_resolve_and_scale",           test_meters_resolve_and_scale },
    { "a_split_fft_frame_reassembles",      test_a_split_fft_frame_reassembles },
    { "caps_declare_only_the_wire_formats", test_caps_declare_only_the_wire_formats },
    { "a_native_request_reports_no_conversion", test_a_native_request_reports_no_conversion },
    { "a_converted_request_still_delivers_samples", test_a_converted_request_still_delivers_samples },
    { "a_mono_request_maps_channels",       test_a_mono_request_maps_channels },
    { "streams_can_be_cycled_from_two_threads", test_streams_can_be_cycled_from_two_threads },
    { "hardware_time_names_its_source",     test_hardware_time_names_its_source },
    { NULL, NULL }
};
