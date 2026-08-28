/*
 *  Hamlib stream loss-accounting tests
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

/* Unit tests for stream loss accounting: producer index, gap marking, */
/* per-cause counters, and read-side dropped-sample attribution.       */

#include "acutest.h"
#include "test_debug.h"

#include <string.h>

#include <hamlib/rig.h>
#include "stream.h"


/* Fabricate a minimal stream for primitive-level tests: S16LE mono
 * (2 bytes/frame) with a small ring buffer. */
static void stream_setup(struct rig_stream *s, size_t capacity)
{
    memset(s, 0, sizeof(*s));
    TEST_ASSERT(stream_ringbuf_init(&s->ringbuf, capacity) == 0);
    stream_write_event_init(s);
    s->config.format = RIG_STREAM_FORMAT_PCM_S16;
    s->config.channels = 1;
    s->config.sample_rate = 48000;
    s->frame_bytes = 2;
    s->active = 1;
}


static void stream_teardown(struct rig_stream *s)
{
    stream_write_event_destroy(s);
    stream_ringbuf_destroy(&s->ringbuf);
}


static void write_frames(struct rig_stream *s, int frames)
{
    unsigned char buf[512];
    memset(buf, 0xAB, sizeof(buf));

    while (frames > 0)
    {
        int chunk = frames > 256 ? 256 : frames;
        stream_ringbuf_write(&s->ringbuf, buf, (size_t)chunk * 2);
        frames -= chunk;
    }
}


static void test_write_total_counts_all_bytes(void)
{
    struct rig_stream_ringbuf rb;
    unsigned char buf[300];

    TEST_ASSERT(stream_ringbuf_init(&rb, 256) == 0);
    TEST_CHECK(rb.write_total == 0);

    stream_ringbuf_write(&rb, buf, 100);
    TEST_CHECK(rb.write_total == 100);

    /* Overwrite: still counts every byte produced */
    stream_ringbuf_write(&rb, buf, 200);
    TEST_CHECK_(rb.write_total == 300,
                "write_total=%llu", (unsigned long long)rb.write_total);

    /* Write larger than capacity: counts the full original length */
    stream_ringbuf_write(&rb, buf, 300);
    TEST_CHECK(rb.write_total == 600);

    stream_ringbuf_destroy(&rb);
}


static void test_first_readable_index_basic(void)
{
    struct rig_stream s;
    stream_setup(&s, 1024);

    TEST_CHECK(stream_first_readable_index(&s) == 0);

    write_frames(&s, 10);
    /* Nothing consumed or lost: oldest readable frame is index 0 */
    TEST_CHECK(stream_first_readable_index(&s) == 0);

    unsigned char out[8];
    stream_ringbuf_read(&s.ringbuf, out, 8, 10);   /* consume 4 frames */
    TEST_CHECK_(stream_first_readable_index(&s) == 4,
                "idx=%llu",
                (unsigned long long)stream_first_readable_index(&s));

    stream_teardown(&s);
}


static void test_first_readable_index_jumps_on_overrun(void)
{
    struct rig_stream s;
    stream_setup(&s, 256);   /* 128 frames capacity */

    write_frames(&s, 128);
    TEST_CHECK(stream_first_readable_index(&s) == 0);

    /* 10 more frames overwrite the 10 oldest */
    write_frames(&s, 10);
    TEST_CHECK_(stream_first_readable_index(&s) == 10,
                "idx=%llu",
                (unsigned long long)stream_first_readable_index(&s));
    TEST_CHECK(s.ringbuf.overrun_count == 1);

    stream_teardown(&s);
}


static void test_mark_gap_sized(void)
{
    struct rig_stream s;
    stream_setup(&s, 1024);

    write_frames(&s, 4);
    TEST_CHECK(rig_stream_mark_gap(&s, 5000) == RIG_OK);

    TEST_CHECK(s.gap_count == 1);
    TEST_CHECK(s.gaps_unknown == 0);
    TEST_CHECK(s.dropped_samples_gap == 5000);

    /* The index domain acknowledges the hole */
    TEST_CHECK_(stream_first_readable_index(&s) == 5000,
                "idx=%llu",
                (unsigned long long)stream_first_readable_index(&s));

    stream_teardown(&s);
}


