/*
 * Hamlib Elecraft response tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <math.h>
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
#include "../rigs/kenwood/kenwood.h"

extern struct rig_caps k4_caps;
extern struct rig_caps k3_caps;
extern struct rig_caps k3s_caps;

struct peer_case
{
    int fd;
    const char *const *commands;
    const char *const *responses;
    size_t count;
    int status;
    const int *delay_ms;
};

static void set_split_routing(RIG *rig, split_t split, vfo_t tx_vfo)
{
    STATE(rig)->rx_vfo = RIG_VFO_A;
    STATE(rig)->tx_vfo = tx_vfo;
    CACHE(rig)->split = split;
    CACHE(rig)->split_vfo = tx_vfo;
    elapsed_ms(&CACHE(rig)->time_split, HAMLIB_ELAPSED_SET);
}

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

static int test_k3_agc_and_nb(void)
{
    static const char *commands[] =
    {
        "GT;", "GT;", "GT;", "GT;", "GT;", "GT;", "GT;", "GT;", "GT;",
        "NB;", "NB;", "NB;", "NB;", "NB;", "NB;", "NB;"
    };
    static const char *responses[] =
    {
        "GT000;", "GT002;", "GT004;", "GT0020;", "GT0040;", "GT0021;", "GT0041;",
        "GT0042;", "GT0050;",
        "NB0;", "NB1;", "NB00;", "NB10;", "NB20;", "NB11;", "NB100;"
    };
    static const int expected[] =
    {
        RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_SLOW, RIG_AGC_OFF, RIG_AGC_OFF,
        RIG_AGC_FAST, RIG_AGC_SLOW, -RIG_EPROTO, -RIG_EPROTO,
        0, 1, 0, 1, -RIG_EPROTO, -RIG_EPROTO, -RIG_EPROTO
    };
    struct peer_case peer =
    {
        .commands = commands,
        .responses = responses,
        .count = sizeof(commands) / sizeof(commands[0]),
        .status = -1
    };
    int sockets[2], failed = 0;
    pthread_t thread;
    RIG *rig = rig_init(RIG_MODEL_K3);

    if (rig == NULL) { return 1; }

    if (open_test_connection(sockets) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    peer.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;

    for (size_t i = 0; i < peer.count; i++)
    {
        int retval, actual;

        if (strcmp(commands[i], "GT;") == 0)
        {
            value_t level = { .i = -1 };
            retval = rig->caps->get_level(rig, RIG_VFO_A, RIG_LEVEL_AGC, &level);
            actual = level.i;
        }
        else
        {
            actual = -1;
            retval = rig->caps->get_func(rig, RIG_VFO_A, RIG_FUNC_NB, &actual);
        }

        if (retval != (expected[i] < 0 ? expected[i] : RIG_OK)
                || actual != (expected[i] < 0 ? -1 : expected[i]))
        {
            fprintf(stderr, "K3 %s reply %s: expected %d, got result %d, value %d\n",
                    commands[i], responses[i], expected[i], retval, actual);
            failed = 1;
        }
    }

    RIGPORT(rig)->fd = -1;
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);
    rig_cleanup(rig);

    return failed || peer.status != 0;
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
    STATE(rig)->rx_vfo = initial_rx_vfo;
    STATE(rig)->tx_vfo = initial_tx_vfo;

    retval = rig->caps->get_vfo(rig, &vfo);
    actual_rx_vfo = STATE(rig)->rx_vfo;
    actual_tx_vfo = STATE(rig)->tx_vfo;

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

static int test_k3_vfo_steps(void)
{
    static const char *commands[] =
    {
        "UPB;", "ID;", "FB;", "FA;",
        "DNB;", "ID;", "FB;", "FA;",
        "UP;", "ID;", "FA;", "FB;"
    };
    static const char *responses[] =
    {
        NULL, "ID017;", "FB00007236010;", "FA00007155390;",
        NULL, "ID017;", "FB00007236000;", "FA00007155390;",
        NULL, "ID017;", "FA00007155400;", "FB00007236010;"
    };
    static const struct
    {
        vfo_t vfo;
        vfo_op_t operation;
        freq_t wanted;
        freq_t other;
    } steps[] =
    {
        { RIG_VFO_B,    RIG_OP_UP,   7236010, 7155390 },
        { RIG_VFO_B,    RIG_OP_DOWN, 7236000, 7155390 },
        { RIG_VFO_CURR, RIG_OP_UP,   7155400, 7236010 }
    };
    struct peer_case test =
    {
        .commands = commands,
        .responses = responses,
        .count = sizeof(commands) / sizeof(commands[0]),
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    RIG *rig = rig_init(RIG_MODEL_K3);
    int result = 1;

    if (rig == NULL) { return 1; }

    if (open_test_connection(sockets) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    STATE(rig)->comm_state = 1;
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->current_vfo = RIG_VFO_A;
    rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, HAMLIB_CACHE_ALWAYS);
    rig_set_cache_freq(rig, RIG_VFO_A, 7155390);
    rig_set_cache_freq(rig, RIG_VFO_B, 7236000);

    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i)
    {
        vfo_t vfo = steps[i].vfo == RIG_VFO_CURR ? RIG_VFO_A : steps[i].vfo;
        vfo_t other_vfo = vfo == RIG_VFO_A ? RIG_VFO_B : RIG_VFO_A;
        freq_t freq = 0;
        freq_t other = 0;

        if (rig_vfo_op(rig, steps[i].vfo, steps[i].operation) != RIG_OK
                || rig_get_freq(rig, vfo, &freq) != RIG_OK
                || rig_get_freq(rig, other_vfo, &other) != RIG_OK
                || freq != steps[i].wanted || other != steps[i].other
                || STATE(rig)->current_vfo != RIG_VFO_A)
        {
            fprintf(stderr, "K3 step %zu: unexpected operation or readback\n", i);
            goto cleanup;
        }
    }

    result = 0;

cleanup:
    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    return result || test.status != 0;
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

enum split_query
{
    SPLIT_FREQ,
    SPLIT_MODE,
    SPLIT_FREQ_MODE
};

static int run_k3_split_get_case(enum split_query query, bool split,
                                 bool retry_malformed)
{
    const char *commands[5];
    const char *responses[5];
    size_t count = 0;
    struct peer_case test =
    {
        .commands = commands,
        .responses = responses,
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    freq_t freq = -1;
    rmode_t mode = RIG_MODE_CW;
    pbwidth_t width = -1;
    int result = 1;

    RIG *rig = rig_init(RIG_MODEL_K3);

    if (rig == NULL) { return 1; }

    bool uses_if = rig->caps->get_split_vfo == kenwood_get_split_vfo_if;
    const char *status_command = uses_if ? "IF;" : "FT;";
    const char *split_on = uses_if ? "IF00007155390     -000000 0001001001 ;"
                           : "FT1;";
    const char *split_off = uses_if ? "IF00007155390     -000000 0001000001 ;"
                            : "FT0;";

    if (retry_malformed)
    {
        commands[count] = status_command;
        responses[count++] = uses_if ? "IF00007155390     -000000 0001002001 ;"
                             : "FT2;";
    }

    commands[count] = status_command;
    responses[count++] = split ? split_on : split_off;

    if (split && query != SPLIT_MODE)
    {
        commands[count] = "FB;";
        responses[count++] = "FB00007236000;";
    }

    if (split && query != SPLIT_FREQ)
    {
        commands[count] = "MD$;";
        responses[count++] = "MD$1;";
        commands[count] = "BW$;";
        responses[count++] = "BW$0400;";
    }

    test.count = count;

    if (open_test_connection(sockets) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->comm_state = 1;
    STATE(rig)->current_vfo = RIG_VFO_A;

    if (retry_malformed)
    {
        int ret = rig_get_split_freq_mode(rig, RIG_VFO_CURR, &freq, &mode, &width);

        if (ret != -RIG_EPROTO)
        {
            fprintf(stderr, "Malformed K3 split response returned %d\n", ret);
            goto cleanup;
        }

        /* The Kenwood wire cache retains any well-framed IF reply. Force
         * only that cache to expire to exercise retry of unknown routing. */
        if (uses_if)
        {
            struct kenwood_priv_data *priv = STATE(rig)->priv;
            elapsed_ms(&priv->cache_start, HAMLIB_ELAPSED_INVALIDATE);
        }
    }

    int ret;

    switch (query)
    {
    case SPLIT_FREQ:
        ret = rig_get_split_freq(rig, RIG_VFO_CURR, &freq);
        break;

    case SPLIT_MODE:
        ret = rig_get_split_mode(rig, RIG_VFO_CURR, &mode, &width);
        break;

    case SPLIT_FREQ_MODE:
    default:
        ret = rig_get_split_freq_mode(rig, RIG_VFO_CURR, &freq, &mode, &width);
        break;
    }

    if (ret == RIG_OK && (query == SPLIT_MODE || freq == (split ? 7236000 : 0))
            && (query == SPLIT_FREQ
                || (mode == (split ? RIG_MODE_LSB : RIG_MODE_NONE)
                    && width == (split ? 4000 : 0))))
    {
        result = 0;
    }
    else
    {
        fprintf(stderr, "K3 split query %d returned %d, freq %.0f, mode %s, "
                        "width %ld\n", query, ret, freq, rig_strrmode(mode), width);
    }

