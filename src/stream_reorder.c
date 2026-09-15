/*
 *  Hamlib streaming subsystem - packet reorder window
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

/* Sequence-ordered release of datagrams, with a bounded hold window. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "stream_reorder.h"

#include <stdlib.h>
#include <string.h>

/* One sequence position ahead of the release point. */
struct reorder_slot
{
    int present;
    int requested;          /* listed by stream_reorder_missing() */
    int64_t arrival_ms;
    int64_t requested_ms;
    size_t length;
};

struct stream_reorder
{
    unsigned int window_ms;
    uint32_t mask;          /* sequence modulus - 1 */
    uint32_t half;          /* offsets at or above this are "behind" */
    size_t cap;             /* positions held ahead of the release point */
    uint32_t max_gap;
    size_t max_payload;

    struct reorder_slot *slots;   /* ring: slots[base] is position `next` */
    uint8_t *data;                /* cap * max_payload */
    uint8_t *out;                 /* copy of the packet last popped */

    int started;
    uint32_t next;          /* sequence of the release point */
    size_t base;
    size_t span;            /* positions known ahead of `next` (the last one
                               is always a held packet) */
    size_t held;            /* present slots */
    uint32_t late_run;      /* consecutive late arrivals */

    int unsized_pending;    /* release an unsized gap first */
    uint32_t sized_pending; /* release a sized gap of this many first */
    uint32_t sized_pending_seq;

    /* A packet too far ahead to hold: kept aside while older positions are
     * forced out. */
    int force_pending;
    uint32_t force_seq;
    size_t force_length;
    int64_t force_arrival;
    uint8_t *force_data;

    struct stream_reorder_stats stats;
};


static struct reorder_slot *slot_at(const struct stream_reorder *r,
                                    size_t offset)
{
    return &r->slots[(r->base + offset) % r->cap];
}

static uint8_t *data_at(const struct stream_reorder *r, size_t offset)
{
    return r->data + ((r->base + offset) % r->cap) * r->max_payload;
}

static uint32_t offset_of(const struct stream_reorder *r, uint32_t seq)
{
    return (seq - r->next) & r->mask;
}

static void store(struct stream_reorder *r, size_t offset, const uint8_t *data,
                  size_t length, int64_t now_ms)
{
    struct reorder_slot *s = slot_at(r, offset);

    s->present = 1;
    s->requested = 0;
    s->arrival_ms = now_ms;
    s->length = length;

    if (length > 0)
    {
        memcpy(data_at(r, offset), data, length);
    }

    r->held++;

    if (offset + 1 > r->span)
    {
        r->span = offset + 1;
    }
}

static void advance(struct stream_reorder *r, size_t count)
{
    size_t i;

    for (i = 0; i < count && i < r->cap; i++)
    {
        struct reorder_slot *s = slot_at(r, i);

        if (s->present)
        {
            r->held--;
        }

        s->present = 0;
        s->requested = 0;
    }

    r->base = (r->base + count) % r->cap;
    r->next = (r->next + (uint32_t)count) & r->mask;
    r->span = r->span > count ? r->span - count : 0;
}

/* Drop everything and restart ordering at seq. */
static void resync(struct stream_reorder *r, uint32_t seq)
{
    size_t i;

    for (i = 0; i < r->cap; i++)
    {
        r->slots[i].present = 0;
        r->slots[i].requested = 0;
    }

    r->stats.dropped += r->held;
    r->stats.resyncs++;
    r->held = 0;
    r->span = 0;
    r->base = 0;
    r->next = seq & r->mask;
    r->late_run = 0;
    r->force_pending = 0;
    r->sized_pending = 0;
    r->unsized_pending = 1;
}

/* First held position at or after offset `from`, or r->span if none. */
static size_t first_present(const struct stream_reorder *r, size_t from)
{
    size_t k;

    for (k = from; k < r->span; k++)
    {
        if (slot_at(r, k)->present)
        {
            return k;
        }
    }

    return r->span;
}


