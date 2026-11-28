/*
 *  Hamlib Icom network sequence-buffer tests
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

/* Unit tests for the Icom network protocol per-socket sequence bookkeeping. */
/* Covers the TX replay buffer and the RX gap/missing/retransmit tracker. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"
#include "network_seqbuf.h"
#include <string.h>
#include <stdint.h>


/* --- TX replay buffer --- */

/* The stored packet with this sequence, or NULL. */
static const uint8_t *stored(struct icom_network_txbuf *tb, uint16_t sequence,
                             size_t *length)
{
    size_t cursor = 0;

    return icom_network_txbuf_next_in_range(tb, sequence, sequence, &cursor,
                                            length);
}

void test_txbuf_add_get(void)
{
    struct icom_network_txbuf tb;
    uint8_t a[3] = { 0xAA, 0xBB, 0xCC };
    uint8_t b[2] = { 0x11, 0x22 };
    const uint8_t *p;
    size_t length = 0;

    icom_network_txbuf_init(&tb);
    TEST_CHECK(icom_network_txbuf_add(&tb, 1, a, 3, 0) == 0);
    TEST_CHECK(icom_network_txbuf_add(&tb, 2, b, 2, 0) == 0);

    p = stored(&tb, 1, &length);
    TEST_CHECK(p != NULL && length == 3 && memcmp(p, a, 3) == 0);
    p = stored(&tb, 2, &length);
    TEST_CHECK(p != NULL && length == 2 && memcmp(p, b, 2) == 0);
    TEST_CHECK(stored(&tb, 99, &length) == NULL);
}

void test_txbuf_purge(void)
{
    struct icom_network_txbuf tb;
    uint8_t a[1] = { 0x5 };

    icom_network_txbuf_init(&tb);
    icom_network_txbuf_add(&tb, 7, a, 1, 1000);
    TEST_CHECK(stored(&tb, 7, NULL) != NULL);

    icom_network_txbuf_purge(&tb, 5000, 10000);     /* not old enough */
    TEST_CHECK(stored(&tb, 7, NULL) != NULL);
    icom_network_txbuf_purge(&tb, 12000, 10000);    /* now older than 10s */
    TEST_CHECK(stored(&tb, 7, NULL) == NULL);
}

void test_txbuf_overwrite_oldest(void)
{
    struct icom_network_txbuf tb;
    uint8_t d[1] = { 0 };
    int i;

    icom_network_txbuf_init(&tb);

    /* fill exactly MAX, then one more -> the first sequence is overwritten */
    for (i = 0; i < ICOM_NETWORK_SEQBUF_MAX; i++)
    {
        icom_network_txbuf_add(&tb, (uint16_t)(1000 + i), d, 1, 0);
    }
    TEST_CHECK(stored(&tb, 1000, NULL) != NULL);

    icom_network_txbuf_add(&tb, 9999, d, 1, 0);
    TEST_CHECK(stored(&tb, 1000, NULL) == NULL);
    TEST_CHECK(stored(&tb, 9999, NULL) != NULL);

    /* too-large packet not stored */
    TEST_CHECK(icom_network_txbuf_add(&tb, 1, d,
               ICOM_NETWORK_SEQBUF_PKTMAX + 1, 0) == -1);
}

void test_txbuf_oversize_rejected(void)
{
    struct icom_network_txbuf tb;
    static uint8_t big[ICOM_NETWORK_SEQBUF_PKTMAX + 8];
    icom_network_txbuf_init(&tb);
    TEST_CHECK(icom_network_txbuf_add(&tb, 1, big, sizeof(big), 0) == -1);
}

/* A range across 0xffff, in a buffer that has wrapped as well, comes out
 * whole and in the order it was sent. */
