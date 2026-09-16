/*
 *  Hamlib streaming subsystem - packet reorder window tests
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

/* Unit tests for the sequence reorder window (stream_reorder). */

#include "acutest.h"
#include "test_debug.h"

#include <string.h>

#include "stream_reorder.h"


/* A one-byte payload carrying the low byte of its sequence, so every released
 * packet can be checked against the sequence it claims. */
static int push_seq(struct stream_reorder *r, uint32_t seq, int64_t now)
{
    uint8_t b = (uint8_t)seq;
    return stream_reorder_push(r, seq, &b, 1, now);
}

/* Pop one item and describe it as P<seq>, G<seq>x<lost>, U<seq> or "-". */
static const char *pop_str(struct stream_reorder *r, int64_t now, char *buf,
                           size_t len)
{
    struct stream_reorder_item it;

    switch (stream_reorder_pop(r, now, &it))
    {
    case STREAM_REORDER_PACKET:
        TEST_CHECK_(it.length == 1 && it.data[0] == (uint8_t)it.seq,
                    "payload of %u does not match its sequence", it.seq);
        snprintf(buf, len, "P%u", it.seq);
        break;

    case STREAM_REORDER_GAP:
        if (it.unsized)
        {
            snprintf(buf, len, "U%u", it.seq);
        }
        else
        {
            snprintf(buf, len, "G%ux%u", it.seq, it.lost);
        }

        break;

    default:
        snprintf(buf, len, "-");
        break;
    }

    return buf;
}

/* Pop everything ready now into a space-separated string. */
static const char *drain(struct stream_reorder *r, int64_t now, char *out,
                         size_t len)
{
    char one[32];

    out[0] = '\0';

    for (;;)
    {
        pop_str(r, now, one, sizeof(one));

        if (strcmp(one, "-") == 0)
        {
            break;
        }

        if (out[0] != '\0')
        {
            strncat(out, " ", len - strlen(out) - 1);
        }

        strncat(out, one, len - strlen(out) - 1);
    }

    return out;
}

#define CHECK_DRAIN(r, now, expect)                                   \
    do {                                                              \
        char _b[256];                                                 \
        drain((r), (now), _b, sizeof(_b));                            \
        TEST_CHECK_(strcmp(_b, (expect)) == 0,                        \
                    "released \"%s\", expected \"%s\"", _b, (expect)); \
    } while (0)


static void test_window0_in_order(void)
{
    struct stream_reorder *r = stream_reorder_new(0, 16, 64, 50, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 100, 0);
    CHECK_DRAIN(r, 0, "P100");
    push_seq(r, 101, 1);
    CHECK_DRAIN(r, 1, "P101");

    /* A gap is reported as soon as the packet after it arrives. */
    push_seq(r, 104, 2);
    CHECK_DRAIN(r, 2, "G102x2 P104");

    /* Late (the given-up ones) and duplicates never come out. */
    TEST_CHECK(push_seq(r, 103, 3) == STREAM_REORDER_LATE);
    TEST_CHECK(push_seq(r, 104, 3) == STREAM_REORDER_LATE);
    CHECK_DRAIN(r, 3, "");
    TEST_CHECK(stream_reorder_next_deadline(r, 3) == -1);

    struct stream_reorder_stats st;
    stream_reorder_get_stats(r, &st);
    TEST_CHECK(st.released == 3);
    TEST_CHECK(st.late == 2);
    TEST_CHECK(st.gaps == 1);
    TEST_CHECK(st.lost == 2);

    /* Nothing to request: a resend could never be used. */
    uint32_t miss[8];
    push_seq(r, 107, 4);
    TEST_CHECK(stream_reorder_missing(r, 4, 0, miss, 8) == 0);

    stream_reorder_free(r);
}


static void test_window_reorders(void)
{
    struct stream_reorder *r = stream_reorder_new(50, 16, 64, 50, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 10, 0);
    CHECK_DRAIN(r, 0, "P10");

    /* 12 before 11: held, not a gap. */
    push_seq(r, 12, 10);
    CHECK_DRAIN(r, 10, "");
    TEST_CHECK(stream_reorder_next_deadline(r, 10) == 50);
    TEST_CHECK(stream_reorder_next_deadline(r, 40) == 20);

    push_seq(r, 11, 20);
    CHECK_DRAIN(r, 20, "P11 P12");

    struct stream_reorder_stats st;
    stream_reorder_get_stats(r, &st);
    TEST_CHECK(st.gaps == 0);
    TEST_CHECK(st.released == 3);

    stream_reorder_free(r);
}