static void test_mark_gap_unsized(void)
{
    struct rig_stream s;
    stream_setup(&s, 1024);

    TEST_CHECK(rig_stream_mark_gap(&s, 0) == RIG_OK);

    TEST_CHECK(s.gap_count == 1);
    TEST_CHECK(s.gaps_unknown == 1);
    TEST_CHECK(s.dropped_samples_gap == 0);
    /* Unsized: no index advance */
    TEST_CHECK(stream_first_readable_index(&s) == 0);

    stream_teardown(&s);
}


static void test_consume_account_gap_attribution(void)
{
    struct rig_stream s;
    struct rig_stream_read_info info;
    stream_setup(&s, 1024);

    write_frames(&s, 10);

    unsigned char out[20];
    size_t got = stream_ringbuf_read(&s.ringbuf, out, 20, 10);
    TEST_CHECK(got == 20);
    stream_consume_account(&s, 0, 10, &info);

    TEST_CHECK(info.sample_index == 0);
    TEST_CHECK(info.dropped_samples == 0);
    TEST_CHECK(info.drop_flags == 0);

    /* Radio-side gap of 100, then more data */
    rig_stream_mark_gap(&s, 100);
    write_frames(&s, 10);

    uint64_t first = stream_first_readable_index(&s);
    TEST_CHECK_(first == 110, "first=%llu", (unsigned long long)first);

    got = stream_ringbuf_read(&s.ringbuf, out, 20, 10);
    TEST_CHECK(got == 20);
    stream_consume_account(&s, first, 10, &info);

    TEST_CHECK(info.sample_index == 110);
    TEST_CHECK_(info.dropped_samples == 100,
                "dropped=%u", info.dropped_samples);
    TEST_CHECK(info.drop_flags == RIG_STREAM_DROP_GAP);
    TEST_CHECK(s.dropped_samples_gap == 100);
    TEST_CHECK(s.dropped_samples_overrun == 0);

    /* Next read: clean again */
    write_frames(&s, 4);
    first = stream_first_readable_index(&s);
    got = stream_ringbuf_read(&s.ringbuf, out, 8, 10);
    stream_consume_account(&s, first, 4, &info);
    TEST_CHECK(info.dropped_samples == 0);
    TEST_CHECK(info.drop_flags == 0);

    stream_teardown(&s);
}


static void test_consume_account_overrun_attribution(void)
{
    struct rig_stream s;
    struct rig_stream_read_info info;
    stream_setup(&s, 256);   /* 128 frames */

    write_frames(&s, 128);
    write_frames(&s, 32);    /* 32 oldest frames overwritten */

    uint64_t first = stream_first_readable_index(&s);
    TEST_CHECK(first == 32);

    unsigned char out[16];
    stream_ringbuf_read(&s.ringbuf, out, 16, 10);
    stream_consume_account(&s, first, 8, &info);

    TEST_CHECK_(info.dropped_samples == 32,
                "dropped=%u", info.dropped_samples);
    TEST_CHECK(info.drop_flags == RIG_STREAM_DROP_OVERRUN);
    TEST_CHECK(s.dropped_samples_overrun == 32);
    TEST_CHECK(s.dropped_samples_gap == 0);

    stream_teardown(&s);
}


static void test_consume_account_mixed_causes(void)
{
    struct rig_stream s;
    struct rig_stream_read_info info;
    stream_setup(&s, 256);   /* 128 frames */

    write_frames(&s, 128);
    rig_stream_mark_gap(&s, 50);       /* radio gap */
    rig_stream_mark_gap(&s, 0);        /* plus an unsized one */
    write_frames(&s, 16);              /* 16 oldest overwritten */

    uint64_t first = stream_first_readable_index(&s);
    /* 16 (overrun) + 50 (gap) holes before the oldest readable frame */
    TEST_CHECK_(first == 66, "first=%llu", (unsigned long long)first);

    unsigned char out[16];
    stream_ringbuf_read(&s.ringbuf, out, 16, 10);
    stream_consume_account(&s, first, 8, &info);

    TEST_CHECK(info.dropped_samples == 66);
    TEST_CHECK(info.drop_flags == (RIG_STREAM_DROP_GAP
                                   | RIG_STREAM_DROP_OVERRUN
                                   | RIG_STREAM_DROP_UNSIZED));
    TEST_CHECK(s.dropped_samples_gap == 50);
    TEST_CHECK(s.dropped_samples_overrun == 16);
    TEST_CHECK(s.gap_count == 2);
    TEST_CHECK(s.gaps_unknown == 1);

    stream_teardown(&s);
}