struct stream_reorder *stream_reorder_new(unsigned int window_ms,
        unsigned int seq_bits,
        size_t max_packets,
        uint32_t max_gap,
        size_t max_payload)
{
    struct stream_reorder *r;

    if (seq_bits < 1 || seq_bits > 32 || max_packets == 0 || max_payload == 0)
    {
        return NULL;
    }

    r = calloc(1, sizeof(*r));

    if (r == NULL)
    {
        return NULL;
    }

    r->window_ms = window_ms;
    r->mask = seq_bits == 32 ? 0xffffffffu : ((1u << seq_bits) - 1u);
    r->half = seq_bits == 32 ? 0x80000000u : (1u << (seq_bits - 1));
    /* A position must never alias a "behind" offset. */
    r->cap = max_packets < r->half ? max_packets : r->half;
    r->max_gap = max_gap;
    r->max_payload = max_payload;
    r->slots = calloc(r->cap, sizeof(*r->slots));
    r->data = malloc(r->cap * max_payload);
    r->out = malloc(max_payload);
    r->force_data = malloc(max_payload);

    if (!r->slots || !r->data || !r->out || !r->force_data)
    {
        stream_reorder_free(r);
        return NULL;
    }

    return r;
}


void stream_reorder_free(struct stream_reorder *r)
{
    if (r == NULL)
    {
        return;
    }

    free(r->slots);
    free(r->data);
    free(r->out);
    free(r->force_data);
    free(r);
}


void stream_reorder_reset(struct stream_reorder *r)
{
    size_t i;

    for (i = 0; i < r->cap; i++)
    {
        r->slots[i].present = 0;
        r->slots[i].requested = 0;
    }

    r->started = 0;
    r->held = 0;
    r->span = 0;
    r->base = 0;
    r->late_run = 0;
    r->force_pending = 0;
    r->unsized_pending = 0;
    r->sized_pending = 0;
}


int stream_reorder_push(struct stream_reorder *r, uint32_t seq,
                        const uint8_t *data, size_t length, int64_t now_ms)
{
    uint32_t offset;
    size_t missing_before;

    if (length > r->max_payload || r->force_pending)
    {
        r->stats.dropped++;
        return STREAM_REORDER_DROPPED;
    }

    seq &= r->mask;

    if (!r->started)
    {
        r->started = 1;
        r->next = seq;
        store(r, 0, data, length, now_ms);
        return STREAM_REORDER_ACCEPTED;
    }

    offset = offset_of(r, seq);

    if (offset >= r->half)
    {
        /* Behind the release point. A long run of these is not lateness but
         * a sender that restarted its counter. */
        r->late_run++;

        if (r->max_gap > 0 && r->late_run > r->max_gap)
        {
            resync(r, seq);
            store(r, 0, data, length, now_ms);
            return STREAM_REORDER_RESYNC;
        }

        r->stats.late++;
        return STREAM_REORDER_LATE;
    }

    r->late_run = 0;

    if (offset < r->span)
    {
        if (slot_at(r, offset)->present)
        {
            r->stats.duplicates++;
            return STREAM_REORDER_DUPLICATE;
        }

        store(r, offset, data, length, now_ms);
        return STREAM_REORDER_ACCEPTED;
    }

    missing_before = offset - r->span;

    if (r->max_gap > 0 && missing_before > r->max_gap)
    {
        resync(r, seq);
        store(r, 0, data, length, now_ms);
        return STREAM_REORDER_RESYNC;
    }

    if (offset < r->cap)
    {
        store(r, offset, data, length, now_ms);
        return STREAM_REORDER_ACCEPTED;
    }

    if (r->span == 0)
    {
        /* Nothing held: the whole jump is a sized gap, released first. */
        r->sized_pending = offset;
        r->sized_pending_seq = r->next;
        r->base = 0;
        r->next = seq;
        store(r, 0, data, length, now_ms);
        return STREAM_REORDER_ACCEPTED;
    }

    /* Too far ahead to hold: keep it aside and force older positions out. */
    r->force_pending = 1;
    r->force_seq = seq;
    r->force_length = length;
    r->force_arrival = now_ms;

    if (length > 0)
    {
        memcpy(r->force_data, data, length);
    }

    return STREAM_REORDER_ACCEPTED;
}


/* Place the set-aside packet once it fits, or turn the remaining distance into
 * a sized gap when nothing older is left to release. Returns 1 when that gap
 * was queued. */
static int settle_forced(struct stream_reorder *r)
{
    uint32_t offset;

    if (!r->force_pending)
    {
        return 0;
    }

    offset = offset_of(r, r->force_seq);

    if (offset < r->cap)
    {
        r->force_pending = 0;
        store(r, offset, r->force_data, r->force_length, r->force_arrival);
        return 0;
    }

    if (r->span == 0)
    {
        r->force_pending = 0;
        r->sized_pending = offset;
        r->sized_pending_seq = r->next;
        r->base = 0;
        r->next = r->force_seq;
        store(r, 0, r->force_data, r->force_length, r->force_arrival);
        return 1;
    }

    return 0;
}