static void test_window_deadline_gives_up(void)
{
    struct stream_reorder *r = stream_reorder_new(50, 16, 64, 50, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 1, 0);
    CHECK_DRAIN(r, 0, "P1");

    push_seq(r, 4, 100);
    push_seq(r, 5, 110);
    CHECK_DRAIN(r, 149, "");
    /* The wait is measured from the packet after the hole (seq 4 at 100). */
    CHECK_DRAIN(r, 150, "G2x2 P4 P5");

    /* The missing packet turns up after it was given up: dropped. */
    TEST_CHECK(push_seq(r, 3, 160) == STREAM_REORDER_LATE);
    CHECK_DRAIN(r, 160, "");

    stream_reorder_free(r);
}


static void test_missing_list_and_period(void)
{
    struct stream_reorder *r = stream_reorder_new(300, 16, 64, 50, 16);
    uint32_t miss[8];
    TEST_ASSERT(r != NULL);

    push_seq(r, 1, 0);
    CHECK_DRAIN(r, 0, "P1");
    push_seq(r, 4, 10);

    TEST_CHECK(stream_reorder_missing(r, 10, 100, miss, 8) == 2);
    TEST_CHECK(miss[0] == 2 && miss[1] == 3);

    /* Not listed again within the period... */
    TEST_CHECK(stream_reorder_missing(r, 50, 100, miss, 8) == 0);
    /* ...but again after it. */
    TEST_CHECK(stream_reorder_missing(r, 110, 100, miss, 8) == 2);

    /* A recovered packet drops out of the list and releases in order. */
    push_seq(r, 2, 120);
    TEST_CHECK(stream_reorder_missing(r, 220, 100, miss, 8) == 1);
    TEST_CHECK(miss[0] == 3);
    push_seq(r, 3, 230);
    CHECK_DRAIN(r, 230, "P2 P3 P4");

    /* Past the deadline a hole is not worth asking for. */
    push_seq(r, 7, 240);
    TEST_CHECK(stream_reorder_missing(r, 540, 100, miss, 8) == 0);
    CHECK_DRAIN(r, 540, "G5x2 P7");

    stream_reorder_free(r);
}


/* When the next request falls due, so a receiver can sleep until then. */
static void test_next_request(void)
{
    struct stream_reorder *r = stream_reorder_new(100, 16, 64, 50, 16);
    struct stream_reorder *r0 = stream_reorder_new(0, 16, 64, 50, 16);
    uint32_t miss[8];
    TEST_ASSERT(r != NULL && r0 != NULL);

    TEST_CHECK(stream_reorder_next_request(r, 0, 30) == -1);
    push_seq(r, 1, 0);
    CHECK_DRAIN(r, 0, "P1");
    TEST_CHECK(stream_reorder_next_request(r, 5, 30) == -1);

    /* A new hole is due at once; the deadline is 110. */
    push_seq(r, 4, 10);
    TEST_CHECK(stream_reorder_next_request(r, 10, 30) == 0);
    TEST_CHECK(stream_reorder_missing(r, 10, 30, miss, 8) == 2);
    TEST_CHECK(stream_reorder_next_request(r, 15, 30) == 25);
    TEST_CHECK(stream_reorder_next_request(r, 40, 30) == 0);
    TEST_CHECK(stream_reorder_missing(r, 40, 30, miss, 8) == 2);
    TEST_CHECK(stream_reorder_missing(r, 70, 30, miss, 8) == 2);
    TEST_CHECK(stream_reorder_next_request(r, 80, 30) == 20);
    TEST_CHECK(stream_reorder_missing(r, 100, 30, miss, 8) == 2);

    /* The next one would be due at 130, after the deadline. */
    TEST_CHECK(stream_reorder_next_request(r, 100, 30) == -1);

    /* The earliest of several holes counts; recovered ones drop out. */
    push_seq(r, 2, 105);
    push_seq(r, 3, 106);
    CHECK_DRAIN(r, 106, "P2 P3 P4");
    push_seq(r, 6, 120);
    TEST_CHECK(stream_reorder_missing(r, 120, 30, miss, 8) == 1);
    push_seq(r, 8, 135);
    TEST_CHECK(stream_reorder_next_request(r, 140, 30) == 0);
    TEST_CHECK(stream_reorder_missing(r, 140, 30, miss, 8) == 1);
    TEST_CHECK(miss[0] == 7);
    TEST_CHECK(stream_reorder_next_request(r, 141, 30) == 9);
    push_seq(r, 5, 142);
    TEST_CHECK(stream_reorder_next_request(r, 142, 30) == 28);
    push_seq(r, 7, 143);
    CHECK_DRAIN(r, 143, "P5 P6 P7 P8");
    TEST_CHECK(stream_reorder_next_request(r, 143, 30) == -1);

    /* Nothing is ever requested without a window. */
    push_seq(r0, 1, 0);
    push_seq(r0, 3, 1);
    TEST_CHECK(stream_reorder_next_request(r0, 1, 30) == -1);

    stream_reorder_free(r);
    stream_reorder_free(r0);
}