cleanup:
    RIGPORT(rig)->fd = -1;
    STATE(rig)->comm_state = 0;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    return result || test.status != 0;
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
    static const char *k3_commands[] =
    {
        "MD$;", "DT;", "DT0;", "ID;", "DT;", "MD$;", "MD$6;", "ID;", "MD$;"
    };
    static const char *k3_responses[] =
    {
        "MD$9;", "DT2;", NULL, "ID017;", "DT0;", "MD$9;", NULL, "ID017;", "MD$6;"
    };
    static const char *k3_rttyr_commands[] =
    {
        "MD$;", "DT;", "DT2;", "ID;", "DT;", "MD$;", "MD$9;", "ID;", "MD$;"
    };
    static const char *k3_rttyr_responses[] =
    {
        "MD$6;", "DT0;", NULL, "ID017;", "DT2;", "MD$6;", NULL, "ID017;", "MD$9;"
    };
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
    else if (mode == RIG_MODE_RTTYR)
    {
        commands = k3_rttyr_commands;
        responses = k3_rttyr_responses;
        count = sizeof(k3_rttyr_commands) / sizeof(k3_rttyr_commands[0]);
    }
    else
    {
        commands = k3_commands;
        responses = k3_responses;
        count = sizeof(k3_commands) / sizeof(k3_commands[0]);
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

static int run_k3_split_sequence(rig_model_t model)
{
    static const char *commands[] =
    {
        "FT;",
        "FT;", "FT1;", "ID;", "FT;",
        "FB;", "FB00010136500;", "ID;", "FB;",
        "FB;", "FB00010136500;", "ID;", "FB;",
        "MD$;", "MD$6;", "ID;", "DT;", "MD$;",
        "FT;", "FT1;", "ID;", "FT;",
        "FT;",
        "FT;", "FR0;", "ID;", "FT;"
    };
    static const char *responses[] =
    {
        "FT2;",
        "FT0;", NULL, "ID017;", "FT0;",
        "FB00010136000;", NULL, "ID017;", "FB00010136000;",
        "FB00010136000;", NULL, "ID017;", "FB00010136500;",
        "MD$2;", NULL, "ID017;", "DT0;", "MD$6;",
        "FT0;", NULL, "ID017;", "FT1;",
        "FT1;",
        "FT1;", NULL, "ID017;", "FT0;"
    };
    struct peer_case test =
    {
        .commands = commands,
        .responses = responses,
        .count = sizeof(commands) / sizeof(commands[0]),
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    RIG *rig = rig_init(model);
    int result = 1;

    if (rig == NULL) { return 1; }

    if (open_test_connection(sockets) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    STATE(rig)->comm_state = 1;
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->current_vfo = RIG_VFO_A;

    /* The prior band left a cached split-on value on this handle. */
    set_split_routing(rig, RIG_SPLIT_ON, RIG_VFO_B);
    int ret = rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, RIG_VFO_B);

    if (ret != -RIG_EPROTO)
    {
        fprintf(stderr, "%s: malformed FT read returned %d\n",
                rig->caps->model_name, ret);
        goto cleanup;
    }

    ret = rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, RIG_VFO_B);

    if (ret != -RIG_ERJCTED || CACHE(rig)->split != RIG_SPLIT_OFF
            || STATE(rig)->tx_vfo != RIG_VFO_A)
    {
        fprintf(stderr, "%s: rejected FT1 returned %d, split %d, TX VFO %s\n",
                rig->caps->model_name, ret, CACHE(rig)->split,
                rig_strvfo(STATE(rig)->tx_vfo));
        goto cleanup;
    }

    ret = rig_set_split_freq_mode(rig, RIG_VFO_CURR, 10136500,
                                  RIG_MODE_PKTUSB, RIG_PASSBAND_NOCHANGE);

    if (ret != -RIG_ERJCTED)
    {
        fprintf(stderr, "%s: ignored FB write returned %d\n",
                rig->caps->model_name, ret);
        goto cleanup;
    }

    ret = rig_set_split_freq_mode(rig, RIG_VFO_CURR, 10136500,
                                  RIG_MODE_PKTUSB, RIG_PASSBAND_NOCHANGE);

    if (ret != RIG_OK || CACHE(rig)->split != RIG_SPLIT_ON
            || STATE(rig)->tx_vfo != RIG_VFO_B)
    {
        fprintf(stderr, "%s: prepared split returned %d, split %d, TX VFO %s\n",
                rig->caps->model_name, ret, CACHE(rig)->split,
                rig_strvfo(STATE(rig)->tx_vfo));
        goto cleanup;
    }

    if (rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, RIG_VFO_B) != RIG_OK
            || rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_OFF,
                                  RIG_VFO_A) != RIG_OK)
    {
        fprintf(stderr, "%s: split confirm or cancel failed\n",
                rig->caps->model_name);
        goto cleanup;
    }

    result = CACHE(rig)->split != RIG_SPLIT_OFF || STATE(rig)->tx_vfo != RIG_VFO_A;

