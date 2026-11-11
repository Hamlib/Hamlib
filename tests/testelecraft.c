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

extern struct rig_caps k4_caps;

struct peer_case
{
    int fd;
    const char *const *commands;
    const char *const *responses;
    size_t count;
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

cleanup:
#ifdef _WIN32
    WSACleanup();
#endif
    return failed;
}
