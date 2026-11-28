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
    static const char *commands[] = { "FR;", "FT;", "TQ;" };
    const char *responses[] = { fr, ft, tq };
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
        .count = tq != NULL ? 3 : ft != NULL ? 2 : 1,
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

static int run_k3_mode_get_case(const char *name, int split,
                             const char *md_response,
                             const char *dt_response,
                             rmode_t expected_mode)
{
    static const char *commands[] = { "MD$;", "DT;", "BW$;" };
    const char *responses[] = { md_response, dt_response, "BW$0482;" };
    struct peer_case test =
    {
        .fd = -1,
        .commands = commands,
        .responses = responses,
        .count = 3,
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

    rig = rig_init(RIG_MODEL_K3);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    priv = STATE(rig)->priv;
    priv->is_k3 = 1;
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

static int run_k3_split_mode_set_case(rmode_t mode)
{
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

    if (mode == RIG_MODE_RTTYR)
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

    rig = rig_init(RIG_MODEL_K3);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    priv = STATE(rig)->priv;
    priv->is_k3 = 1;
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
                "k3-%s-split-mode-set: expected status/retval 0/%d, "
                "got %d/%d\n", rig_strrmode(mode),
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

static int run_meter_case(const char *name, setting_t level,
                          const char *command,
                          const char *response, int expected_retval,
                          float expected_value)
{
    struct peer_case test =
    {
        .fd = -1,
        .commands = &command,
        .responses = &response,
        .count = 1,
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

    if (!failed) { failed = test_meters(); }
    if (!failed) { failed = test_k3_agc_and_nb(); }
    if (!failed) { failed = test_k3_vfo_steps(); }
    if (!failed) { failed = run_k3_passband_readback(); }
    if (!failed)
    {
        failed = run_k3_split_sequence(RIG_MODEL_K3)
                 || run_k3_split_sequence(RIG_MODEL_K3S)
                 || run_k3_split_get_case(SPLIT_FREQ, true, false)
                 || run_k3_split_get_case(SPLIT_MODE, true, false)
                 || run_k3_split_get_case(SPLIT_FREQ_MODE, true, true)
                 || run_k3_split_get_case(SPLIT_FREQ_MODE, false, false);
    }
    if (!failed)
    {
        failed = run_k3_mode_get_case("k3-mode-b-reverse-data", 0,
                                      "MD$9;", "DT0;", RIG_MODE_PKTLSB)
                 || run_k3_mode_get_case("k3-mode-b-reverse-afsk", 0,
                                         "MD$9;", "DT1;", RIG_MODE_PKTUSB)
                 || run_k3_mode_get_case("k3-split-shared-data", 1,
                                         "MD$6;", "DT1;", RIG_MODE_PKTLSB)
                 || run_k3_split_mode_set_case(RIG_MODE_PKTUSB)
                 || run_k3_split_mode_set_case(RIG_MODE_RTTYR);
    }

cleanup:
#ifdef _WIN32
    WSACleanup();
#endif
    return failed;
}
