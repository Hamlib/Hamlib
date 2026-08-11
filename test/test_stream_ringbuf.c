/*
 *  Hamlib streaming ring-buffer tests
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

/* Ring buffer unit tests for the Hamlib streaming subsystem. */
/* Tests init/destroy, read/write, wraparound, overwrite, and concurrency. */

#include "acutest.h"
#include "test_debug.h"
#include "stream.h"
#include <string.h>
#include <pthread.h>


void test_ringbuf_init_destroy(void)
{
    struct rig_stream_ringbuf rb;
    int ret = stream_ringbuf_init(&rb, 1024);

    TEST_CHECK(ret == 0);
    TEST_MSG("stream_ringbuf_init returned %d", ret);
    TEST_CHECK(rb.buffer != NULL);
    TEST_CHECK(rb.capacity >= 1024);
    TEST_CHECK(rb.read_pos == 0);
    TEST_CHECK(rb.write_pos == 0);
    TEST_CHECK(rb.count == 0);
    TEST_CHECK(rb.overrun_count == 0);
    TEST_CHECK(rb.underrun_count == 0);

    stream_ringbuf_destroy(&rb);
    TEST_CHECK(rb.buffer == NULL);
}


void test_ringbuf_write_read(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 1024);

    unsigned char data[100];
    unsigned char out[100];

    for (int i = 0; i < 100; i++)
    {
        data[i] = (unsigned char)i;
    }

    size_t written = stream_ringbuf_write(&rb, data, 100);
    TEST_CHECK(written == 100);
    TEST_MSG("written = %zu", written);

    size_t avail = stream_ringbuf_available(&rb);
    TEST_CHECK(avail == 100);

    size_t nread = stream_ringbuf_read(&rb, out, 100, 100);
    TEST_CHECK(nread == 100);
    TEST_MSG("nread = %zu", nread);
    TEST_CHECK(memcmp(data, out, 100) == 0);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_partial_read(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 1024);

    unsigned char data[100];
    unsigned char out[50];

    for (int i = 0; i < 100; i++)
    {
        data[i] = (unsigned char)i;
    }

    stream_ringbuf_write(&rb, data, 100);

    size_t nread = stream_ringbuf_read(&rb, out, 50, 100);
    TEST_CHECK(nread == 50);
    TEST_CHECK(memcmp(data, out, 50) == 0);

    /* Second read should get the remaining 50 bytes */
    nread = stream_ringbuf_read(&rb, out, 50, 100);
    TEST_CHECK(nread == 50);
    TEST_CHECK(memcmp(data + 50, out, 50) == 0);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_wraparound(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 128);  /* Small buffer to force wraparound */

    unsigned char data[96];
    unsigned char out[96];

    for (int i = 0; i < 96; i++)
    {
        data[i] = (unsigned char)i;
    }

    /* Write 96 bytes, read 96 — positions now at 96 */
    stream_ringbuf_write(&rb, data, 96);
    stream_ringbuf_read(&rb, out, 96, 100);

    /* Write 96 more — wraps around at 128 */
    for (int i = 0; i < 96; i++)
    {
        data[i] = (unsigned char)(i + 100);
    }

    stream_ringbuf_write(&rb, data, 96);

    memset(out, 0, 96);
    size_t nread = stream_ringbuf_read(&rb, out, 96, 100);
    TEST_CHECK(nread == 96);
    TEST_MSG("nread = %zu", nread);
    TEST_CHECK(memcmp(data, out, 96) == 0);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_overwrite_oldest(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 64);  /* 64-byte buffer */

    unsigned char first[64];
    unsigned char second[32];
    unsigned char out[64];

    for (int i = 0; i < 64; i++)
    {
        first[i] = (unsigned char)i;
    }

    for (int i = 0; i < 32; i++)
    {
        second[i] = (unsigned char)(i + 200);
    }

    /* Fill the buffer */
    stream_ringbuf_write(&rb, first, 64);
    TEST_CHECK(rb.overrun_count == 0);

    /* Write 32 more — should overwrite oldest 32 bytes */
    stream_ringbuf_write(&rb, second, 32);
    TEST_CHECK(rb.overrun_count > 0);
    TEST_MSG("overrun_count = %d", rb.overrun_count);

    /* Read should give us: last 32 of first + 32 of second */
    size_t nread = stream_ringbuf_read(&rb, out, 64, 100);
    TEST_CHECK(nread == 64);
    TEST_CHECK(memcmp(out, first + 32, 32) == 0);
    TEST_CHECK(memcmp(out + 32, second, 32) == 0);

    stream_ringbuf_destroy(&rb);
}


/* Atomic record write: hdr+payload land together, or not at all — a full
 * ring never gets a partial record and never overwrites (drop-newest for
 * codec streams). peek does not consume. */