int stream_reorder_pop(struct stream_reorder *r, int64_t now_ms,
                       struct stream_reorder_item *item)
{
    struct reorder_slot *head;
    size_t k;

    memset(item, 0, sizeof(*item));
    item->kind = STREAM_REORDER_NONE;

    if (r->unsized_pending)
    {
        r->unsized_pending = 0;
        r->stats.gaps++;
        item->kind = STREAM_REORDER_GAP;
        item->seq = r->next;
        item->unsized = 1;
        return item->kind;
    }

    settle_forced(r);

    if (r->sized_pending > 0)
    {
        item->kind = STREAM_REORDER_GAP;
        item->seq = r->sized_pending_seq;
        item->lost = r->sized_pending;
        r->stats.gaps++;
        r->stats.lost += r->sized_pending;
        r->sized_pending = 0;
        return item->kind;
    }

    if (r->span == 0)
    {
        return item->kind;
    }

    head = slot_at(r, 0);

    if (head->present)
    {
        item->kind = STREAM_REORDER_PACKET;
        item->seq = r->next;
        item->length = head->length;

        if (head->length > 0)
        {
            memcpy(r->out, data_at(r, 0), head->length);
        }

        item->data = r->out;
        r->stats.released++;
        advance(r, 1);
        settle_forced(r);
        return item->kind;
    }

    k = first_present(r, 1);

    if (!r->force_pending
            && now_ms - slot_at(r, k)->arrival_ms < (int64_t)r->window_ms)
    {
        return item->kind;
    }

    item->kind = STREAM_REORDER_GAP;
    item->seq = r->next;
    item->lost = (uint32_t)k;
    r->stats.gaps++;
    r->stats.lost += k;
    advance(r, k);
    settle_forced(r);
    return item->kind;
}


int64_t stream_reorder_next_deadline(const struct stream_reorder *r,
                                     int64_t now_ms)
{
    size_t k;
    int64_t due;

    if (r->unsized_pending || r->sized_pending > 0 || r->force_pending)
    {
        return 0;
    }

    if (r->span == 0)
    {
        return -1;
    }

    if (slot_at(r, 0)->present)
    {
        return 0;
    }

    k = first_present(r, 1);
    due = slot_at(r, k)->arrival_ms + (int64_t)r->window_ms - now_ms;

    return due > 0 ? due : 0;
}


size_t stream_reorder_missing(struct stream_reorder *r, int64_t now_ms,
                              int64_t period_ms, uint32_t *out, size_t max)
{
    size_t off, n = 0;

    if (r->window_ms == 0 || r->span == 0)
    {
        return 0;
    }

    for (off = 0; off < r->span && n < max; off++)
    {
        struct reorder_slot *s = slot_at(r, off);
        struct reorder_slot *after;

        if (s->present)
        {
            continue;
        }

        /* Past its deadline it is about to be given up; asking is useless. */
        after = slot_at(r, first_present(r, off + 1));

        if (now_ms - after->arrival_ms >= (int64_t)r->window_ms)
        {
            continue;
        }

        if (s->requested && now_ms - s->requested_ms < period_ms)
        {
            continue;
        }

        s->requested = 1;
        s->requested_ms = now_ms;
        out[n++] = (r->next + (uint32_t)off) & r->mask;
    }

    return n;
}


int64_t stream_reorder_next_request(const struct stream_reorder *r,
                                    int64_t now_ms, int64_t period_ms)
{
    size_t off;
    int64_t best = -1;

    if (r->window_ms == 0)
    {
        return -1;
    }

    for (off = 0; off < r->span; off++)
    {
        const struct reorder_slot *s = slot_at(r, off);
        int64_t deadline, due;

        if (s->present)
        {
            continue;
        }

        deadline = slot_at(r, first_present(r, off + 1))->arrival_ms
                   + (int64_t)r->window_ms;

        if (now_ms >= deadline)
        {
            continue;
        }

        due = s->requested ? s->requested_ms + period_ms : now_ms;

        /* A request due only at or after the deadline is never sent. */
        if (due >= deadline)
        {
            continue;
        }

        due = due > now_ms ? due - now_ms : 0;

        if (best < 0 || due < best)
        {
            best = due;
        }
    }

    return best;
}


void stream_reorder_get_stats(const struct stream_reorder *r,
                              struct stream_reorder_stats *stats)
{
    *stats = r->stats;
}


unsigned int stream_reorder_window_ms(const struct stream_reorder *r)
{
    return r->window_ms;
}