static void test_duplicates(void)
{
    struct stream_reorder *r = stream_reorder_new(100, 16, 64, 50, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 1, 0);
    push_seq(r, 3, 0);
    TEST_CHECK(push_seq(r, 3, 1) == STREAM_REORDER_DUPLICATE);
    CHECK_DRAIN(r, 1, "P1");
    TEST_CHECK(push_seq(r, 1, 2) == STREAM_REORDER_LATE);

    struct stream_reorder_stats st;
    stream_reorder_get_stats(r, &st);
    TEST_CHECK(st.duplicates == 1);
    TEST_CHECK(st.late == 1);

    stream_reorder_free(r);
}


static void test_wrap_16(void)
{
    struct stream_reorder *r = stream_reorder_new(20, 16, 64, 50, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 0xfffe, 0);
    push_seq(r, 0x0000, 1);
    push_seq(r, 0xffff, 2);
    CHECK_DRAIN(r, 2, "P65534 P65535 P0");
    push_seq(r, 0x0002, 3);
    CHECK_DRAIN(r, 23, "G1x1 P2");

    stream_reorder_free(r);
}


static void test_wrap_4_bit(void)
{
    /* VITA-49 style 4-bit counter: one reordered packet must not read as a
     * 15-packet gap. */
    struct stream_reorder *r = stream_reorder_new(20, 4, 8, 6, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 14, 0);
    CHECK_DRAIN(r, 0, "P14");
    push_seq(r, 0, 1);        /* 15 is late on the wire */
    push_seq(r, 15, 2);
    CHECK_DRAIN(r, 2, "P15 P0");

    struct stream_reorder_stats st;
    stream_reorder_get_stats(r, &st);
    TEST_CHECK(st.gaps == 0);

    stream_reorder_free(r);
}


static void test_wrap_32(void)
{
    struct stream_reorder *r = stream_reorder_new(0, 32, 64, 50, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 0xffffffffu, 0);
    push_seq(r, 1, 0);
    CHECK_DRAIN(r, 0, "P4294967295 G0x1 P1");

    stream_reorder_free(r);
}


static void test_resync_on_jump(void)
{
    struct stream_reorder *r = stream_reorder_new(100, 16, 256, 50, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 1, 0);
    push_seq(r, 3, 0);                     /* held, waiting for 2 */
    TEST_CHECK(push_seq(r, 1000, 1) == STREAM_REORDER_RESYNC);
    CHECK_DRAIN(r, 1, "U1000 P1000");
    push_seq(r, 1001, 2);
    CHECK_DRAIN(r, 2, "P1001");

    struct stream_reorder_stats st;
    stream_reorder_get_stats(r, &st);
    TEST_CHECK(st.resyncs == 1);

    stream_reorder_free(r);
}


static void test_resync_on_counter_restart(void)
{
    /* A sender that restarts its counter looks late at first. From further
     * back than max_gap, a second such packet in a row settles it; a stray
     * old copy alone does not. */
    struct stream_reorder *r = stream_reorder_new(0, 16, 64, 8, 16);
    uint32_t s;
    TEST_ASSERT(r != NULL);

    push_seq(r, 40000, 0);
    CHECK_DRAIN(r, 0, "P40000");

    /* 39000.. is behind 40001 in the 16-bit space (0 would be ahead). */
    TEST_CHECK(push_seq(r, 39000, 1) == STREAM_REORDER_LATE);
    TEST_CHECK(push_seq(r, 40001, 1) == STREAM_REORDER_ACCEPTED);
    CHECK_DRAIN(r, 1, "P40001");
    TEST_CHECK(push_seq(r, 39001, 1) == STREAM_REORDER_LATE);
    TEST_CHECK(push_seq(r, 39002, 1) == STREAM_REORDER_RESYNC);
    CHECK_DRAIN(r, 1, "U39002 P39002");

    /* Close behind, only a run longer than max_gap is a restart. */
    stream_reorder_free(r);
    r = stream_reorder_new(0, 16, 64, 8, 16);
    TEST_ASSERT(r != NULL);

    for (s = 10; s <= 20; s++)
    {
        push_seq(r, s, 0);
    }

    CHECK_DRAIN(r, 0, "P10 P11 P12 P13 P14 P15 P16 P17 P18 P19 P20");

    for (s = 13; s <= 20; s++)
    {
        TEST_CHECK(push_seq(r, s, 1) == STREAM_REORDER_LATE);
    }

    TEST_CHECK(push_seq(r, 20, 1) == STREAM_REORDER_RESYNC);

    stream_reorder_free(r);
}