static void test_skip_samples_link_attribution(void)
{
    struct rig_stream s;
    stream_setup(&s, 1024);

    stream_skip_samples(&s, 200, RIG_STREAM_DROP_LINK);
    TEST_CHECK(s.link_loss == 1);
    TEST_CHECK(s.dropped_samples_link == 200);
    TEST_CHECK(stream_first_readable_index(&s) == 200);

    stream_skip_samples(&s, 30, RIG_STREAM_DROP_OVERRUN);
    TEST_CHECK(s.remote_overruns == 1);
    TEST_CHECK(s.dropped_samples_overrun == 30);

    stream_teardown(&s);
}


static void test_get_stats_snapshot(void)
{
    struct rig_stream s;
    struct rig_stream_stats st;
    RIG *rig = rig_init(RIG_MODEL_DUMMY);

    TEST_ASSERT(rig != NULL);
    stream_setup(&s, 256);   /* 128 frames */

    write_frames(&s, 128);
    write_frames(&s, 16);                 /* 1 overrun event */
    rig_stream_mark_gap(&s, 50);
    rig_stream_mark_gap(&s, 0);
    stream_skip_samples(&s, 200, RIG_STREAM_DROP_LINK);

    /* Consume so overrun lost-samples get attributed */
    unsigned char out[16];
    uint64_t first = stream_first_readable_index(&s);
    stream_ringbuf_read(&s.ringbuf, out, 16, 10);
    stream_consume_account(&s, first, 8, NULL);

    TEST_CHECK(rig_stream_get_stats(rig, &s, &st) == RIG_OK);
    TEST_CHECK_(st.overruns == 1, "overruns=%u", st.overruns);
    TEST_CHECK(st.gaps == 2);
    TEST_CHECK(st.gaps_unknown == 1);
    TEST_CHECK(st.link_loss == 1);
    TEST_CHECK(st.dropped_samples_gap == 50);
    TEST_CHECK(st.dropped_samples_overrun == 16);
    TEST_CHECK(st.dropped_samples_link == 200);
    TEST_CHECK(st.underruns == 0);
    TEST_CHECK(st.tx_late == 0);

    TEST_CHECK(rig_stream_get_stats(NULL, &s, &st) == -RIG_EINVAL);
    TEST_CHECK(rig_stream_get_stats(rig, NULL, &st) == -RIG_EINVAL);
    TEST_CHECK(rig_stream_get_stats(rig, &s, NULL) == -RIG_EINVAL);

    stream_teardown(&s);
    rig_cleanup(rig);
}