void test_txbuf_range_wraps(void)
{
    struct icom_network_txbuf tb;
    uint8_t d[1];
    const uint8_t *p;
    size_t cursor = 0, length = 0;
    int i, found = 0;

    icom_network_txbuf_init(&tb);

    /* 0xff00 ... 0xffff, 0, 1, 2: the first three slots are reused */
    for (i = 0; i < ICOM_NETWORK_SEQBUF_MAX + 3; i++)
    {
        uint16_t sequence = (uint16_t)(0xff00 + i);

        d[0] = (uint8_t)sequence;
        icom_network_txbuf_add(&tb, sequence, d, 1, 0);
    }

    while ((p = icom_network_txbuf_next_in_range(&tb, 0xfffe, 0x0001, &cursor,
                &length)) != NULL)
    {
        TEST_CHECK(length == 1 && p[0] == (uint8_t)(0xfe + found));
        found++;
    }

    TEST_CHECK(found == 4);
}


/* --- RX gap / retransmit tracker --- */

void test_rxtrack_sequential(void)
{
    struct icom_network_rxtrack rt;
    icom_network_rxtrack_init(&rt);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 10, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 11, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 12, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(rt.nmissing == 0);
}

void test_rxtrack_gap_and_recover(void)
{
    struct icom_network_rxtrack rt;
    icom_network_rxtrack_init(&rt);

    icom_network_rxtrack_observe(&rt, 10, 0, 0);
    icom_network_rxtrack_observe(&rt, 13, 0, 0);   /* 11, 12 missing */
    TEST_CHECK(rt.nmissing == 2);

    /* a retransmit of 11 arrives (behind last_sequence=13) */
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 11, 0, 0)
               == ICOM_NETWORK_RX_RECOVERED);
    TEST_CHECK(rt.nmissing == 1);

    icom_network_rxtrack_received(&rt, 12);
    TEST_CHECK(rt.nmissing == 0);
}

void test_rxtrack_due_and_retry_cap(void)
{
    struct icom_network_rxtrack rt;
    uint16_t out[8];
    int k;

    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 10, 0, 0);
    icom_network_rxtrack_observe(&rt, 13, 0, 0);   /* 11, 12 missing */

    /* due immediately on first poll */
    TEST_CHECK(icom_network_rxtrack_due(&rt, 0, 100, out, 8) == 2);
    /* not due again before the period elapses */
    TEST_CHECK(icom_network_rxtrack_due(&rt, 50, 100, out, 8) == 0);

    /* retries 2,3,4 at successive periods */
    for (k = 2; k <= ICOM_NETWORK_RETRANSMIT_MAX; k++)
    {
        TEST_CHECK(icom_network_rxtrack_due(&rt, k * 100, 100, out, 8) == 2);
    }

    /* now past the retry cap -> entries dropped, nothing returned */
    TEST_CHECK(icom_network_rxtrack_due(&rt, 1000, 100, out, 8) == 0);
    TEST_CHECK(rt.nmissing == 0);
}

void test_rxtrack_flush_on_large_jump(void)
{
    struct icom_network_rxtrack rt;
    uint16_t seq;

    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 10, 0, 0);
    /* jump beyond the flush threshold triggers a resync */
    TEST_CHECK(icom_network_rxtrack_observe(&rt,
               (uint16_t)(10 + ICOM_NETWORK_MISSING_FLUSH + 5), 0, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(rt.nmissing == 0);
    /* the same stream goes on: a copy of a packet from before the jump is
     * still a copy */
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 10, 0, 0)
               == ICOM_NETWORK_RX_DUPLICATE);

    /* more gaps than the threshold, one packet apart: they are dropped too,
     * and the stream goes on */
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 10, 0, 0);

    for (seq = 12; seq < 10 + 2 * (ICOM_NETWORK_MISSING_FLUSH + 1); seq += 2)
    {
        icom_network_rxtrack_observe(&rt, seq, 0, 0);
    }

    TEST_CHECK(rt.nmissing == ICOM_NETWORK_MISSING_FLUSH);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, seq, 0, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK_(rt.nmissing == 0, "%zu gaps left", rt.nmissing);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 10, 0, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
}

