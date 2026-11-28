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
/* TX replay buffer (for answering retransmit requests) + RX gap/missing */
/* tracking (for issuing them). Pure logic, no sockets — unit-testable. */

#ifndef _ICOM_NETWORK_SEQBUF_H
#define _ICOM_NETWORK_SEQBUF_H 1

#include <stddef.h>
#include <stdint.h>

#define ICOM_NETWORK_SEQBUF_MAX     256  /* tracked outstanding packets/socket */
#define ICOM_NETWORK_SEQBUF_PKTMAX  1500 /* max stored packet bytes */
#define ICOM_NETWORK_RETRANSMIT_MAX 4    /* re-request attempts per sequence */
#define ICOM_NETWORK_MISSING_FLUSH  50   /* flush/resync threshold */

/* ---- TX replay buffer ---- */

struct icom_network_txentry
{
    uint16_t sequence;
    int      valid;
    int64_t  ts_ms;
    size_t   length;
    uint8_t  data[ICOM_NETWORK_SEQBUF_PKTMAX];
};

struct icom_network_txbuf
{
    struct icom_network_txentry e[ICOM_NETWORK_SEQBUF_MAX];
    size_t head;   /* next write slot (circular) */
};

void icom_network_txbuf_init(struct icom_network_txbuf *tb);

/* Store a sent packet for possible replay; overwrites the oldest slot when
 * full. Packets larger than ICOM_NETWORK_SEQBUF_PKTMAX are not stored. */
int icom_network_txbuf_add(struct icom_network_txbuf *tb, uint16_t sequence,
                           const uint8_t *data, size_t length, int64_t now_ms);

/* Next stored packet, oldest first, with sequence in [first, last] modulo 2^16
 * (*cursor starts at 0); returns its bytes (and sets *length) or NULL. */
const uint8_t *icom_network_txbuf_next_in_range(struct icom_network_txbuf *tb,
        uint16_t first, uint16_t last, size_t *cursor, size_t *length);

/* Invalidate entries older than purge_ms. */
void icom_network_txbuf_purge(struct icom_network_txbuf *tb, int64_t now_ms,
                              int64_t purge_ms);

/* ---- RX gap / retransmit tracker ---- */

/* A sender that starts numbering again is recognised by a packet this far
 * behind the newest, or by this many packets in a row from behind that never
 * arrived before. The radio does it when a stream is reopened: a new session
 * first gets the previous client's undelivered packets, with their old, high
 * numbers, and then the new stream counting from zero. */
#define ICOM_NETWORK_RESTART_DISTANCE ICOM_NETWORK_SEQBUF_MAX
#define ICOM_NETWORK_RESTART_RUN      4

struct icom_network_missing
{
    uint16_t sequence;
    int      retries;
    int64_t  last_ms;
};

struct icom_network_rxtrack
{
    int      started;
    uint16_t last_sequence;
    struct icom_network_missing missing[ICOM_NETWORK_SEQBUF_MAX];
    size_t   nmissing;
    /* The most recent sequence received for each low byte and digests of
     * the last two different packets with that number, so a packet is called
     * a duplicate only when that very number did arrive before with the same
     * content, also when another packet with that number arrived since. */
    uint16_t received[256];
    uint32_t received_digest[256][2];
    uint8_t  received_count[256];   /* digests held: 0, 1 or 2 */
    /* Consecutive packets from behind the newest that never arrived before,
     * and their numbers: a run of them is the sender numbering from scratch,
     * and they are the first packets of its new stream. */
    unsigned behind_run;
    uint16_t behind[ICOM_NETWORK_RESTART_RUN - 1];
};

void icom_network_rxtrack_init(struct icom_network_rxtrack *rt);

/* What a received sequence number turned out to be. */
enum icom_network_rx_result
{
    ICOM_NETWORK_RX_NEW = 0,     /* the next sequence, or one past a gap */
    ICOM_NETWORK_RX_RECOVERED,   /* one that was missing, arriving late */
    ICOM_NETWORK_RX_DUPLICATE,   /* already received, or given up on */
    ICOM_NETWORK_RX_RESYNC,      /* new, but the gaps could no longer be
                                    recovered and were dropped */
};

/* Observe a received data-packet sequence number; digest is a checksum of
 * the packet. Newly-detected gaps are appended to the missing set; a packet
 * that fills one (ICOM_NETWORK_RX_RECOVERED) clears its entry, a copy does
 * not, so the gap is asked for until given up on. ICOM_NETWORK_RX_DUPLICATE
 * means this exact sequence was received before with the same digest,
 * wherever it falls, and the packet should be discarded. A packet from
 * behind that never arrived (one given up on, or another stream's with a
 * number already seen: a restarted sender's, or the one packet of the
 * previous client's stream that a radio still holding it may send around
 * the opening of the new one) is not a duplicate: it is reported as new.
 * ICOM_NETWORK_RX_RESYNC (too many missing, a jump larger than the flush
 * threshold, or a restart) reports that the gaps were dropped and tracking
 * goes on from here; that packet itself is new. After a restart it goes on
 * as if the packets that showed it had come in order. What was received
 * stays known, a restart included: with no stream in the packet, a new one
 * byte for byte the same as one received with its number before is taken
 * for a copy. */
enum icom_network_rx_result icom_network_rxtrack_observe(
    struct icom_network_rxtrack *rt, uint16_t sequence, uint32_t digest,
    int64_t now_ms);

/* Mark a sequence as received (removes it from the missing set). */
void icom_network_rxtrack_received(struct icom_network_rxtrack *rt,
                                   uint16_t sequence);

/* Collect up to max sequences due for (re)request (older than period_ms since
 * their last request). Increments each one's retry counter and timestamp, and
 * drops entries that have exceeded ICOM_NETWORK_RETRANSMIT_MAX. Returns the
 * number written to out. */
size_t icom_network_rxtrack_due(struct icom_network_rxtrack *rt, int64_t now_ms,
                                int64_t period_ms, uint16_t *out, size_t max);

/* Start tracking a stream whose first packet will carry the sequence first,
 * so that losing that very packet is noticed from the gap the next one leaves.
 * Without it tracking starts at whatever arrives first, and a lost first
 * packet is never missed. Nothing of the new stream is received yet, so the
 * record of received packets starts empty. */
void icom_network_rxtrack_expect(struct icom_network_rxtrack *rt,
                                 uint16_t first);

#endif /* _ICOM_NETWORK_SEQBUF_H */
