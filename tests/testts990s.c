/*
 * Hamlib TS-990S mode compatibility tests
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
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"

extern struct rig_caps ts990s_caps;

struct peer_state
{
    int fd;
    char mode_main;
    char mode_sub;
    char operating_band;
    int fail_next_mode;
    int fail_verify;
    int saw_data1_write;
    int status;
};

static int close_socket(int fd)
{
#ifdef _WIN32
    return closesocket((SOCKET)fd);
#else
    return close(fd);
#endif
}

static int read_socket(int fd, char *buffer, size_t length)
{
#ifdef _WIN32
    return recv((SOCKET)fd, buffer, (int)length, 0);
#else
    return (int)read(fd, buffer, length);
#endif
}

static int write_socket(int fd, const char *buffer, size_t length)
{
#ifdef _WIN32
    return send((SOCKET)fd, buffer, (int)length, 0);
#else
    return (int)write(fd, buffer, length);
#endif
}

static int read_command(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        int count = read_socket(fd, buffer + used, 1);

        if (count <= 0) { return -1; }

        if (buffer[used++] == ';')
        {
            buffer[used] = '\0';
            return 0;
        }
    }

    return -1;
}

static int write_reply(int fd, const char *reply)
{
    size_t written = 0;
    size_t length = strlen(reply);

    while (written < length)
    {
        int count = write_socket(fd, reply + written, length - written);

        if (count <= 0) { return -1; }

        written += (size_t)count;
    }

    return 0;
}

static int handle_command(struct peer_state *peer, const char *command)
{
    char reply[16];

    if (strcmp(command, "OM0;") == 0)
    {
        snprintf(reply, sizeof(reply), "OM0%c;", peer->mode_main);
        return write_reply(peer->fd, reply);
    }

    if (strcmp(command, "OM1;") == 0)
    {
        snprintf(reply, sizeof(reply), "OM1%c;", peer->mode_sub);
        return write_reply(peer->fd, reply);
    }

    if (strcmp(command, "CB;") == 0)
    {
        snprintf(reply, sizeof(reply), "CB%c;", peer->operating_band);
        return write_reply(peer->fd, reply);
    }

    if (strcmp(command, "ID;") == 0)
    {
        if (peer->fail_verify)
        {
            peer->fail_verify = 0;
            return write_reply(peer->fd, "?;");
        }

        return write_reply(peer->fd, "ID022;");
    }

    if (strncmp(command, "CB", 2) == 0 && command[2] != ';')
    {
        peer->operating_band = command[2];
        return 0;
    }

    if (strncmp(command, "OM0", 3) == 0 && command[3] != ';')
    {
        if (command[3] == 'D') { peer->saw_data1_write = 1; }

        if (peer->fail_next_mode)
        {
            peer->fail_next_mode = 0;
            peer->fail_verify = 1;
            return 0;
        }

        if (peer->operating_band == '0') { peer->mode_main = command[3]; }
        else { peer->mode_sub = command[3]; }

        return 0;
    }

    if (strncmp(command, "TB", 2) == 0)
    {
        return 0;
    }

    return -1;
}

static void *run_peer(void *arg)
{
    struct peer_state *peer = arg;
    char command[16];

    peer->status = 0;

    for (;;)
    {
        if (read_command(peer->fd, command, sizeof(command)) != 0) { return NULL; }

        if (handle_command(peer, command) != 0)
        {
            peer->status = 1;
            return NULL;
        }
    }
}

static int open_test_connection(int sockets[2])
{
#ifdef _WIN32
    (void)sockets;
    return -1;
#else
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
#endif
}

static int run_case(const char *name, char mode_main, char mode_sub,
                    rmode_t request, pbwidth_t width, int fail_next_mode,
                    int split, vfo_t target_vfo, int expected_retval, char expected_main,
                    char expected_sub, char expected_operating,
                    int expect_data1_write)
{
    int sockets[2];
    pthread_t thread;
    struct peer_state peer;
    RIG *rig;
    int retval;

    if (open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "%s: socket setup failed\n", name);
        return 1;
    }

    memset(&peer, 0, sizeof(peer));
    peer.fd = sockets[1];
    peer.mode_main = mode_main;
    peer.mode_sub = mode_sub;
    peer.operating_band = '0';
    peer.fail_next_mode = fail_next_mode;

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        close_socket(sockets[0]);
        close_socket(sockets[1]);
        return 1;
    }

    rig = rig_init(RIG_MODEL_TS990S);

    if (rig == NULL)
    {
        close_socket(sockets[0]);
        pthread_join(thread, NULL);
        close_socket(sockets[1]);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->comm_state = 1;
    STATE(rig)->current_vfo = RIG_VFO_MAIN;
    STATE(rig)->rx_vfo = RIG_VFO_MAIN;
    STATE(rig)->tx_vfo = RIG_VFO_SUB;

    if (split)
    {
        retval = rig_set_split_vfo(rig, RIG_VFO_MAIN, RIG_SPLIT_ON, RIG_VFO_SUB);

        if (retval != RIG_OK)
        {
            RIGPORT(rig)->fd = -1;
            rig_cleanup(rig);
            close_socket(sockets[0]);
            pthread_join(thread, NULL);
            close_socket(sockets[1]);
            return 1;
        }
    }

    if (split)
    {
        retval = rig_set_split_mode(rig, target_vfo, request, width);
    }
    else
    {
        retval = rig_set_mode(rig, target_vfo, request, width);
    }

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_socket(sockets[1]);

    if (retval != expected_retval
            || peer.status != 0
            || peer.mode_main != expected_main
            || peer.mode_sub != expected_sub
            || peer.operating_band != expected_operating
            || peer.saw_data1_write != expect_data1_write)
    {
        fprintf(stderr, "%s: result=%d mode=%c/%c operating=%c data1=%d\n",
                name, retval, peer.mode_main, peer.mode_sub,
                peer.operating_band, peer.saw_data1_write);
        return 1;
    }

    return 0;
}

int main(void)
{
    int failed = 0;

#ifdef _WIN32
    return 77;
#else
    rig_register(&ts990s_caps);

    {
        RIG *rig = rig_init(RIG_MODEL_TS990S);

        if (rig == NULL || rig_passband_normal(rig, RIG_MODE_USBD2) != 2600
                || rig_passband_normal(rig, RIG_MODE_LSBD3) != 2600)
        {
            fprintf(stderr, "TS-990S DATA profiles do not advertise a normal width\n");
            failed = 1;
        }

        if (rig != NULL) { rig_cleanup(rig); }
    }

    failed |= run_case("generic USB preserves USBD2", 'H', 'L',
                       RIG_MODE_PKTUSB, RIG_PASSBAND_NOCHANGE, 0, 0, RIG_VFO_MAIN,
                       RIG_OK, 'H', 'L', '0', 0);
    failed |= run_case("explicit USB width preserves USBD2", 'H', 'L',
                       RIG_MODE_PKTUSB, 2700, 0, 0, RIG_VFO_MAIN, RIG_OK,
                       'H', 'L', '0', 0);
    failed |= run_case("split generic USB preserves TX profile", 'H', 'H',
                       RIG_MODE_PKTUSB, RIG_PASSBAND_NOCHANGE, 0, 1, RIG_VFO_MAIN,
                       RIG_OK, 'H', 'H', '0', 0);
    failed |= run_case("mode error restores operating VFO", 'H', 'L',
                       RIG_MODE_USBD1, RIG_PASSBAND_NOCHANGE, 1, 0, RIG_VFO_SUB,
                       -RIG_ETIMEOUT, 'H', 'L', '0', 1);

    return failed;
#endif
}