static void test_write_status_fifo(void)
{
    struct rig_stream s;
    struct rig_stream_write_status out;
    RIG *rig = rig_init(RIG_MODEL_DUMMY);

    TEST_ASSERT(rig != NULL);
    stream_setup(&s, 256);
    s.type = RIG_STREAM_TYPE_AUDIO_TX;   /* the event channel is TX-only */

    /* Nothing recorded yet -> non-blocking poll times out. */
    memset(&out, 0xFF, sizeof(out));
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == -RIG_ETIMEOUT);

    struct rig_stream_write_status ev;
    memset(&ev, 0, sizeof(ev));
    ev.event = RIG_STREAM_WRITE_EVENT_LATE;
    ev.sample_index = 7777;
    ev.lateness = 480;
    ev.time_valid = 1;
    ev.seconds = 1736000000;
    stream_record_write_status(&s, &ev, 0);

    /* First poll consumes the event; local event has no REMOTE flag. */
    memset(&out, 0, sizeof(out));
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == RIG_OK);
    TEST_CHECK(out.event == RIG_STREAM_WRITE_EVENT_LATE);
    TEST_CHECK(out.sample_index == 7777);
    TEST_CHECK_(out.lateness == 480, "lateness=%lld", (long long)out.lateness);
    TEST_CHECK((out.flags & RIG_STREAM_WRITE_STATUS_REMOTE) == 0);
    TEST_CHECK(s.tx_late == 1);

    /* Drained. */
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == -RIG_ETIMEOUT);

    /* FIFO preserves order (does not overwrite). */
    struct rig_stream_write_status a = { .event = RIG_STREAM_WRITE_EVENT_UNDERRUN };
    struct rig_stream_write_status b = { .event = RIG_STREAM_WRITE_EVENT_OVERRUN };
    stream_record_write_status(&s, &a, 0);
    stream_record_write_status(&s, &b, 0);
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == RIG_OK);
    TEST_CHECK(out.event == RIG_STREAM_WRITE_EVENT_UNDERRUN);
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == RIG_OK);
    TEST_CHECK(out.event == RIG_STREAM_WRITE_EVENT_OVERRUN);
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == -RIG_ETIMEOUT);

    /* Remote event: REMOTE flag set, remote_overruns bumped. */
    struct rig_stream_write_status r = { .event = RIG_STREAM_WRITE_EVENT_OVERRUN };
    stream_record_write_status(&s, &r, 1);
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == RIG_OK);
    TEST_CHECK(out.flags & RIG_STREAM_WRITE_STATUS_REMOTE);
    TEST_CHECK(s.remote_overruns == 1);

    /* Overflow drops oldest and counts write_events_dropped. */
    for (int i = 0; i < RIG_STREAM_WRITE_EVENT_DEPTH + 2; i++)
    {
        struct rig_stream_write_status f = { .event = RIG_STREAM_WRITE_EVENT_OVERRUN };
        f.sample_index = (uint64_t)i;
        stream_record_write_status(&s, &f, 0);
    }

    TEST_CHECK_(s.write_events_dropped == 2, "write_events_dropped=%u",
                s.write_events_dropped);
    /* Oldest two dropped: first surviving event has sample_index 2. */
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == RIG_OK);
    TEST_CHECK_(out.sample_index == 2, "sample_index=%llu",
                (unsigned long long)out.sample_index);

    /* Argument validation + RX rejection. */
    TEST_CHECK(rig_stream_wait_write_status(NULL, &s, &out, 0) == -RIG_EINVAL);
    TEST_CHECK(rig_stream_wait_write_status(rig, NULL, &out, 0) == -RIG_EINVAL);
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, NULL, 0) == -RIG_EINVAL);
    s.type = RIG_STREAM_TYPE_AUDIO_RX;
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 0) == -RIG_ENAVAIL);

    /* Closing wakes/refuses a waiter even with a blocking timeout. */
    s.type = RIG_STREAM_TYPE_AUDIO_TX;
    s.write_event_closing = 1;
    TEST_CHECK(rig_stream_wait_write_status(rig, &s, &out, 1000) == -RIG_ENAVAIL);

    stream_teardown(&s);
    rig_cleanup(rig);
}


/* --- Time anchors, interpolation, staleness watchdog --- */

static struct rig_stream_time_anchor make_anchor(uint64_t idx, int64_t sec,
        uint64_t ps)
{
    struct rig_stream_time_anchor a;
    memset(&a, 0, sizeof(a));
    a.sample_index = idx;
    a.seconds = sec;
    a.picoseconds = ps;
    a.source = RIG_STREAM_TIME_SRC_GPS;
    a.flags = RIG_STREAM_TIME_FLAG_LOCKED | RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED;
    a.accuracy = RIG_STREAM_TIME_ACC_100NS;
    return a;
}


static void test_anchor_push_get(void)
{
    struct rig_stream s;
    struct rig_stream_time_anchor a, out;
    stream_setup(&s, 1024);

    TEST_CHECK(rig_stream_get_time_anchor(&s, &out) == -RIG_ENAVAIL);

    a = make_anchor(100, 1736000000, 250000000000ULL);
    TEST_CHECK(rig_stream_push_time_anchor(&s, &a) == RIG_OK);

    TEST_CHECK(rig_stream_get_time_anchor(&s, &out) == RIG_OK);
    TEST_CHECK(out.sample_index == 100);
    TEST_CHECK(out.seconds == 1736000000);
    TEST_CHECK(out.picoseconds == 250000000000ULL);
    TEST_CHECK(out.source == RIG_STREAM_TIME_SRC_GPS);

    stream_teardown(&s);
}


