/*
 * Hamlib Elecraft K4 VFO response tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "cache.h"
#include "misc.h"

extern struct rig_caps k4_caps;

struct peer_case
{
    int fd;
    const char *responses[3];
    int status;
};

static int close_test_socket(int fd)
{
#ifdef _WIN32
    return closesocket(fd);
#else
    return close(fd);
#endif
}

static int read_test_socket(int fd, void *buffer, size_t length)
{
#ifdef _WIN32
    return recv(fd, buffer, (int) length, 0);
#else
    return (int) read(fd, buffer, length);
#endif
}

static int write_test_socket(int fd, const void *buffer, size_t length)
{
#ifdef _WIN32
    return send(fd, buffer, (int) length, 0);
#else
    return (int) write(fd, buffer, length);
#endif
}

static int open_test_connection(int sockets[2])
{
#ifdef _WIN32
    SOCKET listener;
    SOCKET client;
    SOCKET peer;
    struct sockaddr_in address;
    int address_length = sizeof(address);

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == INVALID_SOCKET) { return -1; }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(listener, (struct sockaddr *) &address, sizeof(address)) != 0 ||
            listen(listener, 1) != 0 ||
            getsockname(listener, (struct sockaddr *) &address,
                        &address_length) != 0)
    {
        closesocket(listener);
        return -1;
    }

    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client == INVALID_SOCKET ||
            connect(client, (struct sockaddr *) &address,
                    sizeof(address)) != 0)
    {
        if (client != INVALID_SOCKET) { closesocket(client); }

        closesocket(listener);
        return -1;
    }

    peer = accept(listener, NULL, NULL);
    closesocket(listener);

    if (peer == INVALID_SOCKET)
    {
        closesocket(client);
        return -1;
    }

    sockets[0] = (int) client;
    sockets[1] = (int) peer;
    return 0;
#else
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
#endif
}

static int read_command(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        int count = read_test_socket(fd, buffer + used, 1);

        if (count != 1) { return -1; }

        if (buffer[used++] == ';')
        {
            buffer[used] = '\0';
            return 0;
        }
    }

    return -1;
}

static int write_all(int fd, const char *buffer, size_t length)
{
    size_t written = 0;

    while (written < length)
    {
        int count = write_test_socket(fd, buffer + written, length - written);

        if (count <= 0) { return -1; }

        written += (size_t)count;
    }

    return 0;
}

static void *run_peer(void *arg)
{
    static const char *commands[] = { "FR;", "FT;", "TQ;" };
    struct peer_case *test = arg;
    char command[8];
    size_t i;

    test->status = -1;

    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        if (test->responses[i] == NULL) { break; }

        if (read_command(test->fd, command, sizeof(command)) != 0
                || strcmp(command, commands[i]) != 0)
        {
            return NULL;
        }

        if (write_all(test->fd, test->responses[i],
                      strlen(test->responses[i])) != 0)
        {
            return NULL;
        }
    }

    test->status = 0;
    return NULL;
}

static int run_case(const char *name, const char *fr, const char *ft,
                    const char *tq, int expected_retval,
                    vfo_t expected_vfo, vfo_t expected_rx_vfo,
                    vfo_t expected_tx_vfo)
{
    const vfo_t initial_vfo = RIG_VFO_MEM;
    const vfo_t initial_rx_vfo = RIG_VFO_MAIN;
    const vfo_t initial_tx_vfo = RIG_VFO_SUB;
    int sockets[2];
    pthread_t thread;
    struct peer_case test =
    {
        .fd = -1,
        .responses = { fr, ft, tq },
        .status = -1
    };
    RIG *rig;
    vfo_t vfo = initial_vfo;
    vfo_t actual_rx_vfo;
    vfo_t actual_tx_vfo;
    int retval;

    if (open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "test socket setup failed\n");
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    rig = rig_init(RIG_MODEL_K4);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    rig_set_rx_vfo_state(rig, initial_rx_vfo);
    rig_set_tx_vfo_state(rig, initial_tx_vfo);

    retval = rig->caps->get_vfo(rig, &vfo);
    {
        struct rig_cache_routing_snapshot routing;

        rig_get_cache_routing_snapshot(rig, &routing);
        actual_rx_vfo = routing.rx_vfo;
        actual_tx_vfo = routing.tx_vfo;
    }

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (expected_retval != RIG_OK)
    {
        expected_vfo = initial_vfo;
        expected_rx_vfo = initial_rx_vfo;
        expected_tx_vfo = initial_tx_vfo;
    }

    if (test.status != 0 || retval != expected_retval
            || vfo != expected_vfo
            || actual_rx_vfo != expected_rx_vfo
            || actual_tx_vfo != expected_tx_vfo)
    {
        fprintf(stderr,
                "%s: expected %d/%d/%d/%d, got %d/%d/%d/%d\n",
                name, expected_retval, expected_vfo, expected_rx_vfo,
                expected_tx_vfo, retval, vfo, actual_rx_vfo, actual_tx_vfo);
        return 1;
    }

    return 0;
}

enum setter_behavior
{
    SETTER_ACCEPT,
    SETTER_REJECT,
    SETTER_DELAY,
    SETTER_NOTIFICATION_ROLLBACK,
    SETTER_READBACK_ROLLBACK,
    SETTER_BARRIER_ROLLBACK,
    SETTER_UNCHANGED_ROLLBACK
};

struct setter_case
{
    const char *name;
    vfo_t vfo;
    freq_t initial;
    freq_t requested;
    enum setter_behavior behavior;
    freq_t rollback;
    const char *verify_error;
    const char *readback_error;
    const char *queued_error;
    int verify_timeout;
    int readback_timeout;
    int readback_delay_ms;
    int retry_id;
    int stale_cache;
    int expected_retval;
};

struct setter_peer
{
    int fd;
    const struct setter_case *test;
    int setters;
    int id_queries;
    int verification_queries;
    int queries;
    int readbacks;
    int status;
};

static int bound_peer_reads(int fd)
{
#ifdef _WIN32
    DWORD timeout = 3000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                      (const char *)&timeout, sizeof(timeout));
#else
    struct timeval timeout = { 3, 0 };
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

static void shutdown_test_socket(int fd)
{
#ifdef _WIN32
    shutdown(fd, SD_BOTH);
#else
    shutdown(fd, SHUT_RDWR);
#endif
}

static int setter_reply(struct setter_peer *peer, freq_t value)
{
    char reply[32];

    if (peer->test->vfo == RIG_VFO_NONE)
    {
        snprintf(reply, sizeof(reply), "FT%d;", (int)value);
    }
    else
    {
        char vfo = peer->test->vfo == RIG_VFO_B
                   || peer->test->vfo == RIG_VFO_SUB ? 'B' : 'A';
        snprintf(reply, sizeof(reply), "F%c%011lld;", vfo, (long long)value);
    }

    return write_all(peer->fd, reply, strlen(reply));
}

static int handle_setter_command(struct setter_peer *peer, const char *command)
{
    const struct setter_case *test = peer->test;
    char query[8];
    char setter[32];

    if (test->vfo == RIG_VFO_NONE)
    {
        snprintf(query, sizeof(query), "FT;");
        snprintf(setter, sizeof(setter), "FT%d;", (int)test->requested);
    }
    else
    {
        char vfo = test->vfo == RIG_VFO_B || test->vfo == RIG_VFO_SUB ? 'B' : 'A';
        snprintf(query, sizeof(query), "F%c;", vfo);
        snprintf(setter, sizeof(setter), "F%c%011lld;", vfo,
                 (long long)test->requested);
    }

    if (strcmp(command, setter) == 0)
    {
        peer->setters++;

        if (test->behavior == SETTER_NOTIFICATION_ROLLBACK)
        {
            return setter_reply(peer, test->requested);
        }

        return 0;
    }

    if (strcmp(command, "ID;") == 0)
    {
        peer->id_queries++;

        if (peer->setters != 0)
        {
            peer->verification_queries++;

            if (test->verify_timeout) { return 0; }

            if (test->retry_id && peer->verification_queries == 1) { return 0; }

            if (test->verify_error != NULL)
            {
                return write_all(peer->fd, test->verify_error,
                                 strlen(test->verify_error));
            }
        }

        return write_all(peer->fd, "ID017;", 6);
    }

    if (strcmp(command, query) == 0)
    {
        freq_t value = test->initial;
        peer->queries++;

        if (test->behavior == SETTER_UNCHANGED_ROLLBACK && peer->queries > 1)
        {
            value = test->rollback;
        }

        if (peer->setters != 0)
        {
            peer->readbacks++;

            if (test->readback_delay_ms)
            {
                hl_usleep(test->readback_delay_ms * 1000);
            }

            if (test->readback_timeout) { return 0; }

            if (test->readback_error != NULL)
            {
                return write_all(peer->fd, test->readback_error,
                                 strlen(test->readback_error));
            }

            if (test->behavior == SETTER_ACCEPT
                    || (test->behavior == SETTER_DELAY && peer->readbacks > 1)
                    || (test->behavior == SETTER_READBACK_ROLLBACK
                        && peer->readbacks == 1)
                    || (test->behavior == SETTER_BARRIER_ROLLBACK
                        && peer->readbacks <= 2))
            {
                value = test->requested;
            }

            if (test->behavior == SETTER_BARRIER_ROLLBACK && peer->readbacks == 2)
            {
                if (setter_reply(peer, value) != 0) { return -1; }

                return setter_reply(peer, test->initial);
            }

            if (test->queued_error != NULL && peer->readbacks == 1)
            {
                if (setter_reply(peer, value) != 0) { return -1; }

                return write_all(peer->fd, test->queued_error,
                                 strlen(test->queued_error));
            }
        }

        return setter_reply(peer, value);
    }

    if (strcmp(command, "FR;") == 0) { return write_all(peer->fd, "FR0;", 4); }

    if (strcmp(command, "FT;") == 0) { return write_all(peer->fd, "FT0;", 4); }

    if (strcmp(command, "TQ;") == 0) { return write_all(peer->fd, "TQ0;", 4); }

    fprintf(stderr, "%s: unexpected CAT command %s\n", test->name, command);
    return -1;
}

static void *run_setter_peer(void *arg)
{
    struct setter_peer *peer = arg;
    char command[64];

    while (read_command(peer->fd, command, sizeof(command)) == 0)
    {
        if (handle_setter_command(peer, command) != 0)
        {
            peer->status = 1;
            shutdown_test_socket(peer->fd);
            break;
        }
    }

    return NULL;
}

static int run_setter_case(const struct setter_case *test)
{
    const freq_t untouched_b = 14074000;
    struct setter_peer peer = { .fd = -1, .test = test };
    struct rig_cache_routing_snapshot routing;
    int sockets[2];
    pthread_t thread;
    RIG *rig;
    freq_t cached = 0;
    freq_t cached_b = 0;
    int cache_age = 0;
    int cache_timeout;
    int retval;
    int failed = 0;
    int expected_setters = test->initial == test->requested ? 0 : 1;
    freq_t expected_actual = test->behavior == SETTER_UNCHANGED_ROLLBACK
                             ? test->rollback : test->initial;

    if (open_test_connection(sockets) != 0) { return 1; }

    if (bound_peer_reads(sockets[1]) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    peer.fd = sockets[1];
    rig = rig_init(RIG_MODEL_K4);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    /* Confirmation needs time for multiple CAT exchanges and 50 ms polls. */
    RIGPORT(rig)->timeout = (test->verify_timeout || test->readback_timeout)
                            ? 160 : 1000;
    RIGPORT(rig)->retry = 1;
    STATE(rig)->comm_state = 1;
    STATE(rig)->twiddle_timeout = 0;
    rig_set_vfo_state(rig, RIG_VFO_A, RIG_VFO_B);
    rig_set_split_routing_state(rig, RIG_SPLIT_OFF, RIG_VFO_A, RIG_VFO_A);
    rig_set_cache_freq(rig, RIG_VFO_A, 28074000);
    rig_set_cache_freq(rig, RIG_VFO_B, untouched_b);

    if (test->vfo == RIG_VFO_NONE)
    {
        split_t initial = (split_t)(test->stale_cache ? test->requested : test->initial);
        rig_set_split_routing_state(rig, initial, RIG_VFO_A,
                                    initial ? RIG_VFO_B : RIG_VFO_A);
    }
    else
    {
        rig_set_cache_freq(rig, test->vfo,
                           test->stale_cache ? test->requested : test->initial);
    }

    if (pthread_create(&thread, NULL, run_setter_peer, &peer) != 0)
    {
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    if (test->vfo == RIG_VFO_NONE)
    {
        retval = rig_set_split_vfo(rig, RIG_VFO_A, (split_t)test->requested,
                                   test->requested ? RIG_VFO_B : RIG_VFO_A);
    }
    else
    {
        retval = rig_set_freq(rig, test->vfo, test->requested);
    }

    rig_get_cache_routing_snapshot(rig, &routing);

    if (test->vfo == RIG_VFO_NONE)
    {
        split_t expected_split = (split_t)(retval == RIG_OK
                                           ? test->requested : expected_actual);
        split_t cached_split;
        vfo_t cached_tx;
        vfo_t expected_tx = expected_split ? RIG_VFO_B : RIG_VFO_A;
        rig_get_cache_freq(rig, RIG_VFO_B, &cached_b, NULL);
        rig_get_cache_split(rig, &cached_split, &cached_tx, &cache_age,
                            &cache_timeout);

        if (routing.split != expected_split || routing.rx_vfo != RIG_VFO_A
                || routing.tx_vfo != expected_tx || routing.split_vfo != expected_tx
                || cached_b != untouched_b)
        {
            fprintf(stderr, "%s: split/routing/B cache = %d/%d/%d/%d/%.0f\n",
                    test->name, routing.split, routing.rx_vfo, routing.tx_vfo,
                    routing.split_vfo, cached_b);
            failed = 1;
        }
    }
    else
    {
        rig_get_cache_freq(rig, test->vfo, &cached, &cache_age);
        cache_timeout = rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_FREQ);

        if ((retval == RIG_OK && cached != test->requested)
                || (retval == -RIG_ERJCTED && test->verify_error == NULL
                    && test->readback_error == NULL && test->queued_error == NULL
                    && cached != expected_actual)
                || (retval != RIG_OK && cached == test->requested
                    && cache_age < cache_timeout))
        {
            fprintf(stderr, "%s: frequency cache = %.0f\n", test->name, cached);
            failed = 1;
        }

        if (retval != RIG_OK && STATE(rig)->current_freq == test->requested)
        {
            fprintf(stderr, "%s: requested current frequency survived failure\n",
                    test->name);
            failed = 1;
        }

        if (retval == -RIG_ERJCTED && test->verify_error == NULL
                && test->readback_error == NULL && test->queued_error == NULL)
        {
            freq_t observed = 0;
            int read_retval = rig_get_freq(rig, test->vfo, &observed);

            if (read_retval != RIG_OK || observed != expected_actual)
            {
                fprintf(stderr, "%s: immediate get_freq returned %d/%.0f\n",
                        test->name, read_retval, observed);
                failed = 1;
            }
        }
    }

    if (retval != RIG_OK
            && (test->verify_error != NULL || test->readback_error != NULL
                || test->queued_error != NULL
                || test->verify_timeout || test->readback_timeout)
            && cache_age < cache_timeout)
    {
        fprintf(stderr, "%s: failed operation left a fresh cache (%d ms)\n",
                test->name, cache_age);
        failed = 1;
    }

    if (routing.current_vfo != RIG_VFO_A)
    {
        fprintf(stderr, "%s: current VFO changed to %d\n", test->name,
                routing.current_vfo);
        failed = 1;
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    shutdown_test_socket(sockets[0]);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (retval != test->expected_retval || peer.status != 0
            || peer.setters != expected_setters
            || (expected_setters && peer.id_queries == 0)
            || ((test->behavior == SETTER_DELAY
                 || test->behavior == SETTER_READBACK_ROLLBACK)
                && peer.readbacks < 3)
            || (test->retry_id && peer.verification_queries < 2))
    {
        fprintf(stderr, "%s: expected result/setters %d/%d, got %d/%d "
                "(ID=%d, readbacks=%d, peer=%d)\n", test->name,
                test->expected_retval, expected_setters, retval, peer.setters,
                peer.id_queries, peer.readbacks, peer.status);
        failed = 1;
    }

    return failed;
}

