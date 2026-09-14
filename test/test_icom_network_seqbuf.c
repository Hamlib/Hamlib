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

    p = icom_network_txbuf_get(&tb, 1, &length);
    TEST_CHECK(p != NULL && length == 3 && memcmp(p, a, 3) == 0);
    p = icom_network_txbuf_get(&tb, 2, &length);
    TEST_CHECK(p != NULL && length == 2 && memcmp(p, b, 2) == 0);
    TEST_CHECK(icom_network_txbuf_get(&tb, 99, &length) == NULL);
}

void test_txbuf_purge(void)
{
    struct icom_network_txbuf tb;
    uint8_t a[1] = { 0x5 };

    icom_network_txbuf_init(&tb);
    icom_network_txbuf_add(&tb, 7, a, 1, 1000);
    TEST_CHECK(icom_network_txbuf_get(&tb, 7, NULL) != NULL);

    icom_network_txbuf_purge(&tb, 5000, 10000);     /* not old enough */
    TEST_CHECK(icom_network_txbuf_get(&tb, 7, NULL) != NULL);
    icom_network_txbuf_purge(&tb, 12000, 10000);    /* now older than 10s */
    TEST_CHECK(icom_network_txbuf_get(&tb, 7, NULL) == NULL);
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
    TEST_CHECK(icom_network_txbuf_get(&tb, 1000, NULL) != NULL);

    icom_network_txbuf_add(&tb, 9999, d, 1, 0);
    TEST_CHECK(icom_network_txbuf_get(&tb, 1000, NULL) == NULL);
    TEST_CHECK(icom_network_txbuf_get(&tb, 9999, NULL) != NULL);

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


/* --- RX gap / retransmit tracker --- */

void test_rxtrack_sequential(void)
{
    struct icom_network_rxtrack rt;
    icom_network_rxtrack_init(&rt);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 10, 0) == 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 11, 0) == 0);
    TEST_CHECK(icom_network_rxtrack_observe(&rt, 12, 0) == 0);
    TEST_CHECK(rt.nmissing == 0);
}

void test_rxtrack_gap_and_recover(void)
{
    struct icom_network_rxtrack rt;
    icom_network_rxtrack_init(&rt);

    icom_network_rxtrack_observe(&rt, 10, 0);
    icom_network_rxtrack_observe(&rt, 13, 0);   /* 11, 12 missing */
    TEST_CHECK(rt.nmissing == 2);

    /* a retransmit of 11 arrives (behind last_sequence=13) */
    icom_network_rxtrack_observe(&rt, 11, 0);
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
    icom_network_rxtrack_observe(&rt, 10, 0);
    icom_network_rxtrack_observe(&rt, 13, 0);   /* 11, 12 missing */

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
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 10, 0);
    /* jump beyond the flush threshold triggers a resync */
    TEST_CHECK(icom_network_rxtrack_observe(&rt,
               (uint16_t)(10 + ICOM_NETWORK_MISSING_FLUSH + 5), 0) == 1);
    TEST_CHECK(rt.nmissing == 0);
}

void test_rxtrack_sequence_wrap(void)
{
    struct icom_network_rxtrack rt;
    icom_network_rxtrack_init(&rt);
    icom_network_rxtrack_observe(&rt, 0xfffe, 0);
    icom_network_rxtrack_observe(&rt, 0x0001, 0); /* wraps: 0xffff, 0x0000 missing */
    TEST_CHECK(rt.nmissing == 2);
    icom_network_rxtrack_received(&rt, 0xffff);
    icom_network_rxtrack_received(&rt, 0x0000);
    TEST_CHECK(rt.nmissing == 0);
}


TEST_LIST =
{
    { "txbuf_add_get",            test_txbuf_add_get },
    { "txbuf_purge",              test_txbuf_purge },
    { "txbuf_overwrite_oldest",   test_txbuf_overwrite_oldest },
    { "txbuf_oversize_rejected",  test_txbuf_oversize_rejected },
    { "rxtrack_sequential",       test_rxtrack_sequential },
    { "rxtrack_gap_and_recover",  test_rxtrack_gap_and_recover },
    { "rxtrack_due_and_retry_cap", test_rxtrack_due_and_retry_cap },
    { "rxtrack_flush_large_jump", test_rxtrack_flush_on_large_jump },
    { "rxtrack_sequence_wrap",         test_rxtrack_sequence_wrap },
    { NULL, NULL }
};