static void test_anchor_ring_drops_oldest(void)
{
    struct rig_stream s;
    struct rig_stream_time_anchor a, out;
    struct rig_stream_read_info info;
    stream_setup(&s, 1024);

    /* Push 20 anchors at indices 0,100,...,1900 — depth 16 keeps 400..1900 */
    for (int i = 0; i < 20; i++)
    {
        a = make_anchor((uint64_t)i * 100, 1000 + i, 0);
        rig_stream_push_time_anchor(&s, &a);
    }

    TEST_CHECK(rig_stream_get_time_anchor(&s, &out) == RIG_OK);
    TEST_CHECK(out.sample_index == 1900);

    /* Index older than every retained anchor: no time available */
    memset(&info, 0, sizeof(info));
    info.sample_index = 150;
    stream_fill_read_time(&s, &info);
    TEST_CHECK(info.time_valid == 0);

    stream_teardown(&s);
}


static void test_read_time_interpolation(void)
{
    struct rig_stream s;
    struct rig_stream_time_anchor a;
    struct rig_stream_read_info info;
    stream_setup(&s, 1024);   /* 48 kHz from stream_setup */

    a = make_anchor(1000, 1000, 0);
    rig_stream_push_time_anchor(&s, &a);

    /* 24000 samples past the anchor at 48 kHz = +0.5 s */
    memset(&info, 0, sizeof(info));
    info.sample_index = 25000;
    stream_fill_read_time(&s, &info);

    TEST_CHECK(info.time_valid == 1);
    TEST_CHECK_(info.seconds == 1000, "sec=%lld", (long long)info.seconds);
    TEST_CHECK_(info.picoseconds == 500000000000ULL,
                "ps=%llu", (unsigned long long)info.picoseconds);
    TEST_CHECK(info.time_source == RIG_STREAM_TIME_SRC_GPS);
    TEST_CHECK(info.time_accuracy == RIG_STREAM_TIME_ACC_100NS);
    TEST_CHECK(info.time_flags & RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED);
    TEST_CHECK(!(info.time_flags & RIG_STREAM_TIME_FLAG_DISCONTINUITY));

    /* Exactly at the anchor: exact time, no interpolation */
    memset(&info, 0, sizeof(info));
    info.sample_index = 1000;
    stream_fill_read_time(&s, &info);
    TEST_CHECK(info.time_valid == 1);
    TEST_CHECK(info.seconds == 1000 && info.picoseconds == 0);

    stream_teardown(&s);
}


static void test_read_time_watchdog_degrades(void)
{
    struct rig_stream s;
    struct rig_stream_time_anchor a;
    struct rig_stream_read_info info;
    stream_setup(&s, 1024);

    s.stale_coarse_ms = 1000;
    s.stale_invalidate_ms = 5000;

    a = make_anchor(0, 1000, 0);
    rig_stream_push_time_anchor(&s, &a);

    /* 2 s past the anchor: still valid, downgraded to COARSE */
    memset(&info, 0, sizeof(info));
    info.sample_index = 96000;
    stream_fill_read_time(&s, &info);
    TEST_CHECK(info.time_valid == 1);
    TEST_CHECK_(info.time_accuracy == RIG_STREAM_TIME_ACC_COARSE,
                "acc=%u", info.time_accuracy);

    /* 10 s past the anchor: invalidated */
    memset(&info, 0, sizeof(info));
    info.sample_index = 480000;
    stream_fill_read_time(&s, &info);
    TEST_CHECK(info.time_valid == 0);

    stream_teardown(&s);
}


