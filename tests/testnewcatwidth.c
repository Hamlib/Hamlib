/*
 * Hamlib Yaesu NewCAT legacy bandwidth tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifdef _WIN32

#include <stdio.h>

int main(void)
{
    printf("testnewcatwidth: skipped, requires socketpair()\n");
    return 77;
}

#else

#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"

extern struct rig_caps ft950_caps;

struct peer_case
{
    int fd;
    const char *mode_command;
    const char *mode_response;
    const char *width_command;
    const char *width_response;
    const char *narrow_command;
    const char *narrow_response;
    int status;
};

static int read_frame(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        struct pollfd descriptor = { .fd = fd, .events = POLLIN };
        ssize_t count;

        if (poll(&descriptor, 1, 2000) != 1)
        {
            return -1;
        }

        count = read(fd, buffer + used, 1);

        if (count != 1)
        {
            return -1;
        }

        if (buffer[used++] == ';')
        {
            buffer[used] = '\0';
            return 0;
        }
    }

    return -1;
}

static int write_all(int fd, const char *buffer)
{
    size_t length = strlen(buffer);
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

static int exchange(int fd, const char *expected, const char *response)
{
    char command[32];

    if (read_frame(fd, command, sizeof(command)) != 0
            || strcmp(command, expected) != 0)
    {
        return -1;
    }

    if (response != NULL && write_all(fd, response) != 0)
    {
        return -1;
    }

    return 0;
}

static void *run_peer(void *arg)
{
    struct peer_case *test = arg;

    test->status = -1;

    if (exchange(test->fd, test->mode_command,
                 test->mode_response) != 0
            || exchange(test->fd, test->width_command,
                        test->width_response) != 0)
    {
        return NULL;
    }

    if (test->narrow_command != NULL
            && exchange(test->fd, test->narrow_command,
                        test->narrow_response) != 0)
    {
        return NULL;
    }

    test->status = 0;
    return NULL;
}

static int run_narrow_case(const char *name, const char *narrow_response,
                           int expected_retval, pbwidth_t expected_width)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case peer =
    {
        .fd = -1,
        .mode_command = "MD0;",
        .mode_response = "MD02;",
        .width_command = "SH0;",
        .width_response = "SH000;",
        .narrow_command = "NA0;",
        .narrow_response = narrow_response,
        .status = -1
    };
    RIG *rig;
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = RIG_PASSBAND_NORMAL;
    int retval;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return 1;
    }

    rig = rig_init(RIG_MODEL_FT950);

    if (rig == NULL)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->timeout = 100;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->powerstat = RIG_POWER_ON;
    peer.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    retval = rig->caps->get_mode(rig, RIG_VFO_A, &mode, &width);
    pthread_join(thread, NULL);
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    close(sockets[1]);

    if (peer.status != 0 || retval != expected_retval
            || (retval == RIG_OK
                && (mode != RIG_MODE_USB || width != expected_width)))
    {
        fprintf(stderr,
                "%s: expected %d/USB/%d, got %d/%s/%d (peer %d)\n",
                name, expected_retval, (int)expected_width, retval,
                rig_strrmode(mode), (int)width, peer.status);
        return 1;
    }

    return 0;
}

int main(void)
{
    rig_set_debug(RIG_DEBUG_NONE);
    rig_register(&ft950_caps);

    if (run_narrow_case("wide default", "NA00;", RIG_OK, 2400) != 0
            || run_narrow_case("narrow default", "NA01;", RIG_OK, 1800) != 0
            || run_narrow_case("invalid narrow", "NA02;", -RIG_EPROTO,
                               RIG_PASSBAND_NORMAL) != 0)
    {
        return 1;
    }

    return 0;
}

#endif /* !_WIN32 */
