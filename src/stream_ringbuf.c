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
/* Overwrite-oldest producer, timeout-blocking reader using a condition var. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "stream_ringbuf.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>


/* Round up to the next power of 2.  Returns v unchanged if already a power of 2. */
static size_t next_power_of_two(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#if SIZE_MAX > 0xFFFFFFFFUL
    v |= v >> 32;
#endif
    v++;
    return v;
}


int stream_ringbuf_init(struct rig_stream_ringbuf *rb, size_t capacity)
{
    memset(rb, 0, sizeof(*rb));

    if (capacity == 0)
    {
        return -1;
    }

    /* Reject sizes whose power-of-two rounding would wrap to 0, which would
     * otherwise yield a live zero-capacity ring that silently drops data. */
    if (capacity > (SIZE_MAX >> 1) + 1)
    {
        return -1;
    }

    capacity = next_power_of_two(capacity);

    rb->buffer = calloc(1, capacity);

    if (!rb->buffer)
    {
        return -1;
    }

    rb->capacity = capacity;
    pthread_mutex_init(&rb->lock, NULL);

#if defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0
    {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);

        if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0)
        {
            pthread_cond_init(&rb->data_available, &attr);
            rb->use_monotonic = 1;
        }
        else
        {
            pthread_cond_init(&rb->data_available, NULL);
        }

        pthread_condattr_destroy(&attr);
    }
#else
    pthread_cond_init(&rb->data_available, NULL);
#endif

    return 0;
}


void stream_ringbuf_destroy(struct rig_stream_ringbuf *rb)
{
    pthread_mutex_destroy(&rb->lock);
    pthread_cond_destroy(&rb->data_available);
    free(rb->buffer);
    rb->buffer = NULL;
    rb->capacity = 0;
}


size_t stream_ringbuf_write(struct rig_stream_ringbuf *rb,
                            const void *data, size_t len)
{
    const unsigned char *src = data;

    pthread_mutex_lock(&rb->lock);

    /* Producer position counts every byte produced, including bytes that
     * are clamped or overwritten below — the consumer detects the loss as
     * a jump in the first-readable index. */
    rb->write_total += len;

    /* If writing more than capacity, only keep the last capacity bytes. */
    if (len > rb->capacity)
    {
        src += len - rb->capacity;
        len = rb->capacity;
    }

    /* If writing would overflow, advance read_pos to discard oldest data. */
    if (len > rb->capacity - rb->count)
    {
        size_t overflow = len - (rb->capacity - rb->count);
        rb->read_pos = (rb->read_pos + overflow) & (rb->capacity - 1);
        rb->count -= overflow;
        rb->overrun_count++;
    }

    /* Write data, handling wraparound. */
    size_t first = rb->capacity - rb->write_pos;

    if (first >= len)
    {
        memcpy(rb->buffer + rb->write_pos, src, len);
    }
    else
    {
        memcpy(rb->buffer + rb->write_pos, src, first);
        memcpy(rb->buffer, src + first, len - first);
    }

    rb->write_pos = (rb->write_pos + len) & (rb->capacity - 1);
    rb->count += len;

    pthread_cond_signal(&rb->data_available);
    pthread_mutex_unlock(&rb->lock);

    return len;
}


size_t stream_ringbuf_write_record(struct rig_stream_ringbuf *rb,
                                   const void *hdr, size_t hdr_len,
                                   const void *payload, size_t len)
{
    size_t total = hdr_len + len;

    pthread_mutex_lock(&rb->lock);

    /* All or nothing: a partial record would strand the reader mid-frame,
     * and overwriting the oldest bytes would destroy record alignment.
     * No overrun counting here — a retrying TX writer polls this path,
     * so the caller decides whether a failure is a real drop. */
    if (total > rb->capacity - rb->count)
    {
        pthread_mutex_unlock(&rb->lock);
        return 0;
    }

    rb->write_total += total;

    const unsigned char *parts[2] = { hdr, payload };
    const size_t part_len[2] = { hdr_len, len };

    for (int i = 0; i < 2; i++)
    {
        const unsigned char *src = parts[i];
        size_t n = part_len[i];
        size_t first = rb->capacity - rb->write_pos;

        if (first >= n)
        {
            memcpy(rb->buffer + rb->write_pos, src, n);
        }
        else
        {
            memcpy(rb->buffer + rb->write_pos, src, first);
            memcpy(rb->buffer, src + first, n - first);
        }

        rb->write_pos = (rb->write_pos + n) & (rb->capacity - 1);
        rb->count += n;
    }

    pthread_cond_signal(&rb->data_available);
    pthread_mutex_unlock(&rb->lock);

    return total;
}