void test_rxtrack_sequence_wrap(void)
{
    struct icom_network_rxtrack rt;
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 0xfffe, 0, 0);
    icom_network_rxtrack_observe(&rt, 0x0001, 0, 0); /* wraps: 0xffff, 0x0000 missing */
    TEST_CHECK(rt.nmissing == 2);
    icom_network_rxtrack_received(&rt, 0xffff);
    icom_network_rxtrack_received(&rt, 0x0000);
    TEST_CHECK(rt.nmissing == 0);
}

/* A packet already received is recognised as a duplicate, whether it is the
 * newest one again or an older one; a missing one is not. */
void test_rxtrack_duplicates(void)
{
    struct icom_network_rxtrack rt;
    icom_network_rxtrack_init(&rt);

    icom_network_rxtrack_observe(&rt, 10, 0, 0);
    icom_network_rxtrack_observe(&rt, 11, 0, 0);
    icom_network_rxtrack_observe(&rt, 14, 0, 0);   /* 12, 13 missing */

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 14, 0, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 11, 0, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 12, 0, 0)
               == ICOM_NETWORK_RX_RECOVERED);
    /* recovered once; a second copy is a duplicate */
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 12, 0, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(rt.nmissing == 1);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 15, 0, 0) == ICOM_NETWORK_RX_NEW);

    /* across the wrap */
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 0xffff, 0, 0);
    icom_network_rxtrack_observe(&rt, 0x0000, 0, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 0xffff, 0, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
}

/* A reopened stream: the new session first gets the previous client's leftover
 * packets with their old, high numbers, then the new stream counting from
 * zero. None of the new packets may be taken for duplicates -- even those
 * within reach of the old numbers -- and tracking starts again from them. */
void test_rxtrack_restart_is_resync(void)
{
    struct icom_network_rxtrack rt;
    uint16_t out[8];
    uint16_t seq;

    /* short old stream: the leftovers are numbered 190..200 */
    icom_network_rxtrack_init(&rt);

    for (seq = 190; seq <= 200; seq++)
    {
        icom_network_rxtrack_observe(&rt, seq, 0, 0);
    }

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 3, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0, 0) == ICOM_NETWORK_RX_RESYNC);
    /* tracking follows the new stream: a gap there is noticed */
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 5, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 7, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_due(&rt, 0, 100, out, 8) == 1 && out[0] == 6);

    /* a new stream within the old numbers: told by the content */
    icom_network_rxtrack_init(&rt);

    for (seq = 0; seq <= 10; seq++)
    {
        icom_network_rxtrack_observe(&rt, seq, 0xaa, 0);
    }

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 3, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    /* the packets that showed the restart are the new stream's: their
     * copies are copies */
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 3, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);

    /* long old stream: far behind is a restart at once */
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 5000, 0, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0, 0) == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0, 0) == ICOM_NETWORK_RX_NEW);

    /* one packet given up on and arriving afterwards is delivered, not
     * dropped, and is no restart */
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 10, 0, 0);
    icom_network_rxtrack_observe(&rt, 13, 0, 0);   /* 11, 12 missing */
    rt.nmissing = 0;                            /* as if given up */
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 11, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 11, 0, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 14, 0, 0) == ICOM_NETWORK_RX_NEW);
}

/* After a restart, tracking goes on as if the packets that showed it had come
 * in order, wherever in the run they came. What was received stays known:
 * with no stream in the packet, a new one byte for byte the same as one
 * received with its number before is taken for a copy. */
