/*
 * Hamlib Alinco DX-SR8 level getter tests
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
    printf("testdxsr8: skipped, requires socketpair()\n");
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

extern struct rig_caps dxsr8_caps;

struct peer_case
{
    int fd;
    setting_t level;
    int state;
    int status;
};

static int read_line(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        struct pollfd descriptor = { .fd = fd, .events = POLLIN };

        if (poll(&descriptor, 1, 2000) != 1
                || read(fd, buffer + used, 1) != 1)
        {
            return -1;
        }

        if (buffer[used++] == '\n')
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

static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    const char *expected_command;
    char command[32];
    char response[8];

    test->status = -1;
    expected_command = test->level == RIG_LEVEL_RFPOWER
                       ? "AL~RR_PWR\r\n" : "AL~RR_RFG\r\n";

    if (read_line(test->fd, command, sizeof(command)) != 0
            || strcmp(command, expected_command) != 0)
    {
        return NULL;
    }

    snprintf(response, sizeof(response), "%02d\r\n", test->state);

    if (write_all(test->fd, command) != 0
            || write_all(test->fd, response) != 0)
    {
        return NULL;
    }

    test->status = 0;
    return NULL;
}

static int run_case(setting_t level, int state, int expected_retval,
                    value_t expected_value)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case peer =
    {
        .fd = -1,
        .level = level,
        .state = state,
        .status = -1
    };
    RIG *rig;
    value_t value = { .i = -1 };
    int retval;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return 1;
    }

    rig = rig_init(RIG_MODEL_DXSR8);

    if (rig == NULL)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 100;
    RIGPORT(rig)->retry = 0;
    peer.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    retval = rig->caps->get_level(rig, RIG_VFO_CURR, level, &value);
    pthread_join(thread, NULL);
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    close(sockets[1]);

    if (peer.status != 0 || retval != expected_retval)
    {
        fprintf(stderr, "%s state %d: expected return %d, got %d (peer %d)\n",
                rig_strlevel(level), state, expected_retval, retval, peer.status);
        return 1;
    }

    if (retval == RIG_OK
            && ((RIG_LEVEL_IS_FLOAT(level) && value.f != expected_value.f)
                || (!RIG_LEVEL_IS_FLOAT(level) && value.i != expected_value.i)))
    {
        fprintf(stderr, "%s state %d: unexpected level value\n",
                rig_strlevel(level), state);
        return 1;
    }

    return 0;
}

static int run_int_case(setting_t level, int state, int expected_retval,
                        int expected_value)
{
    value_t value = { .i = expected_value };

    return run_case(level, state, expected_retval, value);
}

static int run_float_case(setting_t level, int state, int expected_retval,
                          float expected_value)
{
    value_t value = { .f = expected_value };

    return run_case(level, state, expected_retval, value);
}

int main(void)
{
    static const int preamp_values[] = { 0, 0, 0, 10 };
    static const int attenuator_values[] = { 0, 10, 20, 0 };
    size_t i;

    rig_set_debug(RIG_DEBUG_NONE);
    rig_register(&dxsr8_caps);

    for (i = 0; i < sizeof(preamp_values) / sizeof(preamp_values[0]); i++)
    {
        if (run_int_case(RIG_LEVEL_PREAMP, (int)i, RIG_OK,
                         preamp_values[i]) != 0
                || run_int_case(RIG_LEVEL_ATT, (int)i, RIG_OK,
                                attenuator_values[i]) != 0)
        {
            return 1;
        }
    }

    if (run_int_case(RIG_LEVEL_PREAMP, 4, -RIG_EPROTO, -1) != 0
            || run_int_case(RIG_LEVEL_ATT, 4, -RIG_EPROTO, -1) != 0
            || run_float_case(RIG_LEVEL_RFPOWER, 0, RIG_OK, 1.0) != 0
            || run_float_case(RIG_LEVEL_RFPOWER, 1, RIG_OK, 0.1) != 0
            || run_float_case(RIG_LEVEL_RFPOWER, 2, RIG_OK, 0.01) != 0
            || run_float_case(RIG_LEVEL_RFPOWER, 3, -RIG_EPROTO, -1.0) != 0)
    {
        return 1;
    }

    return 0;
}

#endif /* !_WIN32 */