size_t stream_ringbuf_peek_locked(struct rig_stream_ringbuf *rb,
                                  unsigned char *dst, size_t len)
{
    if (len > rb->count)
    {
        len = rb->count;
    }

    size_t first = rb->capacity - rb->read_pos;

    if (first >= len)
    {
        memcpy(dst, rb->buffer + rb->read_pos, len);
    }
    else
    {
        memcpy(dst, rb->buffer + rb->read_pos, first);
        memcpy(dst + first, rb->buffer, len - first);
    }

    return len;
}


/* Wait for readable data. Caller holds rb->lock.
 * timeout_ms < 0 blocks until data or shutdown; 0 polls; > 0 bounds the wait.
 * Returns 0 when data is available, -1 on timeout (bumps underrun_count) or
 * when the ring is closing. */
int stream_ringbuf_wait_data_locked(struct rig_stream_ringbuf *rb,
                                    int timeout_ms)
{
    if (rb->closing)
    {
        return -1;
    }

    if (rb->count > 0)
    {
        return 0;
    }

    /* Block indefinitely: wait until data arrives or the ring is closed. */
    if (timeout_ms < 0)
    {
        while (rb->count == 0 && !rb->closing && !rb->failed)
        {
            pthread_cond_wait(&rb->data_available, &rb->lock);
        }

        return (rb->closing || rb->failed) ? -1 : 0;
    }

    struct timespec ts;

    clockid_t clk = rb->use_monotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME;
    clock_gettime(clk, &ts);
    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;

    if (ts.tv_nsec >= 1000000000L)
    {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    while (rb->count == 0)
    {
        if (rb->closing)
        {
            return -1;
        }

        int ret = pthread_cond_timedwait(&rb->data_available,
                                         &rb->lock, &ts);

        if (ret == ETIMEDOUT)
        {
            /* An underrun is the producer falling behind, which it cannot do
             * before it has produced anything. A consumer that starts first
             * and reads while the stream is still coming up is early, not
             * starved, and counting that leaves every stream reporting one
             * underrun it never suffered -- which hides the first real one.
             * write_total counts every byte ever produced and survives a
             * reset, so this only excuses the opening silence. */
            if (rb->write_total > 0)
            {
                rb->underrun_count++;
            }

            return -1;
        }
    }

    return 0;
}


/* Copy up to len readable bytes out of the buffer. Caller holds rb->lock. */
size_t stream_ringbuf_consume_locked(struct rig_stream_ringbuf *rb,
                                     unsigned char *dst, size_t len)
{
    if (len > rb->count)
    {
        len = rb->count;
    }

    size_t first = rb->capacity - rb->read_pos;

    if (first >= len)
    {
        memcpy(dst, rb->buffer + rb->read_pos, len);
    }
    else
    {
        memcpy(dst, rb->buffer + rb->read_pos, first);
        memcpy(dst + first, rb->buffer, len - first);
    }

    rb->read_pos = (rb->read_pos + len) & (rb->capacity - 1);
    rb->count -= len;

    return len;
}


size_t stream_ringbuf_read(struct rig_stream_ringbuf *rb,
                           void *data, size_t len, int timeout_ms)
{
    pthread_mutex_lock(&rb->lock);

    if (stream_ringbuf_wait_data_locked(rb, timeout_ms) < 0)
    {
        pthread_mutex_unlock(&rb->lock);
        return 0;
    }

    len = stream_ringbuf_consume_locked(rb, data, len);

    pthread_mutex_unlock(&rb->lock);

    return len;
}


size_t stream_ringbuf_available(struct rig_stream_ringbuf *rb)
{
    pthread_mutex_lock(&rb->lock);
    size_t count = rb->count;
    pthread_mutex_unlock(&rb->lock);
    return count;
}


void stream_ringbuf_reset(struct rig_stream_ringbuf *rb)
{
    pthread_mutex_lock(&rb->lock);
    rb->read_pos = 0;
    rb->write_pos = 0;
    rb->count = 0;
    pthread_mutex_unlock(&rb->lock);
}