void test_rxtrack_restart_starts_from_its_packets(void)
{
    struct icom_network_rxtrack rt;
    uint16_t out[8];
    uint16_t seq;

    /* reordered: the restart shows on 3, after 4 */
    icom_network_rxtrack_init(&rt);

    for (seq = 0; seq <= 10; seq++)
    {
        icom_network_rxtrack_observe(&rt, seq, 0xaa, 0);
    }

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 3, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 5, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_due(&rt, 0, 100, out, 8) == 0);

    /* the new stream's packets received before the run stay received: a
     * copy of one is a copy */
    icom_network_rxtrack_init(&rt);

    for (seq = 0; seq <= 10; seq++)
    {
        if (seq != 2) { icom_network_rxtrack_observe(&rt, seq, 0xaa, 0); }
    }

    icom_network_rxtrack_observe(&rt, 1, 0xbb, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_RECOVERED);

    for (seq = 3; seq <= 5; seq++)
    {
        icom_network_rxtrack_observe(&rt, seq, 0xbb, 0);
    }

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 6, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);

    /* nor does the replay know one received before the run: a gap is left
     * for it, and its retransmits are copies, asked for until given up on */
    icom_network_rxtrack_init(&rt);

    for (seq = 0; seq <= 10; seq++)
    {
        if (seq != 4) { icom_network_rxtrack_observe(&rt, seq, 0xaa, 0); }
    }

    icom_network_rxtrack_observe(&rt, 1, 0xbb, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_RECOVERED);
    icom_network_rxtrack_observe(&rt, 2, 0xbb, 0);
    icom_network_rxtrack_observe(&rt, 3, 0xbb, 0);
    icom_network_rxtrack_observe(&rt, 5, 0xbb, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 6, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    {
        int64_t now;

        for (now = 0; now < 100 * ICOM_NETWORK_RETRANSMIT_MAX; now += 100)
        {
            TEST_CHECK(icom_network_rxtrack_due(&rt, now, 100, out, 8) == 1
                       && out[0] == 4);
            TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, now)
                       == ICOM_NETWORK_RX_DUPLICATE);
        }

        TEST_CHECK_(icom_network_rxtrack_due(&rt, now, 100, out, 8) == 0,
                    "4 given up on");
    }

    /* the price: a packet of the new stream byte for byte the same as the
     * old stream's with its number is dropped, late or in order */
    icom_network_rxtrack_init(&rt);

    for (seq = 0; seq <= 10; seq++)
    {
        icom_network_rxtrack_observe(&rt, seq, 0xaa, 0);
    }

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 3, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 5, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xaa, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 6, 0xaa, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 7, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);

    /* a number missing among them is a gap, asked for again */
    icom_network_rxtrack_init(&rt);

    for (seq = 0; seq <= 10; seq++)
    {
        icom_network_rxtrack_observe(&rt, seq, 0xaa, 0);
    }

    icom_network_rxtrack_observe(&rt, 1, 0xbb, 0);
    icom_network_rxtrack_observe(&rt, 2, 0xbb, 0);
    icom_network_rxtrack_observe(&rt, 5, 0xbb, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 3, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 6, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK_(icom_network_rxtrack_due(&rt, 0, 100, out, 8) == 1
                && out[0] == 4, "4 asked for");
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_RECOVERED);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 4, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);

    /* the whole run is the new stream's, however far apart, with the gaps
     * between them as for any packets */
    icom_network_rxtrack_init(&rt);

    for (seq = 0; seq <= 100; seq++)
    {
        icom_network_rxtrack_observe(&rt, seq, 0xaa, 0);
    }

    icom_network_rxtrack_observe(&rt, 1, 0xbb, 0);
    icom_network_rxtrack_observe(&rt, 2, 0xbb, 0);
    icom_network_rxtrack_observe(&rt, 3, 0xbb, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 52, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    {
        uint16_t gaps[64];
        size_t n = icom_network_rxtrack_due(&rt, 0, 100, gaps, 64);
        size_t i;

        TEST_CHECK_(n == 48, "%zu gaps, expected 4..51", n);

        for (i = 0; i < n && i < 48; i++)
        {
            TEST_CHECK_(gaps[i] == 4 + i, "gap %zu is %u", i, gaps[i]);
        }
    }

    /* far behind, it shows the restart alone; a packet just before it that
     * is near it is the new stream's too, ... */
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 258, 0xaa, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);

    /* ... but not one far from it: the old stream's, late */
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 1000, 0xaa, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 990, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_RESYNC);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 2, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(rt.last_sequence == 2);
    TEST_CHECK(icom_network_rxtrack_due(&rt, 0, 100, out, 8) == 0);
}

