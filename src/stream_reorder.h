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
/* Pure logic: no sockets, threads or locks; the caller serialises calls. */

#ifndef HAMLIB_STREAM_REORDER_H
#define HAMLIB_STREAM_REORDER_H

#include <stddef.h>
#include <stdint.h>

/*
 * A producer that receives sequence-numbered datagrams pushes each one here
 * and pops what is ready. Packets come out in sequence order. A packet that
 * has not arrived is waited for at most window_ms, counted from the arrival
 * of the first packet after it; then it is given up and reported as a gap.
 * Anything arriving behind the release point -- a retransmit or reordered
 * packet that came too late, or a duplicate of one already released -- is
 * dropped, so the output never goes backwards.
 *
 * With window_ms == 0 nothing is held: every packet is released as it
 * arrives, a gap is reported as soon as the packet after it arrives, and
 * late packets are dropped. That is plain in-order delivery.
 *
 * Usage: push one datagram, then pop until STREAM_REORDER_NONE. Also pop
 * when stream_reorder_next_deadline() says a held packet is due, even with
 * nothing new to push.
 *
 * Sequence numbers are seq_bits wide (1..32) and wrap. Packet sizes in
 * samples are the caller's business: a gap reports packet counts only.
 */

struct stream_reorder;

/* stream_reorder_push() results */
#define STREAM_REORDER_ACCEPTED   0  /* held (or ready) for release */
#define STREAM_REORDER_LATE       1  /* behind the release point; dropped */
#define STREAM_REORDER_DUPLICATE  2  /* already held; dropped */
#define STREAM_REORDER_RESYNC     3  /* jump beyond max_gap (or a run of late
                                        packets): held packets discarded, an
                                        unsized gap is released next, and
                                        ordering restarts at this packet */
#define STREAM_REORDER_DROPPED    4  /* too large, or pushed while a forced
                                        release was still pending; dropped */

/* stream_reorder_pop() result kinds */
#define STREAM_REORDER_NONE    0     /* nothing ready */
#define STREAM_REORDER_PACKET  1     /* item.data/length/seq is the next packet */
#define STREAM_REORDER_GAP     2     /* item.lost packets starting at item.seq
                                        were given up; item.unsized = 1 when the
                                        count is unknown (lost is then 0) */

struct stream_reorder_item
{
    int kind;
    uint32_t seq;
    const uint8_t *data;   /* valid until the next push or pop */
    size_t length;
    uint32_t lost;
    int unsized;
};

struct stream_reorder_stats
{
    uint64_t released;     /* packets released */
    uint64_t late;         /* dropped: behind the release point */
    uint64_t duplicates;   /* dropped: already held */
    uint64_t gaps;         /* gap events released (sized and unsized) */
    uint64_t lost;         /* packets in sized gaps */
    uint64_t resyncs;      /* unsized restarts */
    uint64_t dropped;      /* oversize, or pushed while a release was forced */
};

/* Create a window. window_ms: hold time (0 = in-order only). seq_bits: width
 * of the sequence counter. max_packets: how many sequence positions can be
 * held ahead of the release point; a packet further ahead forces the oldest
 * out early (never silently). max_gap: a forward jump larger than this is
 * treated as a restart (STREAM_REORDER_RESYNC); also the length of a run of
 * consecutive late packets that means the sender restarted its counter.
 * max_payload: largest datagram accepted. Returns NULL on bad arguments or
 * allocation failure. */
struct stream_reorder *stream_reorder_new(unsigned int window_ms,
        unsigned int seq_bits,
        size_t max_packets,
        uint32_t max_gap,
        size_t max_payload);

void stream_reorder_free(struct stream_reorder *r);

/* Forget all sequence state and held packets (e.g. after a reconnect).
 * Statistics are kept. */
void stream_reorder_reset(struct stream_reorder *r);

int stream_reorder_push(struct stream_reorder *r, uint32_t seq,
                        const uint8_t *data, size_t length, int64_t now_ms);

/* Returns the item kind and fills *item. */
int stream_reorder_pop(struct stream_reorder *r, int64_t now_ms,
                       struct stream_reorder_item *item);

/* Milliseconds until a pop could return something: 0 when something is
 * ready now, -1 when nothing is held at all. */
int64_t stream_reorder_next_deadline(const struct stream_reorder *r,
                                     int64_t now_ms);

/* Sequences currently missing inside the window that have not been listed in
 * the last period_ms (each listing restarts that sequence's period), for
 * protocols that can request retransmission. Listing stops at max. Returns
 * the count written to out. Always 0 with window_ms == 0, since nothing that
 * arrives late could be used. */
size_t stream_reorder_missing(struct stream_reorder *r, int64_t now_ms,
                              int64_t period_ms, uint32_t *out, size_t max);

/* Milliseconds until stream_reorder_missing() with the same period_ms would
 * list something: 0 when it would now, -1 when no missing sequence can still
 * be asked for before its deadline. A receiver that sleeps until the next
 * datagram should also wake for this, or a retry falls due while it sleeps
 * and the window closes first. */
int64_t stream_reorder_next_request(const struct stream_reorder *r,
                                    int64_t now_ms, int64_t period_ms);

void stream_reorder_get_stats(const struct stream_reorder *r,
                              struct stream_reorder_stats *stats);

unsigned int stream_reorder_window_ms(const struct stream_reorder *r);

#endif /* HAMLIB_STREAM_REORDER_H */
