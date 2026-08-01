/*
 * Hamlib GUOHETEC status framing tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "guohetec.h"

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_SYS_SOCKET_H)
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#define TEST_GUOHE_TRANSACTIONS 1
#endif

static const unsigned char captured_firmware_3_5_status[] = {
    0xA5, 0xA5, 0xA5, 0xA5, 0x1B, 0x0B, 0x00, 0x00,
    0x00, 0x01, 0x41, 0xA7, 0xC0, 0x01, 0x41, 0x90,
    0x50, 0x00, 0x00, 0x3C, 0x3C, 0x32, 0x00, 0xA6,
    0x05, 0x2C, 0x0C, 0x21, 0x01, 0x00, 0x14, 0xB0
};

static const unsigned char documented_status[] = {
    0xA5, 0xA5, 0xA5, 0xA5, 0x1B, 0x0B, 0x01, 0x05,
    0x04, 0x08, 0xAC, 0x27, 0x60, 0x19, 0xED, 0x92,
    0xC0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0xD4
};

static int expect_valid_status(const char *name, const unsigned char *frame,
                               size_t frame_size, uint32_t freq_a,
                               uint32_t freq_b, unsigned char ptt,
                               unsigned char mode_a, unsigned char mode_b,
                               vfo_t vfo)
{
    struct guohetec_status status;
    int ret = guohetec_decode_status(frame, frame_size, &status, name);

    if (ret != RIG_OK || status.freq_a != freq_a || status.freq_b != freq_b ||
            status.ptt != ptt || status.mode_a != mode_a ||
            status.mode_b != mode_b || status.vfo != vfo)
    {
        fprintf(stderr,
                "%s: unexpected decode result %d/%u/%u/%u/%u/%u/%d\n",
                name, ret, status.freq_a, status.freq_b, status.ptt,
                status.mode_a, status.mode_b, status.vfo);
        return 1;
    }

    return 0;
}

static int expect_invalid_status(const char *name, const unsigned char *frame,
                                 size_t frame_size)
{
    struct guohetec_status status;

    if (guohetec_decode_status(frame, frame_size, &status, name) >= 0)
    {
        fprintf(stderr, "%s: malformed status response was accepted\n", name);
        return 1;
    }

    return 0;
}

static int test_status_decoder(void)
{
    unsigned char invalid[sizeof(captured_firmware_3_5_status)];
    static const unsigned char short_status[] = {
        0xA5, 0xA5, 0xA5, 0xA5, 0x03, 0x0B, 0x00, 0x00
    };

    if (expect_valid_status("firmware-3.5", captured_firmware_3_5_status,
                            sizeof(captured_firmware_3_5_status),
                            21080000, 21074000, 0, 0, 0, RIG_VFO_A) != 0 ||
            expect_valid_status("documented", documented_status,
                                sizeof(documented_status),
                                145500000, 435000000, 1, 5, 4,
                                RIG_VFO_B) != 0)
    {
        return 1;
    }

    memcpy(invalid, captured_firmware_3_5_status, sizeof(invalid));
    invalid[0] = 0;

    if (expect_invalid_status("bad-header", invalid, sizeof(invalid)) != 0)
    {
        return 1;
    }

    memcpy(invalid, captured_firmware_3_5_status, sizeof(invalid));
    invalid[4]--;

    if (expect_invalid_status("length-mismatch", invalid, sizeof(invalid)) != 0 ||
            expect_invalid_status("truncated", captured_firmware_3_5_status,
                                  sizeof(captured_firmware_3_5_status) - 1) != 0 ||
            expect_invalid_status("short-payload", short_status,
                                  sizeof(short_status)) != 0)
    {
        return 1;
    }

    memcpy(invalid, captured_firmware_3_5_status, sizeof(invalid));
    invalid[4] = 0xFF;

    if (expect_invalid_status("oversized-length", invalid, sizeof(invalid)) != 0)
    {
        return 1;
    }

    memcpy(invalid, captured_firmware_3_5_status, sizeof(invalid));
    invalid[5] = 0x0A;

    if (expect_invalid_status("wrong-command", invalid, sizeof(invalid)) != 0)
    {
        return 1;
    }

    memcpy(invalid, captured_firmware_3_5_status, sizeof(invalid));
    invalid[sizeof(invalid) - 1] ^= 0x01;

    return expect_invalid_status("bad-crc", invalid, sizeof(invalid));
}

#ifdef TEST_GUOHE_TRANSACTIONS

static const unsigned char status_request[] = {
    0xA5, 0xA5, 0xA5, 0xA5, 0x03, 0x0B, 0xF9, 0x37
};

struct peer_case
{
    int fd;
    const unsigned char **responses;
    size_t response_count;
    int status;
};

static int read_all(int fd, unsigned char *buffer, size_t length)
{
    size_t total = 0;

    while (total < length)
    {
        ssize_t count = read(fd, buffer + total, length - total);

        if (count <= 0) { return -1; }

        total += (size_t)count;
    }

    return 0;
}

static int write_all(int fd, const unsigned char *buffer, size_t length)
{
    size_t total = 0;

    while (total < length)
    {
        ssize_t count = write(fd, buffer + total, length - total);

        if (count <= 0) { return -1; }

        total += (size_t)count;
    }

    return 0;
}

static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    unsigned char request[sizeof(status_request)];

    test->status = -1;

    for (size_t i = 0; i < test->response_count; i++)
    {
        if (read_all(test->fd, request, sizeof(request)) != 0 ||
                memcmp(request, status_request, sizeof(request)) != 0 ||
                write_all(test->fd, test->responses[i],
                          sizeof(captured_firmware_3_5_status)) != 0)
        {
            return NULL;
        }
    }

    test->status = 0;
    return NULL;
}

static int open_test_rig(rig_model_t model, int fd, RIG **rig)
{
    *rig = rig_init(model);

    if (*rig == NULL) { return -1; }

    RIGPORT(*rig)->fd = fd;
    RIGPORT(*rig)->timeout = 250;
    RIGPORT(*rig)->retry = 0;
    return 0;
}

static int run_transaction_case(const char *name, rig_model_t model)
{
    const unsigned char *responses[] = {
        captured_firmware_3_5_status,
        documented_status,
        captured_firmware_3_5_status
    };
    struct peer_case test = {
        .fd = -1,
        .responses = responses,
        .response_count = sizeof(responses) / sizeof(responses[0]),
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    RIG *rig = NULL;
    freq_t freq_a = 0;
    freq_t freq_b = 0;
    ptt_t ptt = RIG_PTT_ON;
    int ret_a = -1;
    int ret_b = -1;
    int ret_ptt = -1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    if (open_test_rig(model, sockets[0], &rig) == 0)
    {
        ret_a = rig->caps->get_freq(rig, RIG_VFO_A, &freq_a);
        ret_b = rig->caps->get_freq(rig, RIG_VFO_B, &freq_b);
        ret_ptt = rig->caps->get_ptt(rig, RIG_VFO_A, &ptt);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
    }

    close(sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (test.status != 0 || ret_a != RIG_OK || ret_b != RIG_OK ||
            ret_ptt != RIG_OK || freq_a != 21080000 || freq_b != 435000000 ||
            ptt != RIG_PTT_OFF)
    {
        fprintf(stderr,
                "%s: transaction mismatch %d/%d/%d/%g/%g/%d peer=%d\n",
                name, ret_a, ret_b, ret_ptt, freq_a, freq_b, ptt,
                test.status);
        return 1;
    }

    return 0;
}

static int test_cached_fallback(void)
{
    unsigned char corrupt_status[sizeof(captured_firmware_3_5_status)];
    const unsigned char *responses[] = {
        captured_firmware_3_5_status,
        corrupt_status
    };
    struct peer_case test = {
        .fd = -1,
        .responses = responses,
        .response_count = sizeof(responses) / sizeof(responses[0]),
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    RIG *rig = NULL;
    freq_t first = 0;
    freq_t cached = 0;
    int first_ret = -1;
    int cached_ret = -1;

    memcpy(corrupt_status, captured_firmware_3_5_status,
           sizeof(corrupt_status));
    corrupt_status[sizeof(corrupt_status) - 1] ^= 0x01;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    if (open_test_rig(RIG_MODEL_PMR171, sockets[0], &rig) == 0)
    {
        first_ret = rig->caps->get_freq(rig, RIG_VFO_A, &first);
        cached_ret = rig->caps->get_freq(rig, RIG_VFO_B, &cached);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
    }

    close(sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (test.status != 0 || first_ret != RIG_OK || cached_ret != RIG_OK ||
            first != 21080000 || cached != 21074000)
    {
        fprintf(stderr,
                "cached-fallback: mismatch %d/%d/%g/%g peer=%d\n",
                first_ret, cached_ret, first, cached, test.status);
        return 1;
    }

    return 0;
}

#endif

int main(void)
{
    if (test_status_decoder() != 0) { return 1; }

#ifdef TEST_GUOHE_TRANSACTIONS
    rig_register(&pmr171_caps);
    rig_register(&q900_caps);

    if (run_transaction_case("PMR-171", RIG_MODEL_PMR171) != 0 ||
            run_transaction_case("Q900", RIG_MODEL_Q900) != 0 ||
            test_cached_fallback() != 0)
    {
        return 1;
    }
#endif

    return 0;
}