void test_ringbuf_write_record_atomic(void)
{
    struct rig_stream_ringbuf rb;
    TEST_ASSERT(stream_ringbuf_init(&rb, 64) == 0);   /* rounds to 64 */

    unsigned char hdr[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    unsigned char payload[16];
    unsigned char out[64];

    for (int i = 0; i < 16; i++)
    {
        payload[i] = (unsigned char)i;
    }

    /* 3 x 20-byte records fill 60 of 64 bytes. */
    for (int r = 0; r < 3; r++)
    {
        size_t n = stream_ringbuf_write_record(&rb, hdr, sizeof(hdr),
                                               payload, sizeof(payload));
        TEST_CHECK(n == 20);
    }

    TEST_CHECK(stream_ringbuf_available(&rb) == 60);

    /* A 4th record does not fit: nothing written (overrun accounting is
     * the caller's call — a retrying TX writer polls this path). */
    size_t n = stream_ringbuf_write_record(&rb, hdr, sizeof(hdr),
                                           payload, sizeof(payload));
    TEST_CHECK(n == 0);
    TEST_CHECK(stream_ringbuf_available(&rb) == 60);
    TEST_CHECK(rb.overrun_count == 0);

    /* Peek copies the head record without consuming. */
    pthread_mutex_lock(&rb.lock);
    size_t got = stream_ringbuf_peek_locked(&rb, out, 20);
    pthread_mutex_unlock(&rb.lock);
    TEST_CHECK(got == 20);
    TEST_CHECK(memcmp(out, hdr, 4) == 0);
    TEST_CHECK(memcmp(out + 4, payload, 16) == 0);
    TEST_CHECK(stream_ringbuf_available(&rb) == 60);

    /* Consuming one record makes room for exactly one more. */
    got = stream_ringbuf_read(&rb, out, 20, 0);
    TEST_CHECK(got == 20);
    n = stream_ringbuf_write_record(&rb, hdr, sizeof(hdr),
                                    payload, sizeof(payload));
    TEST_CHECK(n == 20);

    stream_ringbuf_destroy(&rb);
}


/* Record writes across the wrap point stay intact. */
void test_ringbuf_write_record_wraparound(void)
{
    struct rig_stream_ringbuf rb;
    TEST_ASSERT(stream_ringbuf_init(&rb, 64) == 0);

    unsigned char pad[40];
    unsigned char out[64];
    memset(pad, 0x55, sizeof(pad));

    /* Advance positions close to the end, then drain. */
    TEST_CHECK(stream_ringbuf_write(&rb, pad, 40) == 40);
    TEST_CHECK(stream_ringbuf_read(&rb, out, 40, 0) == 40);

    /* This record spans the wrap boundary (write_pos = 40, cap 64). */
    unsigned char hdr[4] = { 1, 2, 3, 4 };
    unsigned char payload[28];

    for (int i = 0; i < 28; i++)
    {
        payload[i] = (unsigned char)(0x80 + i);
    }

    size_t n = stream_ringbuf_write_record(&rb, hdr, sizeof(hdr),
                                           payload, sizeof(payload));
    TEST_CHECK(n == 32);

    size_t got = stream_ringbuf_read(&rb, out, 32, 0);
    TEST_CHECK(got == 32);
    TEST_CHECK(memcmp(out, hdr, 4) == 0);
    TEST_CHECK(memcmp(out + 4, payload, 28) == 0);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_read_timeout(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 1024);

    unsigned char out[10];

    /* Read from empty buffer with short timeout */
    size_t nread = stream_ringbuf_read(&rb, out, 10, 10);
    TEST_CHECK(nread == 0);
    TEST_MSG("nread = %zu (expected 0 on timeout)", nread);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_underrun_count(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 1024);

    unsigned char out[10];
    TEST_CHECK(rb.underrun_count == 0);

    /* Read from empty buffer — should timeout and increment underrun */
    stream_ringbuf_read(&rb, out, 10, 10);
    TEST_CHECK(rb.underrun_count == 1);
    TEST_MSG("underrun_count = %d", rb.underrun_count);

    /* Second timeout */
    stream_ringbuf_read(&rb, out, 10, 10);
    TEST_CHECK(rb.underrun_count == 2);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_available(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 1024);

    unsigned char data[100];
    unsigned char out[50];

    TEST_CHECK(stream_ringbuf_available(&rb) == 0);

    stream_ringbuf_write(&rb, data, 100);
    TEST_CHECK(stream_ringbuf_available(&rb) == 100);

    stream_ringbuf_read(&rb, out, 50, 100);
    TEST_CHECK(stream_ringbuf_available(&rb) == 50);

    stream_ringbuf_read(&rb, out, 50, 100);
    TEST_CHECK(stream_ringbuf_available(&rb) == 0);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_reset(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 1024);

    unsigned char data[100];
    stream_ringbuf_write(&rb, data, 100);
    TEST_CHECK(stream_ringbuf_available(&rb) == 100);

    stream_ringbuf_reset(&rb);
    TEST_CHECK(stream_ringbuf_available(&rb) == 0);
    TEST_CHECK(rb.read_pos == 0);
    TEST_CHECK(rb.write_pos == 0);
    TEST_CHECK(rb.count == 0);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_power_of_two(void)
{
    struct rig_stream_ringbuf rb;

    /* Request 100 bytes — should round up to 128 */
    stream_ringbuf_init(&rb, 100);
    TEST_CHECK(rb.capacity == 128);
    TEST_MSG("capacity = %zu (expected 128)", rb.capacity);
    stream_ringbuf_destroy(&rb);

    /* Request 64 — should stay 64 */
    stream_ringbuf_init(&rb, 64);
    TEST_CHECK(rb.capacity == 64);
    stream_ringbuf_destroy(&rb);

    /* Request 1 — should round up to some minimum */
    stream_ringbuf_init(&rb, 1);
    TEST_CHECK(rb.capacity >= 1);
    TEST_CHECK((rb.capacity & (rb.capacity - 1)) == 0);
    TEST_MSG("capacity = %zu (must be power of 2)", rb.capacity);
    stream_ringbuf_destroy(&rb);
}


/* Producer thread for concurrency test */
struct concurrent_test_args
{
    struct rig_stream_ringbuf *rb;
    int chunk_size;
    int chunk_count;
};

static void *producer_thread(void *arg)
{
    struct concurrent_test_args *args = arg;
    unsigned char buf[256];

    for (int i = 0; i < args->chunk_count; i++)
    {
        /* Fill buffer with sequential pattern: chunk index */
        memset(buf, (unsigned char)i, args->chunk_size);
        stream_ringbuf_write(args->rb, buf, args->chunk_size);
    }

    return NULL;
}


void test_ringbuf_concurrent(void)
{
    struct rig_stream_ringbuf rb;
    stream_ringbuf_init(&rb, 4096);

    struct concurrent_test_args args =
    {
        .rb = &rb,
        .chunk_size = 64,
        .chunk_count = 50
    };

    pthread_t prod;
    pthread_create(&prod, NULL, producer_thread, &args);

    /* Consumer: read all chunks and verify no corruption */
    unsigned char out[64];
    int chunks_read = 0;

    for (int i = 0; i < 50; i++)
    {
        size_t total = 0;

        while (total < 64)
        {
            size_t n = stream_ringbuf_read(&rb, out + total, 64 - total, 500);

            if (n == 0)
            {
                break;  /* Timeout — producer may be done */
            }

            total += n;
        }

        if (total == 64)
        {
            chunks_read++;

            /* Each byte in the chunk should be the same value */
            int consistent = 1;

            for (int j = 1; j < 64; j++)
            {
                if (out[j] != out[0])
                {
                    consistent = 0;
                    break;
                }
            }

            TEST_CHECK(consistent);
            TEST_MSG("Chunk %d data inconsistent (byte[0]=%d, found mismatch)",
                     i, out[0]);
        }
    }

    pthread_join(prod, NULL);

    TEST_CHECK(chunks_read == 50);
    TEST_MSG("chunks_read = %d (expected 50)", chunks_read);

    stream_ringbuf_destroy(&rb);
}


void test_ringbuf_init_zero_capacity(void)
{
    struct rig_stream_ringbuf rb;
    int ret = stream_ringbuf_init(&rb, 0);

    TEST_CHECK(ret == -1);
    TEST_MSG("stream_ringbuf_init(0) should fail, returned %d", ret);
}


TEST_LIST =
{
    { "stream_ringbuf_init_zero_capacity", test_ringbuf_init_zero_capacity },
    { "stream_ringbuf_init_destroy",   test_ringbuf_init_destroy },
    { "stream_ringbuf_write_read",     test_ringbuf_write_read },
    { "stream_ringbuf_partial_read",   test_ringbuf_partial_read },
    { "stream_ringbuf_wraparound",     test_ringbuf_wraparound },
    { "stream_ringbuf_overwrite_oldest", test_ringbuf_overwrite_oldest },
    { "stream_ringbuf_write_record_atomic", test_ringbuf_write_record_atomic },
    { "stream_ringbuf_write_record_wrap", test_ringbuf_write_record_wraparound },
    { "stream_ringbuf_read_timeout",   test_ringbuf_read_timeout },
    { "stream_ringbuf_underrun_count", test_ringbuf_underrun_count },
    { "stream_ringbuf_available",      test_ringbuf_available },
    { "stream_ringbuf_reset",          test_ringbuf_reset },
    { "stream_ringbuf_power_of_two",   test_ringbuf_power_of_two },
    { "stream_ringbuf_concurrent",     test_ringbuf_concurrent },
    { NULL, NULL }
};