static void test_read_time_discontinuity_flag(void)
{
    struct rig_stream s;
    struct rig_stream_time_anchor a;
    struct rig_stream_read_info info;
    stream_setup(&s, 1024);

    a = make_anchor(0, 1000, 0);
    rig_stream_push_time_anchor(&s, &a);

    memset(&info, 0, sizeof(info));
    info.sample_index = 4800;
    info.dropped_samples = 100;
    info.drop_flags = RIG_STREAM_DROP_GAP;
    stream_fill_read_time(&s, &info);

    TEST_CHECK(info.time_valid == 1);
    TEST_CHECK(info.time_flags & RIG_STREAM_TIME_FLAG_DISCONTINUITY);

    stream_teardown(&s);
}


static void test_stale_threshold_resolution_at_open(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    rig_stream_t *st = NULL;
    struct rig_stream_config cfg;

    TEST_ASSERT(rig != NULL);
    TEST_ASSERT(rig_open(rig) == RIG_OK);

    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    /* Defaults when unset */
    TEST_ASSERT(rig_stream_open(rig, &cfg, &st) == RIG_OK);
    TEST_CHECK_(st->stale_coarse_ms == 1000, "coarse=%u", st->stale_coarse_ms);
    TEST_CHECK(st->stale_invalidate_ms == 5000);
    rig_stream_close(rig, st);

    /* Per-stream override */
    cfg.time_stale_coarse_ms = 200;
    cfg.time_stale_invalidate_ms = 700;
    TEST_ASSERT(rig_stream_open(rig, &cfg, &st) == RIG_OK);
    TEST_CHECK(st->stale_coarse_ms == 200);
    TEST_CHECK(st->stale_invalidate_ms == 700);
    rig_stream_close(rig, st);

    /* coarse > invalidate: clamped (coarse lowered to invalidate) */
    cfg.time_stale_coarse_ms = 9000;
    cfg.time_stale_invalidate_ms = 3000;
    TEST_ASSERT(rig_stream_open(rig, &cfg, &st) == RIG_OK);
    TEST_CHECK_(st->stale_coarse_ms == 3000, "coarse=%u", st->stale_coarse_ms);
    TEST_CHECK(st->stale_invalidate_ms == 3000);
    rig_stream_close(rig, st);

    /* Rig-level conf default applies when per-stream is unset */
    TEST_CHECK(rig_set_conf(rig, rig_token_lookup(rig, "stream_time_stale_coarse"),
                            "300") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, rig_token_lookup(rig,
                            "stream_time_stale_invalidate"),
                            "800") == RIG_OK);
    cfg.time_stale_coarse_ms = 0;
    cfg.time_stale_invalidate_ms = 0;
    TEST_ASSERT(rig_stream_open(rig, &cfg, &st) == RIG_OK);
    TEST_CHECK_(st->stale_coarse_ms == 300, "coarse=%u", st->stale_coarse_ms);
    TEST_CHECK(st->stale_invalidate_ms == 800);
    rig_stream_close(rig, st);

    rig_close(rig);
    rig_cleanup(rig);
}


/* --- TX target channel + samples_written --- */

static void test_samples_written(void)
{
    struct rig_stream s;
    stream_setup(&s, 1024);

    TEST_CHECK(rig_stream_get_samples_written(&s) == 0);

    write_frames(&s, 10);
    TEST_CHECK(rig_stream_get_samples_written(&s) == 10);

    rig_stream_mark_gap(&s, 5);
    TEST_CHECK_(rig_stream_get_samples_written(&s) == 15,
                "written=%llu",
                (unsigned long long)rig_stream_get_samples_written(&s));

    stream_teardown(&s);
}