static int run_setter_cases(void)
{
    static const struct setter_case cases[] =
    {
        { .name = "frequency-rejected-after-id", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 5357000, .behavior = SETTER_REJECT,
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-main-accepted", .vfo = RIG_VFO_MAIN,
          .initial = 28074000, .requested = 28075000 },
        { .name = "frequency-sub-accepted", .vfo = RIG_VFO_SUB,
          .initial = 14074000, .requested = 14075000 },
        { .name = "frequency-b-unchanged", .vfo = RIG_VFO_B,
          .initial = 14074000, .requested = 14074000 },
        { .name = "frequency-unchanged-match-then-rollback", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28074000, .rollback = 14074000,
          .behavior = SETTER_UNCHANGED_ROLLBACK, .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-stale-requested-cache", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000, .stale_cache = 1 },
        { .name = "frequency-rejected-stale-requested-cache", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 5357000, .behavior = SETTER_REJECT,
          .stale_cache = 1, .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-unsolicited-match-rollback", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 5357000,
          .behavior = SETTER_NOTIFICATION_ROLLBACK, .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-readback-match-rollback", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 5357000,
          .behavior = SETTER_READBACK_ROLLBACK, .readback_delay_ms = 60,
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-second-match-rollback-before-barrier", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 5357000,
          .behavior = SETTER_BARRIER_ROLLBACK, .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-delayed-acceptance", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000, .behavior = SETTER_DELAY,
          .readback_delay_ms = 60 },
        { .name = "frequency-id-timeout-does-not-repeat-setter", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000, .retry_id = 1 },
        { .name = "frequency-explicit-er01", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 5357000,
          .verify_error = "ER01;", .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-explicit-er04-text", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 5357000,
          .readback_error = "ER04: VFO A frequency out of range;",
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-queued-er04-during-confirmation", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .queued_error = "ER04: VFO A frequency out of range;",
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-readback-timeout", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000, .readback_timeout = 1,
          .expected_retval = -RIG_ETIMEOUT },
        { .name = "frequency-notification-then-timeout", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .behavior = SETTER_NOTIFICATION_ROLLBACK, .readback_timeout = 1,
          .expected_retval = -RIG_ETIMEOUT },
        { .name = "frequency-verification-timeout", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000, .verify_timeout = 1,
          .expected_retval = -RIG_ETIMEOUT },
        { .name = "frequency-malformed-numeric-readback", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .readback_error = "FA00028X75000;", .expected_retval = -RIG_EPROTO },
        { .name = "split-rejected-after-id", .vfo = RIG_VFO_NONE,
          .requested = 1, .behavior = SETTER_REJECT, .expected_retval = -RIG_ERJCTED },
        { .name = "split-on-accepted-preserves-b", .vfo = RIG_VFO_NONE,
          .requested = 1 },
        { .name = "split-off-accepted-preserves-b", .vfo = RIG_VFO_NONE,
          .initial = 1 },
        { .name = "split-on-unchanged", .vfo = RIG_VFO_NONE,
          .initial = 1, .requested = 1 },
        { .name = "split-unchanged-match-then-rollback", .vfo = RIG_VFO_NONE,
          .initial = 1, .requested = 1, .behavior = SETTER_UNCHANGED_ROLLBACK,
          .expected_retval = -RIG_ERJCTED },
        { .name = "split-off-unchanged", .vfo = RIG_VFO_NONE },
        { .name = "split-stale-requested-cache", .vfo = RIG_VFO_NONE,
          .requested = 1, .stale_cache = 1 },
        { .name = "split-delayed-acceptance", .vfo = RIG_VFO_NONE,
          .requested = 1, .behavior = SETTER_DELAY, .readback_delay_ms = 60 },
        { .name = "split-readback-match-rollback", .vfo = RIG_VFO_NONE,
          .requested = 1, .behavior = SETTER_READBACK_ROLLBACK,
          .readback_delay_ms = 60, .expected_retval = -RIG_ERJCTED },
        { .name = "split-second-match-rollback-before-barrier", .vfo = RIG_VFO_NONE,
          .requested = 1, .behavior = SETTER_BARRIER_ROLLBACK,
          .expected_retval = -RIG_ERJCTED },
        { .name = "split-explicit-er23-text", .vfo = RIG_VFO_NONE,
          .requested = 1, .verify_error = "ER23: SPLIT unavailable;",
          .expected_retval = -RIG_ERJCTED },
        { .name = "split-queued-er23-during-confirmation", .vfo = RIG_VFO_NONE,
          .requested = 1, .queued_error = "ER23: SPLIT unavailable;",
          .expected_retval = -RIG_ERJCTED },
        { .name = "split-readback-timeout", .vfo = RIG_VFO_NONE,
          .requested = 1, .readback_timeout = 1, .expected_retval = -RIG_ETIMEOUT },
        { .name = "split-malformed-readback", .vfo = RIG_VFO_NONE,
          .requested = 1, .readback_error = "FTX;", .expected_retval = -RIG_EPROTO }
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_setter_case(&cases[i]) != 0) { return 1; }
    }

    return 0;
}

