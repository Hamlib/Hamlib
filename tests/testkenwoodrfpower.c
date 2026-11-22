/*
 * Hamlib Kenwood RFPOWER restore tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * kenwood_get_level(RIG_LEVEL_RFPOWER) probes min/max with
 * PC;PC000;PC;PC255;PC;PC000; when the mode changes. With restore=0 the
 * final PC000; left the radio at minimum power (#2240). restore=1 must
 * write the original PC value back, matching MICGAIN.
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
#include "../rigs/kenwood/kenwood.h"

extern struct rig_caps ts50s_caps;

#define PROBE_REPLY "PC080;PC005;PC100;"
#define RESTORE_CMD "PC080;"
#define SIMPLE_REPLY "PC080;"
#define ID_REPLY "ID019;"

struct peer_state
{
    int fd;
    int saw_probe;
    int saw_restore;
    int saw_simple;
    int bad;
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

static int write_all(int fd, const char *buffer, size_t length)
{
    size_t written = 0;

    while (written < length)
    {
        int count = write_test_socket(fd, buffer + written, length - written);

        if (count <= 0) { return -1; }

        written += (size_t) count;
    }

    return 0;
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

/*
 * The probe is one write_block of six semicolon-terminated pieces.
 * Read them as six commands, reply once with the three PC; answers,
 * then handle restore PC080; (no echo) + ID; verify, then a later
 * simple PC; read.
 */
static void *run_peer(void *arg)
{
    struct peer_state *test = arg;
    char command[64];
    int probe_parts = 0;

    test->status = -1;

    while (read_command(test->fd, command, sizeof(command)) == 0)
    {
        if (strcmp(command, "PC;") == 0 ||
                strcmp(command, "PC000;") == 0 ||
                strcmp(command, "PC255;") == 0)
        {
            if (!test->saw_probe)
            {
                probe_parts++;

                if (probe_parts == 6)
                {
                    test->saw_probe = 1;

                    if (write_all(test->fd, PROBE_REPLY,
                                  strlen(PROBE_REPLY)) != 0)
                    {
                        test->bad = 1;
                        return NULL;
                    }
                }
            }
            else if (strcmp(command, "PC;") == 0)
            {
                test->saw_simple = 1;

                if (write_all(test->fd, SIMPLE_REPLY,
                              strlen(SIMPLE_REPLY)) != 0)
                {
                    test->bad = 1;
                    return NULL;
                }

                test->status = 0;
                return NULL;
            }
            else
            {
                test->bad = 1;
                return NULL;
            }
        }
        else if (strcmp(command, RESTORE_CMD) == 0)
        {
            test->saw_restore = 1;
            /* Kenwood PC set has no echo; wait for ID; verify next. */
        }
        else if (strcmp(command, "ID;") == 0)
        {
            if (write_all(test->fd, ID_REPLY, strlen(ID_REPLY)) != 0)
            {
                test->bad = 1;
                return NULL;
            }
        }
        else
        {
            test->bad = 1;
            return NULL;
        }
    }

    return NULL;
}

int main(void)
{
    int sockets[2];
    pthread_t thread;
    struct peer_state test =
    {
        .fd = -1,
        .saw_probe = 0,
        .saw_restore = 0,
        .saw_simple = 0,
        .bad = 0,
        .status = -1
    };
    RIG *rig;
    struct kenwood_priv_data *priv;
    value_t val;
    int retval;
    int failed = 0;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) { return 1; }

#endif

    rig_set_debug(RIG_DEBUG_NONE);
    rig_register(&ts50s_caps);
    rig = rig_init(RIG_MODEL_TS50);

    if (rig == NULL || open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "test setup failed\n");
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        return 1;
    }

    priv = STATE(rig)->priv;
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->comm_state = 1;
    /* Seed mode cache so rig_get_mode does not talk on the wire. */
    rig_set_cache_mode(rig, RIG_VFO_A, RIG_MODE_USB, kHz(2.4));
    priv->last_mode_pc = RIG_MODE_NONE;

    val.f = -1.0f;
    retval = rig->caps->get_level(rig, RIG_VFO_A, RIG_LEVEL_RFPOWER, &val);

    if (retval != RIG_OK || fabsf(val.f - 0.8f) > 0.001f)
    {
        fprintf(stderr, "probe get_level: expected OK/0.80, got %d/%.3f\n",
                retval, val.f);
        failed = 1;
    }

    val.f = -1.0f;
    retval = rig->caps->get_level(rig, RIG_VFO_A, RIG_LEVEL_RFPOWER, &val);

    if (retval != RIG_OK || fabsf(val.f - 0.8f) > 0.001f)
    {
        fprintf(stderr, "simple get_level: expected OK/0.80, got %d/%.3f\n",
                retval, val.f);
        failed = 1;
    }

    RIGPORT(rig)->fd = -1;
    STATE(rig)->comm_state = 0;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (test.bad || test.status != 0 || !test.saw_probe || !test.saw_restore
            || !test.saw_simple)
    {
        fprintf(stderr,
                "peer: bad=%d status=%d probe=%d restore=%d simple=%d\n",
                test.bad, test.status, test.saw_probe, test.saw_restore,
                test.saw_simple);
        failed = 1;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return failed;
}