static void test_tx_target_push_pop(void)
{
    struct rig_stream s;
    struct rig_stream_tx_target t, out;
    stream_setup(&s, 1024);

    /* Empty: nothing pending */
    TEST_CHECK(rig_stream_pop_tx_target(&s, 1000, &out) == 0);

    memset(&t, 0, sizeof(t));
    t.sample_index = 100;
    t.seconds = 2000;
    t.picoseconds = 250000000000ULL;
    t.flags = RIG_STREAM_TIME_FLAG_SOB;
    TEST_CHECK(stream_push_tx_target(&s, &t) == RIG_OK);

    t.sample_index = 500;
    t.seconds = 0;                         /* unscheduled boundary */
    t.picoseconds = 0;
    t.flags = RIG_STREAM_TIME_FLAG_EOB;
    TEST_CHECK(stream_push_tx_target(&s, &t) == RIG_OK);

    /* Backend hasn't reached the first target's index yet */
    TEST_CHECK(rig_stream_pop_tx_target(&s, 50, &out) == 0);

    /* Reaching index 400 releases the SOB target only, exactly once */
    TEST_CHECK(rig_stream_pop_tx_target(&s, 400, &out) == 1);
    TEST_CHECK(out.sample_index == 100);
    TEST_CHECK(out.seconds == 2000);
    TEST_CHECK(out.flags == RIG_STREAM_TIME_FLAG_SOB);
    TEST_CHECK(rig_stream_pop_tx_target(&s, 400, &out) == 0);

    TEST_CHECK(rig_stream_pop_tx_target(&s, 600, &out) == 1);
    TEST_CHECK(out.sample_index == 500);
    TEST_CHECK(out.flags == RIG_STREAM_TIME_FLAG_EOB);

    stream_teardown(&s);
}


static void test_tx_target_drop_oldest(void)
{
    struct rig_stream s;
    struct rig_stream_tx_target t, out;
    stream_setup(&s, 1024);

    memset(&t, 0, sizeof(t));

    /* Push 2 more than the depth; the 2 oldest are dropped */
    for (int i = 0; i < 18; i++)
    {
        t.sample_index = (uint64_t)(i + 1) * 10;
        stream_push_tx_target(&s, &t);
    }

    TEST_CHECK(rig_stream_pop_tx_target(&s, 10000, &out) == 1);
    TEST_CHECK_(out.sample_index == 30,
                "idx=%llu", (unsigned long long)out.sample_index);

    int popped = 1;

    while (rig_stream_pop_tx_target(&s, 10000, &out) == 1)
    {
        popped++;
    }

    TEST_CHECK_(popped == 16, "popped=%d", popped);

    stream_teardown(&s);
}


static void test_channels_validated_at_open(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    rig_stream_t *st = NULL;
    struct rig_stream_config cfg;

    TEST_ASSERT(rig != NULL);
    TEST_ASSERT(rig_open(rig) == RIG_OK);

    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_S16;
    cfg.sample_rate = 48000;

    /* Dummy audio supports 1-2 channels */
    cfg.channels = 2;
    TEST_CHECK(rig_stream_open(rig, &cfg, &st) == RIG_OK);
    rig_stream_close(rig, st);

    cfg.channels = 4;
    st = NULL;
    TEST_CHECK(rig_stream_open(rig, &cfg, &st) == -RIG_EINVAL);

    rig_close(rig);
    rig_cleanup(rig);
}


TEST_LIST =
{
    { "write_total_counts_all_bytes",   test_write_total_counts_all_bytes },
    { "samples_written",                test_samples_written },
    { "tx_target_push_pop",             test_tx_target_push_pop },
    { "tx_target_drop_oldest",          test_tx_target_drop_oldest },
    { "first_readable_index_basic",     test_first_readable_index_basic },
    { "first_readable_index_overrun",   test_first_readable_index_jumps_on_overrun },
    { "mark_gap_sized",                 test_mark_gap_sized },
    { "mark_gap_unsized",               test_mark_gap_unsized },
    { "consume_account_gap",            test_consume_account_gap_attribution },
    { "consume_account_overrun",        test_consume_account_overrun_attribution },
    { "consume_account_mixed",          test_consume_account_mixed_causes },
    { "skip_samples_link",              test_skip_samples_link_attribution },
    { "get_stats_snapshot",             test_get_stats_snapshot },
    { "write_status_fifo",              test_write_status_fifo },
    { "anchor_push_get",                test_anchor_push_get },
    { "anchor_ring_drops_oldest",       test_anchor_ring_drops_oldest },
    { "read_time_interpolation",        test_read_time_interpolation },
    { "read_time_watchdog",             test_read_time_watchdog_degrades },
    { "read_time_discontinuity",        test_read_time_discontinuity_flag },
    { "stale_threshold_resolution",     test_stale_threshold_resolution_at_open },
    { "channels_validated_at_open",     test_channels_validated_at_open },
    { NULL, NULL }
};
