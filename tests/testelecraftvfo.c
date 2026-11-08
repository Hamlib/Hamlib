/*
 * Hamlib Elecraft K4 VFO and mode response tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <pthread.h>
#include <math.h>
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
#include "../rigs/kenwood/kenwood.h"

extern struct rig_caps k4_caps;
extern struct rig_caps k3_caps;

struct peer_case
{
    int fd;
    const char *const *commands;
    const char *const *responses;
    size_t count;
    int status;
    const int *delay_ms;
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
    struct peer_case *test = arg;
    char command[32];
    size_t i;

    test->status = -1;

    for (i = 0; i < test->count; i++)
    {
        if (read_command(test->fd, command, sizeof(command)) != 0
                || strcmp(command, test->commands[i]) != 0)
        {
            return NULL;
        }

        if (test->delay_ms) { hl_usleep(test->delay_ms[i] * 1000); }

        if (test->responses[i] != NULL
                && write_all(test->fd, test->responses[i],
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
    static const char *commands[] = { "FR;", "ID;", "FT;", "ID;", "TQ;", "ID;" };
    const char *responses[] = { fr, "ID017;", ft, "ID017;", tq, "ID017;" };
    const vfo_t initial_vfo = RIG_VFO_MEM;
    const vfo_t initial_rx_vfo = RIG_VFO_MAIN;
    const vfo_t initial_tx_vfo = RIG_VFO_SUB;
    int sockets[2];
    pthread_t thread;
    struct peer_case test =
    {
        .fd = -1,
        .commands = commands,
        .responses = responses,
        .count = tq != NULL ? 6 : ft != NULL ? 4 : 2,
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

static int run_mode_get_case(const char *name, int split, int is_k4,
                             const char *md_response,
                             const char *dt_response,
                             rmode_t expected_mode)
{
    static const char *k4_commands[] = { "MD$;", "ID;", "DT$;", "ID;", "BW$;", "ID;" };
    static const char *k3_commands[] = { "MD$;", "DT;", "BW$;" };
    const char *responses[] = { md_response, dt_response, "BW$0482;" };
    const char *k4_responses[] =
    { md_response, "ID017;", dt_response, "ID017;", "BW$0482;", "ID017;" };
    struct peer_case test =
    {
        .fd = -1,
        .commands = is_k4 ? k4_commands : k3_commands,
        .responses = is_k4 ? k4_responses : responses,
        .count = is_k4 ? 6 : 3,
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    RIG *rig;
    struct kenwood_priv_data *priv;
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = 0;
    int retval;

    if (open_test_connection(sockets) != 0) { return 1; }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    rig = rig_init(is_k4 ? RIG_MODEL_K4 : RIG_MODEL_K3);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    priv = STATE(rig)->priv;
    priv->is_k4 = is_k4;
    priv->is_k3 = !is_k4;
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;

    if (split)
    {
        retval = rig->caps->get_split_mode(rig, RIG_VFO_B, &mode, &width);
    }
    else
    {
        retval = rig->caps->get_mode(rig, RIG_VFO_B, &mode, &width);
    }

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (test.status != 0 || retval != RIG_OK || mode != expected_mode
            || width != 4820)
    {
        fprintf(stderr,
                "%s: expected status/retval/mode/width 0/%d/%llu/4820, "
                "got %d/%d/%llu/%ld\n",
                name, RIG_OK, (unsigned long long)expected_mode, test.status,
                retval, (unsigned long long)mode, (long)width);
        return 1;
    }

    return 0;
}

static int run_split_mode_set_case(int is_k4, rmode_t mode)
{
    static const char *k4_packet_commands[] =
    {
        "DT0;", "DT$0;", "ID;", "MD$6;", "ID;", "FT1;", "ID;"
    };
    static const char *k4_rttyr_commands[] =
    {
        "DT2;", "DT$2;", "ID;", "MD$9;", "ID;", "FT1;", "ID;"
    };
    static const char *k4_responses[] =
    {
        NULL, NULL, "ID017;", NULL, "ID017;", NULL, "ID017;"
    };
    static const char *k3_packet_commands[] = { "MD$6;", "ID;" };
    static const char *k3_rttyr_commands[] = { "MD$9;", "ID;" };
    static const char *k3_responses[] = { NULL, "ID017;" };
    const char *const *commands;
    const char *const *responses;
    size_t count;

    if (is_k4)
    {
        commands = mode == RIG_MODE_RTTYR ? k4_rttyr_commands
                   : k4_packet_commands;
        responses = k4_responses;
        count = 7;
    }
    else
    {
        commands = mode == RIG_MODE_RTTYR ? k3_rttyr_commands
                   : k3_packet_commands;
        responses = k3_responses;
        count = 2;
    }

    struct peer_case test =
    {
        .fd = -1,
        .commands = commands,
        .responses = responses,
        .count = count,
        .status = -1
    };

    int sockets[2];

    pthread_t thread;

    RIG *rig;

    struct kenwood_priv_data *priv;

    int retval;

    if (open_test_connection(sockets) != 0) { return 1; }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    rig = rig_init(is_k4 ? RIG_MODEL_K4 : RIG_MODEL_K3);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    priv = STATE(rig)->priv;
    priv->is_k4 = is_k4;
    priv->is_k3 = !is_k4;
    priv->split = 1;
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;

    retval = rig->caps->set_split_mode(rig, RIG_VFO_B, mode,
                                       RIG_PASSBAND_NOCHANGE);

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (test.status != 0 || retval != RIG_OK)
    {
        fprintf(stderr,
                "%s-%s-split-mode-set: expected status/retval 0/%d, "
                "got %d/%d\n", is_k4 ? "k4" : "k3", rig_strrmode(mode),
                RIG_OK, test.status, retval);
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
    freq_t wire_requested;
    double vfo_comp;
    freq_t lo_freq;
    freq_t retained_api;
    vfo_t current_vfo;
    vfo_t split_tx_vfo;
    enum setter_behavior behavior;
    freq_t rollback;
    const char *verify_error;
    const char *readback_error;
    const char *queued_error;
    int verify_timeout;
    int readback_timeout;
    int partial_readback;
    int readback_delay_ms;
    int trailing_id_delay_ms;
    int queued_notifications;
    int post_marker_notification;
    int retry_zero;
    int getter_after_error;
    int getter_expected_retval;
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

static void finish_test_writes(int fd)
{
#ifdef _WIN32
    shutdown(fd, SD_SEND);
#else
    shutdown(fd, SHUT_WR);
#endif
}

static freq_t setter_wire_value(const struct setter_case *test)
{
    return test->wire_requested ? test->wire_requested : test->requested;
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
                 (long long)setter_wire_value(test));
    }

    if (strcmp(command, setter) == 0)
    {
        peer->setters++;

        if (test->behavior == SETTER_NOTIFICATION_ROLLBACK)
        {
            return setter_reply(peer, setter_wire_value(test));
        }

        return 0;
    }

    if (strcmp(command, "ID;") == 0)
    {
        peer->id_queries++;

        if (peer->id_queries == 1)
        {
            int i;

            for (i = 0; i < test->queued_notifications; i++)
            {
                if (write_all(peer->fd, "FB00014074000;", 14) != 0) { return -1; }
            }
        }

        if (peer->setters != 0)
        {
            peer->verification_queries++;

            if (test->verify_timeout) { return 0; }

            if (test->readback_timeout && peer->readbacks != 0) { return 0; }

            if (test->retry_id && peer->verification_queries == 1) { return 0; }

            if (test->verify_error != NULL)
            {
                if (write_all(peer->fd, test->verify_error,
                              strlen(test->verify_error)) != 0) { return -1; }
            }
        }

        if (test->queued_error != NULL && peer->readbacks == 1
                && test->trailing_id_delay_ms)
        {
            hl_usleep(test->trailing_id_delay_ms * 1000);
        }

        if (write_all(peer->fd, "ID017;", 6) != 0) { return -1; }

        return test->post_marker_notification
               ? setter_reply(peer, setter_wire_value(test)) : 0;
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

            if (test->readback_timeout)
            {
                return test->partial_readback
                       ? write_all(peer->fd, "FA000", 5) : 0;
            }

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
                value = setter_wire_value(test);
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
    vfo_t initial_current = test->current_vfo ? test->current_vfo : RIG_VFO_A;
    int expected_setters = test->expected_retval == -RIG_EINVAL
                           || test->initial == setter_wire_value(test) ? 0 : 1;
    freq_t expected_actual = test->behavior == SETTER_UNCHANGED_ROLLBACK
                             ? test->rollback : test->initial;
    freq_t expected_api = test->retained_api ? test->retained_api : expected_actual;

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
    RIGPORT(rig)->retry = test->retry_zero ? 0 : 1;
    STATE(rig)->comm_state = 1;
    STATE(rig)->twiddle_timeout = 0;
    STATE(rig)->vfo_comp = test->vfo_comp;
    STATE(rig)->lo_freq = test->lo_freq;
    rig_set_vfo_state(rig, initial_current,
                      initial_current == RIG_VFO_A ? RIG_VFO_B : RIG_VFO_A);
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
                           test->stale_cache ? test->requested
                           : test->retained_api ? test->retained_api : test->initial);
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
                                   test->split_tx_vfo ? test->split_tx_vfo
                                   : test->requested ? RIG_VFO_B : RIG_VFO_A);
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

        if ((retval == RIG_OK && !test->vfo_comp && cached != test->requested)
                || (retval != RIG_OK && cache_age < cache_timeout))
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

            if (read_retval != RIG_OK || fabs(observed - expected_api) > 0.01)
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

    if (routing.current_vfo != initial_current)
    {
        fprintf(stderr, "%s: current VFO changed to %d\n", test->name,
                routing.current_vfo);
        failed = 1;
    }

    if (test->getter_after_error)
    {
        freq_t observed = 0;
        int read_retval;
        rig_set_cache_freq(rig, test->vfo, 0);
        read_retval = rig_get_freq(rig, test->vfo, &observed);

        if (read_retval != test->getter_expected_retval
                || (read_retval == RIG_OK && observed != test->initial))
        {
            fprintf(stderr, "%s: getter after error returned %d/%.0f\n",
                    test->name, read_retval, observed);
            failed = 1;
        }
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    finish_test_writes(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);

    if (retval != test->expected_retval || peer.status != 0
            || peer.setters != expected_setters
            || (expected_setters && peer.id_queries == 0)
            || ((test->behavior == SETTER_DELAY
                 || test->behavior == SETTER_READBACK_ROLLBACK)
                && peer.readbacks < 3)
            || (test->retry_id && retval == RIG_OK))
    {
        fprintf(stderr, "%s: expected result/setters %d/%d, got %d/%d "
                "(ID=%d, readbacks=%d, peer=%d)\n", test->name,
                test->expected_retval, expected_setters, retval, peer.setters,
                peer.id_queries, peer.readbacks, peer.status);
        failed = 1;
    }

    return failed;
}

enum recovery_reader
{
    RECOVERY_DEDICATED,
    RECOVERY_GENERIC,
    RECOVERY_RAW
};

struct recovery_peer
{
    int fd;
    enum recovery_reader reader;
    int generic_setter;
    const char *setter_rejection;
    int partial;
    int released;
    int status;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
};

static int expect_peer_command(int fd, const char *expected)
{
    char command[32];
    return read_command(fd, command, sizeof(command)) == 0
           && strcmp(command, expected) == 0 ? 0 : -1;
}

static void *run_recovery_peer(void *arg)
{
    struct recovery_peer *peer = arg;

    peer->status = 1;

    if (peer->generic_setter)
    {
        if (expect_peer_command(peer->fd, "MD1;") != 0) { return NULL; }
    }
    else
    {
        if (expect_peer_command(peer->fd, "ID;") != 0
                || write_all(peer->fd, "ID017;", 6) != 0
                || expect_peer_command(peer->fd, "FA;") != 0) { return NULL; }
    }

    if (expect_peer_command(peer->fd, "ID;") != 0) { return NULL; }

    if (peer->setter_rejection
            && (write_all(peer->fd, peer->setter_rejection,
                          strlen(peer->setter_rejection)) != 0
                || write_all(peer->fd, "ID017;", 6) != 0)) { return NULL; }

    if (peer->partial && write_all(peer->fd, "FA000", 5) != 0) { return NULL; }

    pthread_mutex_lock(&peer->mutex);

    while (!peer->released) { pthread_cond_wait(&peer->condition, &peer->mutex); }

    pthread_mutex_unlock(&peer->mutex);

    /* The abandoned response must not satisfy the next operation. */
    const char *abandoned = peer->partial ? "28075000;ID017;"
                            : "FA00028075000;ID017;";

    if (!peer->setter_rejection
            && write_all(peer->fd, abandoned, strlen(abandoned)) != 0) { return NULL; }

    if (peer->reader == RECOVERY_DEDICATED)
    {
        if (expect_peer_command(peer->fd, "ID;") != 0
                || write_all(peer->fd, "FA00028075000;ID017;", 20) != 0)
        { return NULL; }
    }

    if (expect_peer_command(peer->fd, "FA;") != 0) { return NULL; }

    if (peer->reader != RECOVERY_RAW
            && expect_peer_command(peer->fd, "ID;") != 0)
    { return NULL; }

    const char *fresh = peer->reader != RECOVERY_RAW
                        ? "FA00028076000;ID017;" : "FA00028076000;";

    if (write_all(peer->fd, fresh, strlen(fresh)) == 0) { peer->status = 0; }

    return NULL;
}