cleanup:
    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (test.status != 0)
    {
        fprintf(stderr, "K3 split CAT sequence differed from expectation\n");
        return 1;
    }

    return result;
}

static int run_meter_case(const char *name, setting_t level,
                          const char *command,
                          const char *response, int expected_retval,
                          float expected_value)
{
    const char *commands[] = { command, "ID;" };
    const char *responses[] = { response, "ID017;" };
    struct peer_case test =
    {
        .fd = -1,
        .commands = commands,
        .responses = responses,
        .count = 2,
        .status = -1
    };
    int sockets[2];
    pthread_t thread;
    RIG *rig;
    value_t value = { .f = -1.0f };
    int retval;

    if (open_test_connection(sockets) != 0) { return 1; }

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

    retval = rig->caps->get_level(rig, RIG_VFO_CURR, level, &value);

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (test.status != 0 || retval != expected_retval
            || fabsf(value.f - (retval == RIG_OK ? expected_value : -1.0f))
            > 0.00001f)
    {
        fprintf(stderr, "%s: expected %d/%.4f, got %d/%.4f (peer %d)\n",
                name, expected_retval, expected_value, retval, value.f,
                test.status);
        return 1;
    }

    return 0;
}

static int test_meters(void)
{
    static const struct
    {
        const char *name;
        setting_t level;
        const char *command;
        const char *response;
        int retval;
        float value;
    } cases[] =
    {
        {
            "2w", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "PO0020;", RIG_OK, 2.0f
        },
        {
            "20w", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "PO0200;", RIG_OK, 20.0f
        },
        {
            "minimum-nonzero-output", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "PO0001;", RIG_OK, 0.1f
        },
        {
            "110w", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "PO1100;", RIG_OK, 110.0f
        },
        {
            "zero-output", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "PO0000;", RIG_OK, 0.0f
        },
        {
            "2w-normalized", RIG_LEVEL_RFPOWER_METER,
            "PO;", "PO0020;", RIG_OK, 0.02f
        },
        {
            "20w-normalized", RIG_LEVEL_RFPOWER_METER,
            "PO;", "PO0200;", RIG_OK, 0.2f
        },
        {
            "swr", RIG_LEVEL_SWR,
            "TM;", "TM003004020012;", RIG_OK, 1.2f
        },
        {
            "alc", RIG_LEVEL_ALC,
            "TM;", "TM003004020012;", RIG_OK, 3.0f
        },
        {
            "malformed-power-response", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "PO00X0;", -RIG_EPROTO, 0.0f
        },
        {
            "short-power-response", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "PO020;", -RIG_EPROTO, 0.0f
        },
        {
            "power-query-unavailable", RIG_LEVEL_RFPOWER_METER_WATTS,
            "PO;", "N;", -RIG_ENAVAIL, 0.0f
        },
        {
            "malformed-tm-response", RIG_LEVEL_SWR,
            "TM;", "TM00300402X012;", -RIG_EPROTO, 0.0f
        }
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_meter_case(cases[i].name, cases[i].level, cases[i].command,
                           cases[i].response, cases[i].retval, cases[i].value))
        {
            return 1;
        }
    }

    return 0;
}

