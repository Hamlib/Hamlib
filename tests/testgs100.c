/*
 * Hamlib GS100 transaction parser tests
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
#include <sys/socket.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"

#define GOM_PROMPT "\x1B[1;32mnanocom-ax\x1B[1;30m # \x1B[0m\x1B[0m"
#define GOM_LINE_END "\r\r\n"
#define GOM_MAXLINES 20

enum peer_response
{
    PEER_VALUE,
    PEER_OVERSIZED_LINE,
    PEER_EXCESS_LINES
};

extern struct rig_caps GS100_caps;

struct peer_case
{
    int fd;
    int bootstrap;
    const char *value;
    enum peer_response response;
    int status;
};

static int read_line(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        ssize_t count = read(fd, buffer + used, 1);

        if (count != 1) { return -1; }

        if (buffer[used++] == '\n')
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
        ssize_t count = write(fd, buffer + written, length - written);

        if (count <= 0) { return -1; }

        written += (size_t)count;
    }

    return 0;
}

static int write_line(int fd, const char *content)
{
    if (write_all(fd, content, strlen(content)) != 0) { return -1; }

    return write_all(fd, GOM_LINE_END, strlen(GOM_LINE_END));
}

static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    char command[64];
    char response[257];
    int i;

    test->status = -1;

    if (read_line(test->fd, command, sizeof(command)) != 0) { return NULL; }

    command[strcspn(command, "\r\n")] = '\0';

    if (test->bootstrap && write_line(test->fd, "") != 0) { return NULL; }

    if (!test->bootstrap && write_line(test->fd, command) != 0)
    {
        return NULL;
    }

    if (write_all(test->fd, GOM_PROMPT, strlen(GOM_PROMPT)) != 0)
    {
        return NULL;
    }

    if (read_line(test->fd, command, sizeof(command)) != 0) { return NULL; }

    command[strcspn(command, "\r\n")] = '\0';

    if (write_line(test->fd, command) != 0) { return NULL; }

    if (test->response == PEER_OVERSIZED_LINE)
    {
        memset(response, 'x', sizeof(response) - 1);
        response[sizeof(response) - 1] = '\0';

        if (write_line(test->fd, response) != 0) { return NULL; }
    }
    else if (test->response == PEER_EXCESS_LINES)
    {
        for (i = 0; i < GOM_MAXLINES; i++)
        {
            if (write_line(test->fd, "noise") != 0) { return NULL; }
        }
    }
    else
    {
        snprintf(response, sizeof(response), "freq=%s", test->value);

        if (write_line(test->fd, response) != 0) { return NULL; }
    }

    if (write_all(test->fd, GOM_PROMPT, strlen(GOM_PROMPT)) != 0)
    {
        return NULL;
    }

    test->status = 0;
    return NULL;
}

static int run_case(const char *name, const char *value, int expected,
                    freq_t expected_freq, int bootstrap,
                    enum peer_response response)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test = { .fd = -1, .bootstrap = bootstrap,
        .value = value, .response = response, .status = -1
    };
    RIG *rig;
    freq_t freq = 0;
    int retval;

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

    rig = rig_init(RIG_MODEL_GS100);

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
    retval = rig->caps->get_freq(rig, RIG_VFO_A, &freq);

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (test.status != 0 || retval != expected ||
            (retval == RIG_OK && freq != expected_freq))
    {
        fprintf(stderr, "%s: expected %d/%g, got %d/%g\n", name, expected,
                expected_freq, retval, freq);
        return 1;
    }

    return 0;
}

int main(void)
{
    rig_register(&GS100_caps);

    if (run_case("normal", "435000000", RIG_OK, 435000000, 1,
                 PEER_VALUE) != 0)
    {
        return 1;
    }

    if (run_case("empty", "", -RIG_EPROTO, 0, 0, PEER_VALUE) != 0)
    {
        return 1;
    }

    if (run_case("maximum", "0000000000435000000", RIG_OK, 435000000, 0,
                 PEER_VALUE) != 0)
    {
        return 1;
    }

    if (run_case("oversized-value", "99999999999999999999", -RIG_EPROTO,
                 0, 0, PEER_VALUE) != 0)
    {
        return 1;
    }

    if (run_case("oversized-line", NULL, -RIG_EPROTO, 0, 0,
                 PEER_OVERSIZED_LINE) != 0)
    {
        return 1;
    }

    return run_case("excess-lines", NULL, -RIG_EPROTO, 0, 0,
                    PEER_EXCESS_LINES);
}
