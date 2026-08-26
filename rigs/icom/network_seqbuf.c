/*
 *  Hamlib Icom network backend - sequence bookkeeping
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

/* Per-socket sequence bookkeeping for the Icom network protocol. */
/* TX replay buffer and RX gap/missing tracking; pure logic, no sockets. */

#include "hamlib/config.h"

#include <string.h>

#include "network_seqbuf.h"

/* 16-bit modular "a is strictly after b" (handles wrap). */
static int sequence_after(uint16_t a, uint16_t b)
{
    return (int16_t)(a - b) > 0;
}

/* ------------------------------------------------------------------ */
/* TX replay buffer                                                    */
/* ------------------------------------------------------------------ */

void icom_network_txbuf_init(struct icom_network_txbuf *tb)
{
    memset(tb, 0, sizeof(*tb));
}

int icom_network_txbuf_add(struct icom_network_txbuf *tb, uint16_t sequence,
                           const uint8_t *data, size_t length, int64_t now_ms)
{
    struct icom_network_txentry *e;

    if (length > ICOM_NETWORK_SEQBUF_PKTMAX)
    {
        return -1;
    }

    e = &tb->e[tb->head];
    e->sequence   = sequence;
    e->valid = 1;
    e->ts_ms = now_ms;
    e->length   = length;
    memcpy(e->data, data, length);

    tb->head = (tb->head + 1) % ICOM_NETWORK_SEQBUF_MAX;

    return 0;
}

const uint8_t *icom_network_txbuf_get(struct icom_network_txbuf *tb,
                                      uint16_t sequence, size_t *length)
{
    size_t i;

    for (i = 0; i < ICOM_NETWORK_SEQBUF_MAX; i++)
    {
        if (tb->e[i].valid && tb->e[i].sequence == sequence)
        {
            if (length) { *length = tb->e[i].length; }

            return tb->e[i].data;
        }
    }

    return NULL;
}

void icom_network_txbuf_purge(struct icom_network_txbuf *tb, int64_t now_ms,
                              int64_t purge_ms)
{
    size_t i;

    for (i = 0; i < ICOM_NETWORK_SEQBUF_MAX; i++)
    {
        if (tb->e[i].valid && now_ms - tb->e[i].ts_ms >= purge_ms)
        {
            tb->e[i].valid = 0;
        }
    }
}

/* ------------------------------------------------------------------ */
/* RX gap / retransmit tracker                                         */
/* ------------------------------------------------------------------ */

void icom_network_rxtrack_init(struct icom_network_rxtrack *rt)
{
    memset(rt, 0, sizeof(*rt));
}

void icom_network_rxtrack_reset(struct icom_network_rxtrack *rt)
{
    rt->started  = 0;
    rt->nmissing = 0;
}

static int rxtrack_find(const struct icom_network_rxtrack *rt,
                        uint16_t sequence)
{
    size_t i;

    for (i = 0; i < rt->nmissing; i++)
    {
        if (rt->missing[i].sequence == sequence) { return (int)i; }
    }

    return -1;
}

static void rxtrack_remove_at(struct icom_network_rxtrack *rt, size_t index)
{
    if (index < rt->nmissing)
    {
        rt->missing[index] = rt->missing[rt->nmissing - 1];
        rt->nmissing--;
    }
}

void icom_network_rxtrack_received(struct icom_network_rxtrack *rt,
                                   uint16_t sequence)
{
    int index = rxtrack_find(rt, sequence);

    if (index >= 0) { rxtrack_remove_at(rt, (size_t)index); }
}

static void rxtrack_add_missing(struct icom_network_rxtrack *rt,
                                uint16_t sequence,
                                int64_t now_ms)
{
    struct icom_network_missing *m;

    int already_missing = rxtrack_find(rt, sequence) >= 0;

    if (already_missing || rt->nmissing >= ICOM_NETWORK_SEQBUF_MAX)
    {
        return;
    }

    m = &rt->missing[rt->nmissing++];
    m->sequence     = sequence;
    m->retries = 0;
    /* request immediately on the next due() poll */
    m->last_ms = now_ms - 1000000;
}

int icom_network_rxtrack_observe(struct icom_network_rxtrack *rt,
                                 uint16_t sequence,
                                 int64_t now_ms)
{
    uint16_t s;
    uint16_t gap;

    if (!rt->started)
    {
        rt->started  = 1;
        rt->last_sequence = sequence;
        return 0;
    }

    /* A retransmit or duplicate of something at/behind our high-water mark. */
    if (!sequence_after(sequence, rt->last_sequence))
    {
        icom_network_rxtrack_received(rt, sequence);
        return 0;
    }

    /* Forward jump: everything between last_sequence and sequence is missing. */
    gap = (uint16_t)(sequence - rt->last_sequence);

    if (gap > ICOM_NETWORK_MISSING_FLUSH)
    {
        /* Too large to recover packet-by-packet; resync. */
        rt->nmissing = 0;
        rt->last_sequence = sequence;
        return 1;
    }

    for (s = (uint16_t)(rt->last_sequence + 1); s != sequence;
            s = (uint16_t)(s + 1))
    {
        rxtrack_add_missing(rt, s, now_ms);
    }

    rt->last_sequence = sequence;

    return rt->nmissing > ICOM_NETWORK_MISSING_FLUSH;
}

size_t icom_network_rxtrack_due(struct icom_network_rxtrack *rt, int64_t now_ms,
                                int64_t period_ms, uint16_t *out, size_t max)
{
    size_t i = 0;
    size_t n = 0;

    while (i < rt->nmissing && n < max)
    {
        struct icom_network_missing *m = &rt->missing[i];

        if (now_ms - m->last_ms < period_ms)
        {
            i++;
            continue;
        }

        if (m->retries >= ICOM_NETWORK_RETRANSMIT_MAX)
        {
            /* Give up on this one. */
            rxtrack_remove_at(rt, i);
            continue; /* index i now holds a different entry */
        }

        out[n++]    = m->sequence;
        m->retries += 1;
        m->last_ms  = now_ms;
        i++;
    }

    return n;
}
