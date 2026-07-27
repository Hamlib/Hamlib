/*
 *  Hamlib streaming subsystem
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

/* Thread-safe ring buffer for streaming audio/I/Q data. */
/* Single producer never blocks (overwrites oldest); reader blocks with timeout. */

#ifndef HAMLIB_STREAM_RINGBUF_H
#define HAMLIB_STREAM_RINGBUF_H

#include <hamlib/rig.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

/* HAMLIB_ATOMIC is defined in hamlib/rig.h */


/* Ring buffer for streaming audio/I/Q data between producer and consumer. */
struct rig_stream_ringbuf
{
    unsigned char *buffer;
    size_t capacity;                /* Total bytes (power of 2) */
    size_t read_pos;
    size_t write_pos;
    size_t count;                   /* Bytes available to read */
    uint64_t write_total;           /* Bytes ever produced, incl. overwritten
                                     * and clamped (protected by lock) */
    pthread_mutex_t lock;
    pthread_cond_t data_available;  /* Signal when data written */
    int closing;                    /* 1 = shutting down; wakes blocked reader */
    int use_monotonic;              /* 1 if condvar uses CLOCK_MONOTONIC */
    HAMLIB_ATOMIC int overrun_count;
    HAMLIB_ATOMIC int underrun_count;
};


/* Allocate ring buffer. capacity is rounded up to a power of 2. */
int stream_ringbuf_init(struct rig_stream_ringbuf *rb, size_t capacity);

/* Free ring buffer resources. */
void stream_ringbuf_destroy(struct rig_stream_ringbuf *rb);

/* Write data to ring buffer.
 * Overwrites oldest data if buffer is full (producer never blocks).
 * Increments overrun_count when overwriting.
 * Signals data_available.
 * Returns bytes stored: len, or capacity when len exceeds capacity (only the
 * last capacity bytes are kept). write_total still counts every byte produced,
 * so the consumer detects the dropped remainder. */
size_t stream_ringbuf_write(struct rig_stream_ringbuf *rb,
                            const void *data, size_t len);

/* Read data from ring buffer.
 * Blocks up to timeout_ms waiting for data_available.
 * Returns bytes read (may be less than requested).
 * Increments underrun_count if timeout expires with no data. */
size_t stream_ringbuf_read(struct rig_stream_ringbuf *rb,
                           void *data, size_t len, int timeout_ms);

/* Query bytes available for reading without blocking. */
size_t stream_ringbuf_available(struct rig_stream_ringbuf *rb);

/* Reset ring buffer to empty state. */
void stream_ringbuf_reset(struct rig_stream_ringbuf *rb);

/* Lower-level primitives for consumers that need to snapshot additional
 * state atomically with the read.  The caller must hold rb->lock around
 * both calls (see the enriched-read path in stream.c). */

/* Wait for readable data.  Returns 0 when data is available, -1 on timeout
 * (bumps underrun_count). */
int stream_ringbuf_wait_data_locked(struct rig_stream_ringbuf *rb,
                                    int timeout_ms);

/* Copy up to len readable bytes out of the buffer.  Returns bytes copied. */
size_t stream_ringbuf_consume_locked(struct rig_stream_ringbuf *rb,
                                     unsigned char *dst, size_t len);

#endif /* HAMLIB_STREAM_RINGBUF_H */
