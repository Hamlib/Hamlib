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

const uint8_t *icom_network_txbuf_next_in_range(struct icom_network_txbuf *tb,
        uint16_t first, uint16_t last, size_t *cursor, size_t *length)
{
    uint16_t span = (uint16_t)(last - first);

    /* from head on, the slots come in the order they were written */
    while (*cursor < ICOM_NETWORK_SEQBUF_MAX)
    {
        struct icom_network_txentry *e =
            &tb->e[(tb->head + *cursor) % ICOM_NETWORK_SEQBUF_MAX];
        /* past first, counted modulo 2^16, so that a range may wrap */
        uint16_t offset = (uint16_t)(e->sequence - first);

        (*cursor)++;

        if (e->valid && offset <= span)
        {
            if (length) { *length = e->length; }

            return e->data;
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

void icom_network_rxtrack_expect(struct icom_network_rxtrack *rt,
                                 uint16_t first)
{
    rt->nmissing = 0;
    rt->behind_run = 0;
    memset(rt->received_count, 0, sizeof(rt->received_count));
    rt->started = 1;
    rt->last_sequence = (uint16_t)(first - 1);
}

static int rxtrack_was_received(const struct icom_network_rxtrack *rt,
                                uint16_t sequence, uint32_t digest)
{
    unsigned slot = sequence & 0xff;

    return rt->received_count[slot] > 0
           && rt->received[slot] == sequence
           && (rt->received_digest[slot][0] == digest
               || (rt->received_count[slot] > 1
                   && rt->received_digest[slot][1] == digest));
}

static void rxtrack_mark_received(struct icom_network_rxtrack *rt,
                                  uint16_t sequence, uint32_t digest)
{
    unsigned slot = sequence & 0xff;

    if (rxtrack_was_received(rt, sequence, digest)) { return; }

    if (rt->received_count[slot] > 0 && rt->received[slot] == sequence)
    {
        rt->received_digest[slot][1] = rt->received_digest[slot][0];
        rt->received_count[slot] = 2;
    }
    else
    {
        rt->received[slot] = sequence;
        rt->received_count[slot] = 1;
    }

    rt->received_digest[slot][0] = digest;
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

/* The newest goes forward to sequence: everything in between is missing,
 * unless too much to recover packet by packet. */
static enum icom_network_rx_result rxtrack_advance(
    struct icom_network_rxtrack *rt, uint16_t sequence, int64_t now_ms)
{
    uint16_t gap = (uint16_t)(sequence - rt->last_sequence);
    uint16_t s;

    if (gap > ICOM_NETWORK_MISSING_FLUSH)
    {
        rt->nmissing = 0;
        rt->last_sequence = sequence;
        return ICOM_NETWORK_RX_RESYNC;
    }

    for (s = (uint16_t)(rt->last_sequence + 1); s != sequence;
            s = (uint16_t)(s + 1))
    {
        rxtrack_add_missing(rt, s, now_ms);
    }

    rt->last_sequence = sequence;

    if (rt->nmissing > ICOM_NETWORK_MISSING_FLUSH)
    {
        rt->nmissing = 0;
        return ICOM_NETWORK_RX_RESYNC;
    }

    return ICOM_NETWORK_RX_NEW;
}

/* The sender numbers from scratch. The packets from behind that showed it are
 * the new stream's first, except, when this one is far behind, those far from
 * it: the old stream's, given up on and arriving late. The gaps so far are
 * dropped and tracking goes on as if the new stream's packets had come in
 * order. What was received stays known. */
static void rxtrack_restart(struct icom_network_rxtrack *rt, uint16_t sequence,
                            int far, int64_t now_ms)
{
    uint16_t p[ICOM_NETWORK_RESTART_RUN];
    unsigned n = 0;
    unsigned i, j;

    for (i = 0; i < rt->behind_run; i++)
    {
        int16_t distance = (int16_t)(rt->behind[i] - sequence);

        if (!far || (distance <= ICOM_NETWORK_MISSING_FLUSH
                     && distance >= -ICOM_NETWORK_MISSING_FLUSH))
        {
            p[n++] = rt->behind[i];
        }
    }

    p[n++] = sequence;

    /* oldest first; they are all near this one */
    for (i = 1; i < n; i++)
    {
        for (j = i; j > 0 && (int16_t)(p[j] - sequence)
                < (int16_t)(p[j - 1] - sequence); j--)
        {
            uint16_t t = p[j];

            p[j] = p[j - 1];
            p[j - 1] = t;
        }
    }

    rt->nmissing = 0;
    rt->behind_run = 0;
    rt->last_sequence = p[0];

    for (i = 1; i < n; i++)
    {
        if (sequence_after(p[i], rt->last_sequence))
        {
            rxtrack_advance(rt, p[i], now_ms);
        }
    }
}

enum icom_network_rx_result icom_network_rxtrack_observe(
    struct icom_network_rxtrack *rt, uint16_t sequence, uint32_t digest,
    int64_t now_ms)
{
    int index;
    int far;

    if (!rt->started)
    {
        rt->started  = 1;
        rt->last_sequence = sequence;
        rt->behind_run = 0;
        rxtrack_mark_received(rt, sequence, digest);
        return ICOM_NETWORK_RX_NEW;
    }

    /* A copy, wherever its number falls. */
    if (rxtrack_was_received(rt, sequence, digest))
    {
        return ICOM_NETWORK_RX_DUPLICATE;
    }

    rxtrack_mark_received(rt, sequence, digest);

    if (sequence_after(sequence, rt->last_sequence))
    {
        rt->behind_run = 0;
        return rxtrack_advance(rt, sequence, now_ms);
    }

    /* At or behind the newest: a retransmit we asked for, or never received
     * -- a packet given up on that turned up after all or, far behind or
     * several in a row, a sender that started counting again. */
    index = rxtrack_find(rt, sequence);

    if (index >= 0)
    {
        rxtrack_remove_at(rt, (size_t)index);
        rt->behind_run = 0;
        return ICOM_NETWORK_RX_RECOVERED;
    }

    far = (uint16_t)(rt->last_sequence - sequence)
          > ICOM_NETWORK_RESTART_DISTANCE;

    if (far || rt->behind_run + 1 >= ICOM_NETWORK_RESTART_RUN)
    {
        rxtrack_restart(rt, sequence, far, now_ms);
        return ICOM_NETWORK_RX_RESYNC;
    }

    rt->behind[rt->behind_run++] = sequence;
    return ICOM_NETWORK_RX_NEW;
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
