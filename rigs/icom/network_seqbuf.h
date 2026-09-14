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

/* Find a stored packet by sequence; returns its bytes (and sets *length) or NULL. */
const uint8_t *icom_network_txbuf_get(struct icom_network_txbuf *tb,
                                      uint16_t sequence, size_t *length);

/* Invalidate entries older than purge_ms. */
void icom_network_txbuf_purge(struct icom_network_txbuf *tb, int64_t now_ms,
                              int64_t purge_ms);

/* ---- RX gap / retransmit tracker ---- */

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
};

void icom_network_rxtrack_init(struct icom_network_rxtrack *rt);

/* Observe a received data-packet sequence number. Newly-detected gaps are
 * appended to the missing set; an arriving retransmit clears its entry.
 * Returns 1 when the caller should flush/resync (too many missing, or a jump
 * larger than the flush threshold). */
int icom_network_rxtrack_observe(struct icom_network_rxtrack *rt,
                                 uint16_t sequence,
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

void icom_network_rxtrack_reset(struct icom_network_rxtrack *rt);

#endif /* _ICOM_NETWORK_SEQBUF_H */