int main(void)
{
    int failed = 0;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

#endif

    rig_register(&k4_caps);

    if (run_case("receive-vfo-a", "FR0;", "FT0;", "TQ0;", RIG_OK,
                 RIG_VFO_A, RIG_VFO_MAIN, RIG_VFO_A) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("receive-vfo-b", "FR1;", "FT0;", "TQ0;", RIG_OK,
                 RIG_VFO_B, RIG_VFO_B, RIG_VFO_B) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("transmit-split-off", "FR1;", "FT0;", "TQ1;", RIG_OK,
                 RIG_VFO_A, RIG_VFO_MAIN, RIG_VFO_A) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("transmit-split-on", "FR0;", "FT1;", "TQ1;", RIG_OK,
                 RIG_VFO_B, RIG_VFO_MAIN, RIG_VFO_B) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("malformed-fr", "FRX;", NULL, NULL, -RIG_EPROTO,
                 RIG_VFO_NONE, RIG_VFO_NONE, RIG_VFO_NONE) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("malformed-ft", "FR0;", "FTX;", NULL, -RIG_EPROTO,
                 RIG_VFO_NONE, RIG_VFO_NONE, RIG_VFO_NONE) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("malformed-tq", "FR0;", "FT0;", "TQX;", -RIG_EPROTO,
                 RIG_VFO_NONE, RIG_VFO_NONE, RIG_VFO_NONE) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("invalid-fr-value", "FR9;", NULL, NULL, -RIG_EPROTO,
                 RIG_VFO_NONE, RIG_VFO_NONE, RIG_VFO_NONE) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_case("invalid-ft-value", "FR0;", "FT2;", NULL, -RIG_EPROTO,
                 RIG_VFO_NONE, RIG_VFO_NONE, RIG_VFO_NONE) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    failed = run_case("invalid-tq-value", "FR0;", "FT0;", "TQ7;",
                      -RIG_EPROTO, RIG_VFO_NONE, RIG_VFO_NONE,
                      RIG_VFO_NONE);

    if (!failed) { failed = run_setter_cases(); }

cleanup:
#ifdef _WIN32
    WSACleanup();
#endif
    return failed;
}