static int run_recovery_case(enum recovery_reader reader, int partial,
                             int generic_setter, const char *setter_rejection)
{
    int sockets[2];
    pthread_t thread;
    RIG *rig;
    char reply[64] = {0};
    int first, second;
    struct recovery_peer peer =
    {
        .reader = reader, .partial = partial,
        .generic_setter = generic_setter, .setter_rejection = setter_rejection,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .condition = PTHREAD_COND_INITIALIZER
    };

    if (open_test_connection(sockets) != 0) { return 1; }

    rig = rig_init(RIG_MODEL_K4);

    if (!rig || bound_peer_reads(sockets[1]) != 0)
    {
        if (rig) { rig_cleanup(rig); }

        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    peer.fd = sockets[1];
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->rig = rig;
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 160;

    if (pthread_create(&thread, NULL, run_recovery_peer, &peer) != 0)
    {
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    first = generic_setter ? kenwood_transaction(rig, "MD1", NULL, 0)
            : kenwood_transaction_k4_read(rig, "FA", reply, sizeof(reply));
    pthread_mutex_lock(&peer.mutex);
    peer.released = 1;
    pthread_cond_signal(&peer.condition);
    pthread_mutex_unlock(&peer.mutex);
    RIGPORT(rig)->timeout = 1000;
    if (reader == RECOVERY_RAW)
    {
        second = rig_send_raw(rig, (const unsigned char *)"FA;", 3,
                              (unsigned char *)reply, sizeof(reply),
                              (unsigned char *)";");
    }
    else
    {
        second = reader == RECOVERY_GENERIC
                 ? kenwood_transaction(rig, "FA", reply, sizeof(reply))
                 : kenwood_transaction_k4_read(rig, "FA", reply, sizeof(reply));
    }
    finish_test_writes(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    pthread_cond_destroy(&peer.condition);
    pthread_mutex_destroy(&peer.mutex);

    int expected_second = reader == RECOVERY_RAW ? 14 : RIG_OK;
    const char *expected_reply = reader == RECOVERY_RAW ? "FA00028076000;"
                                 : "FA00028076000";

    int expected_first = setter_rejection ? -RIG_ERJCTED : -RIG_ETIMEOUT;

    if (first != expected_first || second != expected_second
            || strcmp(reply, expected_reply) != 0 || peer.status != 0)
    {
        fprintf(stderr, "recovery getter=%d setter=%d rejection=%s partial=%d: "
                "results %d/%d, reply=%s, peer=%d\n", reader, generic_setter,
                setter_rejection ? setter_rejection : "none", partial,
                first, second, reply, peer.status);
        return 1;
    }

    return 0;
}

static int run_failed_write_reset_case(void)
{
    static const char *commands[] = { "ID;", "FA;", "ID;", "AI;", "FA;", "ID;" };
    static const char *responses[] =
    { "ID017;", "FA00028076000;", "ID017;", "AI0;", "FA00028076000;", "ID017;" };
    struct peer_case peer = { .commands = commands, .responses = responses,
                              .count = 6, .status = -1 };
    RIG *rig = rig_init(RIG_MODEL_K4);
    struct kenwood_priv_data *priv;
    pthread_t thread;
    int sockets[2];
    int failed = 0;
    char reply[64] = {0};

    if (!rig) { return 1; }

    priv = STATE(rig)->priv;
    RIGPORT(rig)->fd = -1;
    RIGPORT(rig)->rig = rig;
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 160;

    if (kenwood_transaction_k4_set_once(rig, "FA00028075000") == RIG_OK
            || !priv->k4_stream_unusable) { failed = 1; }

    RIGPORT(rig)->type.rig = RIG_PORT_SERIAL;
    kenwood_k4_reset(rig);

    if (!priv->k4_stream_unusable
            || kenwood_transaction(rig, "FA", reply, sizeof(reply)) != -RIG_EIO)
    { failed = 1; }

    if (open_test_connection(sockets) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    peer.fd = sockets[1];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->timeout = 1000;

    if (rig_send_raw(rig, (const unsigned char *)"FA;", 3, NULL, 0, NULL)
            != -RIG_EIO) { failed = 1; }

    kenwood_k4_reset(rig);

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        return 1;
    }

    if (kenwood_transaction_k4_read(rig, "FA", reply, sizeof(reply)) != RIG_OK
            || strcmp(reply, "FA00028076000") != 0) { failed = 1; }

    STATE(rig)->comm_state = 1;

    if (rig_send_raw(rig, (const unsigned char *)"AI;", 3,
                     (unsigned char *)reply, sizeof(reply),
                     (unsigned char *)";") != 4
            || memcmp(reply, "AI0;", 4) != 0) { failed = 1; }

    if (kenwood_transaction(rig, "FA", reply, sizeof(reply)) != RIG_OK
            || strcmp(reply, "FA00028076000") != 0) { failed = 1; }

    STATE(rig)->comm_state = 0;

    finish_test_writes(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);

    if (failed || peer.status != 0)
    {
        fprintf(stderr, "failed-write reset: failed=%d, peer=%d\n", failed, peer.status);
        return 1;
    }

    return 0;
}

struct query_failure_case
{
    const char *name;
    const char *reply;
    int partial;
    int backlog;
    int missing_marker;
    int transmit;
    int expected;
};

struct query_failure_peer
{
    int fd;
    const struct query_failure_case *test;
    int queries;
    int markers;
    int rx;
    int tx;
    int setters;
    int failed;
    int released;
    freq_t frequency;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
};

static int query_failure_command(struct query_failure_peer *peer, const char *command)
{
    const struct query_failure_case *test = peer->test;

    if (strcmp(command, "TQ;") == 0)
    {
        peer->queries++;
        if (peer->queries > 1) { return write_all(peer->fd, "TQ0;", 4); }
        if (test->partial) { return write_all(peer->fd, "TQ", 2); }
        return test->reply ? write_all(peer->fd, test->reply, strlen(test->reply)) : 0;
    }

    if (strcmp(command, "ID;") == 0)
    {
        peer->markers++;
        if (peer->queries == 1)
        {
            for (int i = 0; i < test->backlog; i++)
            {
                if (write_all(peer->fd, "FB00014074000;", 14) != 0) { return -1; }
            }

            if (!test->reply && !test->backlog)
            {
                pthread_mutex_lock(&peer->mutex);
                while (!peer->released)
                { pthread_cond_wait(&peer->condition, &peer->mutex); }
                pthread_mutex_unlock(&peer->mutex);

                if (test->missing_marker) { return 0; }
                const char *stale = test->partial ? "1;" : "TQ1;";
                if (write_all(peer->fd, stale, strlen(stale)) != 0) { return -1; }
            }
        }
        return write_all(peer->fd, "ID017;", 6);
    }

    if (strcmp(command, "TX;") == 0) { peer->tx++; return 0; }
    if (strcmp(command, "RX;") == 0) { peer->rx++; return 0; }

    if (strcmp(command, "FA;") == 0)
    {
        char reply[32];
        snprintf(reply, sizeof(reply), "FA%011lld;", (long long)peer->frequency);
        return write_all(peer->fd, reply, strlen(reply));
    }

    if (strcmp(command, "FA00007075000;") == 0)
    { peer->setters++; peer->frequency = 7075000; return 0; }

    return -1;
}

static void *run_query_failure_peer(void *arg)
{
    struct query_failure_peer *peer = arg;
    char command[64];

    while (read_command(peer->fd, command, sizeof(command)) == 0)
    {
        if (query_failure_command(peer, command) != 0)
        {
            fprintf(stderr, "%s: unexpected query command %s\n", peer->test->name, command);
            peer->failed = 1;
            shutdown_test_socket(peer->fd);
            break;
        }
    }
    return NULL;
}

static int run_query_failure_case(const struct query_failure_case *test, int serial)
{
    int sockets[2];
    pthread_t thread;
    int failed = 0;
    ptt_t ptt = RIG_PTT_ON;
    struct query_failure_peer peer =
    {
        .test = test, .frequency = 7074000,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .condition = PTHREAD_COND_INITIALIZER
    };
    RIG *rig = rig_init(RIG_MODEL_K4);

    if (!rig) { return 1; }
    if (open_test_connection(sockets) != 0) { rig_cleanup(rig); return 1; }
    peer.fd = sockets[1];
    bound_peer_reads(peer.fd);
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = serial ? RIG_PORT_SERIAL : RIG_PORT_NETWORK;
    /* The backlog case must reach the frame limit before its time limit. */
    RIGPORT(rig)->timeout = test->backlog ? 3000 : 160;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->comm_state = 1;
    STATE(rig)->twiddle_timeout = 0;
    rig_set_vfo_state(rig, RIG_VFO_A, RIG_VFO_B);
    rig_set_split_routing_state(rig, RIG_SPLIT_OFF, RIG_VFO_A, RIG_VFO_A);
    rig_invalidate_cache(rig);

    if (pthread_create(&thread, NULL, run_query_failure_peer, &peer) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        return 1;
    }

    int first = test->transmit ? rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_ON)
                : rig_get_ptt(rig, RIG_VFO_A, &ptt);
    struct kenwood_priv_data *priv = STATE(rig)->priv;
    if (first != test->expected || priv->k4_stream_unusable) { failed = 1; }

    pthread_mutex_lock(&peer.mutex);
    peer.released = 1;
    pthread_cond_signal(&peer.condition);
    pthread_mutex_unlock(&peer.mutex);

    if (!test->missing_marker) { RIGPORT(rig)->timeout = 1000; }
    int second = rig_get_ptt(rig, RIG_VFO_A, &ptt);
    int set_frequency = RIG_OK;
    int stop_transmit = RIG_OK;
    if (test->missing_marker)
    {
        if (second != -RIG_ETIMEOUT || !priv->k4_marker_pending) { failed = 1; }
    }
    else
    {
        if (second != RIG_OK || ptt != RIG_PTT_OFF || priv->k4_marker_pending)
        { failed = 1; }
        RIGPORT(rig)->timeout = 1000;
        set_frequency = rig_set_freq(rig, RIG_VFO_A, 7075000);
        if (test->transmit) { stop_transmit = rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_OFF); }
        if (set_frequency != RIG_OK || stop_transmit != RIG_OK) { failed = 1; }
    }

    finish_test_writes(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    RIGPORT(rig)->fd = -1;
    STATE(rig)->comm_state = 0;
    rig_cleanup(rig);
    pthread_cond_destroy(&peer.condition);
    pthread_mutex_destroy(&peer.mutex);

    if (test->missing_marker && (peer.queries != 1 || peer.markers != 1)) { failed = 1; }
    if (!test->missing_marker && peer.setters != 1) { failed = 1; }
    if (peer.tx != test->transmit || peer.rx != test->transmit) { failed = 1; }
    if (failed || peer.failed)
    {
        fprintf(stderr, "%s serial=%d: results %d/%d/%d/%d, queries=%d markers=%d "
                "TX/RX=%d/%d setters=%d peer=%d\n", test->name, serial,
                first, second, set_frequency, stop_transmit, peer.queries,
                peer.markers, peer.tx, peer.rx, peer.setters, peer.failed);
        return 1;
    }
    return 0;
}

static int run_query_contract_case(int power_retry)
{
    static const char *commands[] =
    {
        "ID;", "ID;", "KY;", "ID;", "MD$;", "ID;", "BW$;", "ID;",
        "RVM;", "ID;", "MD;", "ID;", "ER;", "ID;", "ER;", "ID;", "IF;", "ID;"
    };
    static const char *responses[] =
    {
        "ID017;", "ID017;", "KY0;", "ID017;", "MD$6;MD1;", "ID017;",
        "BW$0482;BW0240;", "ID017;", "RVM04.00;RVA01.00;", "ID017;",
        "MD2;MD$6;", "ID017;", "ER0;", "ID017;", "ER1;", "ID017;",
        "IF00007074000;", "ID017;"
    };
    static const char *power_commands[] = { "PS;", "ID;", "PS;", "ID;" };
    static const char *power_responses[] = { NULL, "PS0;ID017;", "PS1;", "ID017;" };
    static const int power_delays[] = { 0, 650, 0, 0 };
    static const struct
    {
        const char *command;
        size_t capacity;
        const char *expected;
    } calls[] =
    {
        { "ID", 64, "ID017" }, { "ID;", 64, "ID017" },
        { "KY;", 4, "KY0" }, { "MD$", 64, "MD$6" },
        { "BW$\r", 64, "BW$0482" }, { "RVM", 4, "RVM" },
        { "MD", 64, "MD2" }, { "ER", 64, "ER0" }, { "ER", 64, "ER1" },
        { "IF", 4, "IF0" }, { "IF;", 64, "IF00007074000" }
    };
    struct peer_case peer =
    {
        .commands = power_retry ? power_commands : commands,
        .responses = power_retry ? power_responses : responses,
        .count = power_retry ? 4 : sizeof(commands) / sizeof(commands[0]),
        .delay_ms = power_retry ? power_delays : NULL,
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    int failed = 0;
    char reply[64];
    RIG *rig = rig_init(RIG_MODEL_K4);

    if (!rig) { return 1; }
    if (open_test_connection(sockets) != 0) { rig_cleanup(rig); return 1; }
    peer.fd = sockets[1];
    bound_peer_reads(peer.fd);
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 1000;
    RIGPORT(rig)->retry = 0;
    struct kenwood_priv_data *priv = STATE(rig)->priv;

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        return 1;
    }

    if (power_retry)
    {
        powerstat_t power = RIG_POWER_OFF;
        priv->has_ps = 1;
        if (kenwood_get_powerstat(rig, &power) != RIG_OK || power != RIG_POWER_ON)
        { failed = 1; }
    }
    else
    {
        const char *invalid[] = { NULL, "", "FA;FB", "FA;\r" };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
        {
            if (kenwood_transaction(rig, invalid[i], reply, sizeof(reply)) != -RIG_EINVAL
                    || priv->k4_stream_unusable || priv->k4_marker_pending)
            { failed = 1; }
        }
        if (kenwood_transaction(rig, "FA", NULL, sizeof(reply)) != -RIG_EINVAL
                || priv->k4_stream_unusable || priv->k4_marker_pending)
        { failed = 1; }
        for (size_t i = 0; i < sizeof(calls) / sizeof(calls[0]); i++)
        {
            memset(reply, 'X', sizeof(reply));
            int retval = kenwood_transaction(rig, calls[i].command, reply, calls[i].capacity);
            if (retval != RIG_OK || strncmp(reply, calls[i].expected, calls[i].capacity) != 0
                    || reply[strlen(calls[i].expected)] != '\0')
            {
                fprintf(stderr, "query contract %s: retval=%d reply=%.63s\n",
                        calls[i].command, retval, reply);
                failed = 1;
            }
        }
    }

    finish_test_writes(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    if (failed || peer.status != 0)
    {
        fprintf(stderr, "query contracts power=%d: failed=%d peer=%d\n",
                power_retry, failed, peer.status);
        return 1;
    }
    return 0;
}

static int run_query_cases(void)
{
    static const struct query_failure_case cases[] =
    {
        { .name = "query-timeout", .expected = -RIG_ETIMEOUT },
        { .name = "query-partial-timeout", .partial = 1, .expected = -RIG_ETIMEOUT },
        { .name = "query-rejected", .reply = "ER04;", .expected = -RIG_ERJCTED },
        { .name = "query-error-before-reply", .reply = "ER04;TQ1;", .expected = -RIG_ERJCTED },
        { .name = "query-error-after-reply", .reply = "TQ1;ER04;", .expected = -RIG_ERJCTED },
        { .name = "query-question", .reply = "?;", .expected = -RIG_ERJCTED },
        { .name = "query-no-matching-reply", .reply = "FT0;", .expected = -RIG_EPROTO },
        { .name = "query-frame-budget", .backlog = 200, .expected = -RIG_EPROTO },
        { .name = "query-missing-marker", .missing_marker = 1, .expected = -RIG_ETIMEOUT },
        { .name = "query-after-simulated-tx", .transmit = 1, .expected = -RIG_ETIMEOUT }
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_query_failure_case(&cases[i], 0) != 0) { return 1; }
#ifndef _WIN32
        /* Windows serial I/O requires a registered serial handle, not a socket. */
        if (run_query_failure_case(&cases[i], 1) != 0) { return 1; }
#endif
    }
    return run_query_contract_case(0) || run_query_contract_case(1);
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
        { .name = "frequency-fractional-accepted", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000.5,
          .wire_requested = 28075000 },
        { .name = "frequency-compensated-accepted", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .wire_requested = 28075028, .vfo_comp = 0.000001 },
        { .name = "frequency-compensated-rejected", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .wire_requested = 28075028, .vfo_comp = 0.000001,
          .retained_api = 28073971.926028077, .behavior = SETTER_REJECT,
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-transverter-rejected", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 144075000,
          .wire_requested = 28075000, .lo_freq = 116000000,
          .retained_api = 144074000, .behavior = SETTER_REJECT,
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-queued-reports", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .queued_notifications = 7 },
        { .name = "frequency-queued-reports-without-retries", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .queued_notifications = 7, .retry_zero = 1 },
        { .name = "frequency-stale-notification-after-marker", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .behavior = SETTER_REJECT, .post_marker_notification = 1,
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-fractional-unchanged", .vfo = RIG_VFO_A,
          .initial = 28075000, .requested = 28075000.5,
          .wire_requested = 28075000 },
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
          .initial = 28074000, .requested = 28075000, .retry_id = 1,
          .getter_after_error = 1, .getter_expected_retval = -RIG_ETIMEOUT,
          .expected_retval = -RIG_ETIMEOUT },
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
          .trailing_id_delay_ms = 100,
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-getter-after-queued-error", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .behavior = SETTER_REJECT, .queued_error = "ER04;",
          .trailing_id_delay_ms = 100, .getter_after_error = 1,
          .expected_retval = -RIG_ERJCTED },
        { .name = "frequency-readback-timeout", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000, .readback_timeout = 1,
          .expected_retval = -RIG_ETIMEOUT },
        { .name = "frequency-partial-readback-timeout", .vfo = RIG_VFO_A,
          .initial = 28074000, .requested = 28075000,
          .readback_timeout = 1, .partial_readback = 1,
          .getter_after_error = 1, .getter_expected_retval = -RIG_ETIMEOUT,
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
        { .name = "split-on-other-current-a", .vfo = RIG_VFO_NONE,
          .requested = 1, .split_tx_vfo = RIG_VFO_OTHER },
        { .name = "split-on-other-current-b-rejected", .vfo = RIG_VFO_NONE,
          .requested = 1, .split_tx_vfo = RIG_VFO_OTHER,
          .current_vfo = RIG_VFO_B, .expected_retval = -RIG_EINVAL },
        { .name = "split-off-accepted-preserves-b", .vfo = RIG_VFO_NONE,
          .initial = 1 },
        { .name = "split-off-tx-b-current-a", .vfo = RIG_VFO_NONE,
          .initial = 1, .split_tx_vfo = RIG_VFO_B },
        { .name = "split-off-tx-sub-current-a", .vfo = RIG_VFO_NONE,
          .initial = 1, .split_tx_vfo = RIG_VFO_SUB },
        { .name = "split-off-tx-b-current-b", .vfo = RIG_VFO_NONE,
          .initial = 1, .split_tx_vfo = RIG_VFO_B, .current_vfo = RIG_VFO_B },
        { .name = "split-off-tx-sub-current-b", .vfo = RIG_VFO_NONE,
          .initial = 1, .split_tx_vfo = RIG_VFO_SUB, .current_vfo = RIG_VFO_B },
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

    for (enum recovery_reader reader = RECOVERY_DEDICATED;
            reader <= RECOVERY_RAW; reader++)
    {
        for (int partial = 0; partial < 2; partial++)
        {
            if (run_recovery_case(reader, partial, 0, NULL) != 0) { return 1; }
        }
    }

    for (enum recovery_reader reader = RECOVERY_GENERIC;
            reader <= RECOVERY_RAW; reader++)
    {
        if (run_recovery_case(reader, 1, 1, NULL) != 0
                || run_recovery_case(reader, 0, 1, "ER04;") != 0
                || run_recovery_case(reader, 0, 1, "?;") != 0) { return 1; }
    }

    return run_failed_write_reset_case();
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
    rig_register(&k3_caps);

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

    if (run_case("invalid-tq-value", "FR0;", "FT0;", "TQ7;",
                 -RIG_EPROTO, RIG_VFO_NONE, RIG_VFO_NONE,
                 RIG_VFO_NONE) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("mode-b-data-offset", 0, 1, "MD$6;", "DT$1;",
                          RIG_MODE_PKTLSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("mode-b-reverse-data", 0, 1, "MD$9;", "DT$0;",
                          RIG_MODE_PKTLSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("mode-b-reverse-afsk", 0, 1, "MD$9;", "DT$1;",
                          RIG_MODE_PKTUSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("split-mode-data-vfo", 1, 1, "MD$6;", "DT$1;",
                          RIG_MODE_PKTLSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("split-mode-rttyr", 1, 1, "MD$9;", "DT$2;",
                          RIG_MODE_RTTYR) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("k3-mode-b-reverse-data", 0, 0, "MD$9;", "DT0;",
                          RIG_MODE_PKTUSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("k3-mode-b-reverse-afsk", 0, 0, "MD$9;", "DT1;",
                          RIG_MODE_PKTLSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("k3-split-shared-data", 1, 0, "MD$6;", "DT1;",
                          RIG_MODE_PKTLSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_split_mode_set_case(0, RIG_MODE_PKTUSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_split_mode_set_case(0, RIG_MODE_RTTYR) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_split_mode_set_case(1, RIG_MODE_PKTUSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    failed = run_split_mode_set_case(1, RIG_MODE_RTTYR);

    if (!failed) { failed = run_setter_cases(); }
    if (!failed) { failed = run_query_cases(); }

cleanup:
#ifdef _WIN32
    WSACleanup();
#endif
    return failed;
}