static int run_k3_passband_readback(void)
{
    static const char *const commands[] =
    {
        "MD;", "BW;", "MD$;", "BW$;", "MD1;", "ID;", "BW0239;", "ID;",
        "MD;", "BW;",
        "MD;", "BW;", "MD;", "BW;", "MD$;", "BW$;", "MD2;", "ID;",
        "MD;", "BW;"
    };
    static const char *const responses[] =
    {
        "MD1;", "BW0390;", "MD$1;", "BW$0400;", NULL, "ID017;", NULL, "ID017;",
        "MD1;", "BW0235;",
        "MD1;", "BW0235;", "MD1;", "BW0235;", "MD$1;", "BW$0390;", NULL, "ID017;",
        "MD2;", "BW0290;"
    };
    struct peer_case test =
    {
        .commands = commands,
        .responses = responses,
        .count = sizeof(commands) / sizeof(commands[0]),
        .status = -1
    };
    RIG *rig = rig_init(RIG_MODEL_K3);
    int sockets[2];
    pthread_t thread;
    int failed = 1;
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = 0;

    if (rig == NULL) { return 1; }

    if (open_test_connection(sockets) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    STATE(rig)->comm_state = 1;
    ((struct kenwood_priv_data *)STATE(rig)->priv)->is_k3 = 1;
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->current_vfo = RIG_VFO_A;
    rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 10000);

    if (rig_set_mode(rig, RIG_VFO_A, RIG_MODE_LSB, 2390) != RIG_OK
            || rig_get_mode(rig, RIG_VFO_A, &mode, &width) != RIG_OK
            || mode != RIG_MODE_LSB || width != 2350)
    {
        fprintf(stderr, "K3 mode readback did not report the quantized width "
                        "(got %ld Hz)\n", width);
        goto cleanup;
    }

    if (rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USB, RIG_PASSBAND_NOCHANGE) != RIG_OK
            || rig_get_mode(rig, RIG_VFO_A, &mode, &width) != RIG_OK
            || mode != RIG_MODE_USB || width != 2900)
    {
        fprintf(stderr, "K3 mode readback did not report the recalled width "
                        "(got %ld Hz)\n", width);
        goto cleanup;
    }

    failed = 0;