/* Expecting the first sequence makes a lost first packet a gap like any
 * other. */
void test_rxtrack_expect_first(void)
{
    struct icom_network_rxtrack rt;
    uint16_t out[8];

    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_expect(&rt, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_due(&rt, 0, 100, out, 8) == 1 && out[0] == 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 0, 0, 0)
               == ICOM_NETWORK_RX_RECOVERED);

    /* and a stream that does start at the expected number has no gap */
    icom_network_rxtrack_expect(&rt, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 0, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(rt.nmissing == 0);
}

/* A packet numbered as one already received, with other content, is no copy
 * of it: a radio still holding the previous client's stream may send one
 * packet of that stream, numbered past the start, around the opening of the
 * new one. */
void test_rxtrack_same_number_other_content(void)
{
    struct icom_network_rxtrack rt;
    uint16_t seq;

    /* the other stream's packet 23 before the first packet */
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_expect(&rt, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 23, 0xaa, 0)
               == ICOM_NETWORK_RX_NEW);

    for (seq = 0; seq <= 22; seq++)
    {
        TEST_CHECK_(icom_network_rxtrack_observe(&rt, seq, 0, 0)
                    == ICOM_NETWORK_RX_RECOVERED, "packet %u", seq);
    }

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 23, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 23, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 24, 0, 0) == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(rt.nmissing == 0);

    /* the same after the first packet */
    icom_network_rxtrack_expect(&rt, 0);
    icom_network_rxtrack_observe(&rt, 0, 0, 0);
    icom_network_rxtrack_observe(&rt, 3, 0xaa, 0);
    icom_network_rxtrack_observe(&rt, 1, 0, 0);
    icom_network_rxtrack_observe(&rt, 2, 0, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 3, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
}

/* The first packet arriving after the second does not make the second new
 * again: a second copy of it is still a copy, so a repeated reply or ACK is
 * never taken for the answer to the next command. Nor does another packet
 * with its number, arriving in between (the previous client's). */
void test_rxtrack_copy_of_packet_before_first(void)
{
    struct icom_network_rxtrack rt;

    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_expect(&rt, 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xaa, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 0, 0, 0)
               == ICOM_NETWORK_RX_RECOVERED);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xaa, 0)
               == ICOM_NETWORK_RX_DUPLICATE);

    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_NEW);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xaa, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 1, 0xbb, 0)
               == ICOM_NETWORK_RX_DUPLICATE);
}


TEST_LIST =
{
    { "txbuf_add_get",            test_txbuf_add_get },
    { "txbuf_purge",              test_txbuf_purge },
    { "txbuf_overwrite_oldest",   test_txbuf_overwrite_oldest },
    { "txbuf_oversize_rejected",  test_txbuf_oversize_rejected },
    { "txbuf_range_wraps",        test_txbuf_range_wraps },
    { "rxtrack_sequential",       test_rxtrack_sequential },
    { "rxtrack_gap_and_recover",  test_rxtrack_gap_and_recover },
    { "rxtrack_due_and_retry_cap", test_rxtrack_due_and_retry_cap },
    { "rxtrack_flush_large_jump", test_rxtrack_flush_on_large_jump },
    { "rxtrack_sequence_wrap",         test_rxtrack_sequence_wrap },
    { "rxtrack_duplicates",       test_rxtrack_duplicates },
    { "rxtrack_restart_is_resync", test_rxtrack_restart_is_resync },
    { "rxtrack_restart_starts_from_its_packets", test_rxtrack_restart_starts_from_its_packets },
    { "rxtrack_expect_first",     test_rxtrack_expect_first },
    { "rxtrack_same_number_other_content", test_rxtrack_same_number_other_content },
    { "rxtrack_copy_of_packet_before_first", test_rxtrack_copy_of_packet_before_first },
    { NULL, NULL }
};
