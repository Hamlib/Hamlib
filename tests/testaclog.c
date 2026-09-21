/*
 * Hamlib ACLog backend tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "hamlib/config.h"

#include <stdio.h>

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_SYS_SOCKET_H) && \
    defined(HAVE_UNISTD_H)
#define HAVE_ACLOG_SOCKET_TEST 1
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"

#include "../rigs/dummy/dummy.h"

#ifdef HAVE_ACLOG_SOCKET_TEST
struct peer_data
{
    int fd;
    const char *response;
    int status;
};

static int read_command(int fd)
{
    char command[128];
    size_t used = 0;

    while (used + 1 < sizeof(command))
    {
        if (read(fd, command + used, 1) != 1)
        {
            return -1;
        }

        if (command[used++] == '\n')
        {
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
        ssize_t count = write(fd, buffer + written, length - written);

        if (count <= 0)
        {
            return -1;
        }

        written += (size_t)count;
    }

    return 0;
}

static void *run_peer(void *arg)
{
    struct peer_data *peer = arg;

    peer->status = read_command(peer->fd);

    if (peer->status == 0
            && write_all(peer->fd, peer->response, strlen(peer->response)) != 0)
    {
        peer->status = -1;
    }

    return NULL;
}

static int run_case(const char *name, const char *response, int expected_status,
                    freq_t expected_frequency)
{
    int sockets[2];
    pthread_t thread;
    struct peer_data peer = { .fd = -1, .response = response, .status = -1 };
    RIG *rig;
    freq_t frequency = 0;
    int status;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return 1;
    }

    peer.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    rig = rig_init(RIG_MODEL_ACLOG);

    if (rig == NULL)
    {
        close(sockets[0]);
        close(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    status = rig->caps->get_freq(rig, RIG_VFO_A, &frequency);

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (peer.status != 0 || status != expected_status)
    {
        fprintf(stderr, "%s: expected peer/status 0/%d, got %d/%d\n", name,
                expected_status, peer.status, status);
        return 1;
    }

    if (status == RIG_OK && frequency != expected_frequency)
    {
        fprintf(stderr, "%s: expected frequency %.0f, got %.0f\n", name,
                expected_frequency, frequency);
        return 1;
    }

    return 0;
}
#endif

int main(void)
{
#ifdef HAVE_ACLOG_SOCKET_TEST
    static const char valid_response[] =
        "<CMD><READBMFRESPONSE><FREQ>1,296.171100</FREQ></CMD>\r\n";
    static const char long_response[] =
        "<CMD><READBMFRESPONSE><FREQ>"
        "123456789012345678901234567890"
        "123456789012345678901234567890"
        "123456789012345678901234567890"
        "123456789012345678901234567890"
        "</FREQ></CMD>\r\n";
    static const char malformed_response[] =
        "<CMD><READBMFRESPONSE><FREQ>1.2.3</FREQ></CMD>\r\n";

    rig_register(&aclog_caps);

    if (run_case("valid frequency", valid_response, RIG_OK, 1296171100.0)
            || run_case("reject oversized frequency", long_response,
                        -RIG_EPROTO, 0.0)
            || run_case("reject malformed frequency", malformed_response,
                        -RIG_EPROTO, 0.0))
    {
        return 1;
    }

    return 0;
#else
    fprintf(stderr, "testaclog: socketpair unavailable; skipping tests\n");
    return 77;
#endif
}