cleanup:
    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);
    return failed || test.status != 0;
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
    STATE(rig)->current_vfo = initial_current;
    set_split_routing(rig, RIG_SPLIT_OFF, RIG_VFO_A);
    rig_set_cache_freq(rig, RIG_VFO_A, 28074000);
    rig_set_cache_freq(rig, RIG_VFO_B, untouched_b);

    if (test->vfo == RIG_VFO_NONE)
    {
        split_t initial = (split_t)(test->stale_cache ? test->requested : test->initial);
        set_split_routing(rig, initial, initial ? RIG_VFO_B : RIG_VFO_A);
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

    if (test->vfo == RIG_VFO_NONE)
    {
        split_t expected_split = (split_t)(retval == RIG_OK
                                           ? test->requested : expected_actual);
        vfo_t expected_tx = expected_split ? RIG_VFO_B : RIG_VFO_A;
        rig_get_cache_freq(rig, RIG_VFO_B, &cached_b, NULL);
        cache_age = elapsed_ms(&CACHE(rig)->time_split, HAMLIB_ELAPSED_GET);
        cache_timeout = CACHE(rig)->timeout_ms;

        if (CACHE(rig)->split != expected_split || STATE(rig)->rx_vfo != RIG_VFO_A
                || STATE(rig)->tx_vfo != expected_tx || CACHE(rig)->split_vfo != expected_tx
                || cached_b != untouched_b)
        {
            fprintf(stderr, "%s: split/routing/B cache = %d/%d/%d/%d/%.0f\n",
                    test->name, CACHE(rig)->split, STATE(rig)->rx_vfo, STATE(rig)->tx_vfo,
                    CACHE(rig)->split_vfo, cached_b);
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

            if (read_retval != RIG_OK || !isfinite(observed)
                    || fabs(observed - expected_api) > 0.01)
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

    if (STATE(rig)->current_vfo != initial_current)
    {
        fprintf(stderr, "%s: current VFO changed to %d\n", test->name,
                STATE(rig)->current_vfo);
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
    STATE(rig)->current_vfo = RIG_VFO_A;
    set_split_routing(rig, RIG_SPLIT_OFF, RIG_VFO_A);
    CACHE_RESET;

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
        { .name = "split-negative-acknowledgement", .vfo = RIG_VFO_NONE,
          .requested = 1, .verify_error = "N;", .expected_retval = -RIG_ENAVAIL },
        { .name = "split-overflow", .vfo = RIG_VFO_NONE,
          .requested = 1, .verify_error = "O;", .expected_retval = -RIG_EPROTO },
        { .name = "split-communication-error", .vfo = RIG_VFO_NONE,
          .requested = 1, .verify_error = "E;", .expected_retval = -RIG_EIO },
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

static int split_set_calls;

static int observe_split_off(RIG *rig, vfo_t rx_vfo, split_t split, vfo_t tx_vfo)
{
    split_set_calls++;
    set_split_routing(rig, RIG_SPLIT_OFF, RIG_VFO_A);

    if (rx_vfo == RIG_VFO_A && split == RIG_SPLIT_ON && tx_vfo == RIG_VFO_B)
    {
        return -RIG_ERJCTED;
    }

    if (rx_vfo == RIG_VFO_B && split == RIG_SPLIT_OFF && tx_vfo == RIG_VFO_SUB)
    {
        return RIG_OK;
    }

    return -RIG_EINVAL;
}

static int fixture_get_vfo(RIG *rig, vfo_t *vfo)
{
    *vfo = STATE(rig)->current_vfo;
    return RIG_OK;
}

static int check_split_setter_cache(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    if (!rig) { return 1; }

    struct rig_caps *original_caps = rig->caps;
    struct rig_caps caps = *original_caps;
    int failed = 1;
    int status;

    rig->caps = &caps;
    caps.rig_model = RIG_MODEL_K4;
    caps.set_split_vfo = observe_split_off;
    caps.get_vfo = fixture_get_vfo;
    caps.targetable_vfo = RIG_TARGETABLE_FREQ;
    STATE(rig)->comm_state = 1;
    STATE(rig)->current_vfo = RIG_VFO_A;
    set_split_routing(rig, RIG_SPLIT_ON, RIG_VFO_B);
    split_set_calls = 0;

    status = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);
    if (status != -RIG_ERJCTED || split_set_calls != 1
            || STATE(rig)->current_vfo != RIG_VFO_A
            || CACHE(rig)->split != RIG_SPLIT_OFF
            || STATE(rig)->rx_vfo != RIG_VFO_A || STATE(rig)->tx_vfo != RIG_VFO_A
            || CACHE(rig)->split_vfo != RIG_VFO_A)
    {
        fprintf(stderr, "K4 split rejection lost readback: status=%d calls=%d\n",
                status, split_set_calls);
        goto cleanup;
    }

    STATE(rig)->current_vfo = RIG_VFO_B;
    split_set_calls = 0;
    status = rig_set_split_vfo(rig, RIG_VFO_B, RIG_SPLIT_OFF, RIG_VFO_SUB);
    if (status != RIG_OK || split_set_calls != 1
            || STATE(rig)->current_vfo != RIG_VFO_B
            || CACHE(rig)->split != RIG_SPLIT_OFF
            || STATE(rig)->rx_vfo != RIG_VFO_A || STATE(rig)->tx_vfo != RIG_VFO_A
            || CACHE(rig)->split_vfo != RIG_VFO_A)
    {
        fprintf(stderr, "K4 split success lost readback: status=%d calls=%d\n",
                status, split_set_calls);
        goto cleanup;
    }

    caps.rig_model = original_caps->rig_model;
    STATE(rig)->current_vfo = RIG_VFO_A;
    for (int targetable = 0; targetable < 2; targetable++)
    {
        caps.targetable_vfo = targetable ? RIG_TARGETABLE_FREQ : 0;
        set_split_routing(rig, RIG_SPLIT_ON, RIG_VFO_B);
        split_set_calls = 0;
        status = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);
        if (status != RIG_OK || split_set_calls != 0
                || CACHE(rig)->split != RIG_SPLIT_ON
                || STATE(rig)->rx_vfo != RIG_VFO_A || STATE(rig)->tx_vfo != RIG_VFO_B
                || CACHE(rig)->split_vfo != RIG_VFO_B)
        {
            fprintf(stderr, "split cache shortcut failed: targetable=%d status=%d calls=%d\n",
                    targetable, status, split_set_calls);
            goto cleanup;
        }
    }

    failed = 0;
cleanup:
    rig->caps = original_caps;
    STATE(rig)->comm_state = 0;
    rig_cleanup(rig);
    return failed;
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
    rig_register(&k3s_caps);

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

    if (run_k3_split_sequence(RIG_MODEL_K3) != 0
            || run_k3_split_sequence(RIG_MODEL_K3S) != 0)
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

    if (test_k3_agc_and_nb() != 0)
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

    if (run_k3_passband_readback() != 0)
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

    if (test_k3_vfo_steps() != 0)
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

    if (run_k3_split_get_case(SPLIT_FREQ, true, false)
            || run_k3_split_get_case(SPLIT_MODE, true, false)
            || run_k3_split_get_case(SPLIT_FREQ_MODE, true, true)
            || run_k3_split_get_case(SPLIT_FREQ_MODE, false, false))
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
                          RIG_MODE_PKTLSB) != 0)
    {
        failed = 1;
        goto cleanup;
    }

    if (run_mode_get_case("k3-mode-b-reverse-afsk", 0, 0, "MD$9;", "DT1;",
                          RIG_MODE_PKTUSB) != 0)
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

    if (run_split_mode_set_case(0, RIG_MODE_PKTUSB) != 0
            || run_split_mode_set_case(0, RIG_MODE_RTTYR) != 0)
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

    if (!failed) { failed = test_meters(); }
    if (!failed) { failed = run_setter_cases(); }
    if (!failed) { failed = run_query_cases(); }
    if (!failed) { failed = check_split_setter_cache(); }

cleanup:
#ifdef _WIN32
    WSACleanup();
#endif
    return failed;
}