static void test_capacity_forces_release(void)
{
    /* Only 4 positions can be held: a packet further ahead forces older ones
     * out early, in order, never silently. max_gap 0 disables resync. */
    struct stream_reorder *r = stream_reorder_new(1000, 16, 4, 0, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 1, 0);
    CHECK_DRAIN(r, 0, "P1");
    push_seq(r, 3, 1);                     /* waiting for 2 */
    push_seq(r, 4, 1);
    TEST_CHECK(push_seq(r, 9, 2) == STREAM_REORDER_ACCEPTED);
    /* 9 cannot be held: 2 is given up at once, 3 and 4 released, and the
     * distance to 9 is a sized gap. */
    TEST_CHECK(stream_reorder_next_deadline(r, 2) == 0);
    CHECK_DRAIN(r, 2, "G2x1 P3 P4 G5x4 P9");

    /* While a forced release is pending, a new push is refused. */
    push_seq(r, 11, 3);
    push_seq(r, 20, 3);
    TEST_CHECK(push_seq(r, 21, 3) == STREAM_REORDER_DROPPED);
    CHECK_DRAIN(r, 3, "G10x1 P11 G12x8 P20");

    stream_reorder_free(r);
}


static void test_jump_with_nothing_held(void)
{
    struct stream_reorder *r = stream_reorder_new(0, 16, 4, 0, 16);
    TEST_ASSERT(r != NULL);

    push_seq(r, 1, 0);
    CHECK_DRAIN(r, 0, "P1");
    push_seq(r, 30, 1);
    CHECK_DRAIN(r, 1, "G2x28 P30");

    stream_reorder_free(r);
}


static void test_reset_and_bad_args(void)
{
    struct stream_reorder *r;
    uint8_t big[32];

    TEST_CHECK(stream_reorder_new(0, 0, 8, 0, 16) == NULL);
    TEST_CHECK(stream_reorder_new(0, 33, 8, 0, 16) == NULL);
    TEST_CHECK(stream_reorder_new(0, 16, 0, 0, 16) == NULL);
    TEST_CHECK(stream_reorder_new(0, 16, 8, 0, 0) == NULL);

    r = stream_reorder_new(10, 16, 8, 0, 16);
    TEST_ASSERT(r != NULL);
    TEST_CHECK(stream_reorder_window_ms(r) == 10);

    memset(big, 0, sizeof(big));
    TEST_CHECK(stream_reorder_push(r, 1, big, sizeof(big), 0)
               == STREAM_REORDER_DROPPED);

    push_seq(r, 5, 0);
    push_seq(r, 7, 0);
    stream_reorder_reset(r);
    TEST_CHECK(stream_reorder_next_deadline(r, 0) == -1);
    push_seq(r, 500, 1);
    CHECK_DRAIN(r, 1, "P500");

    stream_reorder_free(r);
    stream_reorder_free(NULL);
}


TEST_LIST =
{
    { "window0_in_order",           test_window0_in_order },
    { "window_reorders",            test_window_reorders },
    { "window_deadline_gives_up",   test_window_deadline_gives_up },
    { "missing_list_and_period",    test_missing_list_and_period },
    { "next_request",               test_next_request },
    { "duplicates",                 test_duplicates },
    { "wrap_16",                    test_wrap_16 },
    { "wrap_4_bit",                 test_wrap_4_bit },
    { "wrap_32",                    test_wrap_32 },
    { "resync_on_jump",             test_resync_on_jump },
    { "resync_on_counter_restart",  test_resync_on_counter_restart },
    { "capacity_forces_release",    test_capacity_forces_release },
    { "jump_with_nothing_held",     test_jump_with_nothing_held },
    { "reset_and_bad_args",         test_reset_and_bad_args },
    { NULL, NULL }
};
