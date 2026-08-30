/*
 * Hamlib Yaesu NewCAT set command transaction tests
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
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"

extern struct rig_caps ft710_caps;
extern struct rig_caps ft9000_caps;
extern struct rig_caps ft891_caps;
extern struct rig_caps ft991_caps;

enum peer_response
{
    PEER_SUCCESS,
    PEER_REJECT,
    PEER_TIMEOUT
};

enum set_action
{
    ACTION_TUNE,
    ACTION_TUNER_OFF,
    ACTION_TUNER_ON
};

struct peer_case
{
    int fd;
    const char *set_command;
    const char *verify_command;
    const char *verify_response;
    enum peer_response response;
    int status;
};

struct query_case
{
    int fd;
    int status;
};

static int close_test_socket(int fd)
{
#ifdef _WIN32
    return closesocket((SOCKET) fd);
#else
    return close(fd);
#endif
}

static int read_test_socket(int fd, void *buffer, size_t length)
{
#ifdef _WIN32
    return recv((SOCKET) fd, buffer, (int) length, 0);
#else
    return (int) read(fd, buffer, length);
#endif
}

static int write_test_socket(int fd, const void *buffer, size_t length)
{
#ifdef _WIN32
    return send((SOCKET) fd, buffer, (int) length, 0);
#else
    return (int) write(fd, buffer, length);
#endif
}

static int wait_test_socket(int fd, int timeout_ms)
{
    fd_set read_fds;
    struct timeval timeout = { timeout_ms / 1000,
        (timeout_ms % 1000) * 1000
    };

    FD_ZERO(&read_fds);
#ifdef _WIN32
    FD_SET((SOCKET) fd, &read_fds);
#else
    FD_SET(fd, &read_fds);
#endif

    return select(fd + 1, &read_fds, NULL, NULL, &timeout);
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

    if (listener == INVALID_SOCKET)
    {
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(listener, (struct sockaddr *) &address, sizeof(address)) != 0
            || listen(listener, 1) != 0
            || getsockname(listener, (struct sockaddr *) &address,
                           &address_length) != 0)
    {
        closesocket(listener);
        return -1;
    }

    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client == INVALID_SOCKET
            || connect(client, (struct sockaddr *) &address,
                       sizeof(address)) != 0)
    {
        if (client != INVALID_SOCKET)
        {
            closesocket(client);
        }

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

static int read_frame(int fd, char *buffer, size_t capacity, int timeout_ms)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        int count;

        if (wait_test_socket(fd, timeout_ms) != 1)
        {
            return -1;
        }

        count = read_test_socket(fd, buffer + used, 1);

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
        int count = write_test_socket(fd, buffer + written, length - written);

        if (count <= 0)
        {
            return -1;
        }

        written += (size_t)count;
    }

    return 0;
}

static void *run_set_peer(void *arg)
{
    struct peer_case *test = arg;
    char command[32];

    test->status = -1;

    if (read_frame(test->fd, command, sizeof(command), 2000) != 0
            || strcmp(command, test->set_command) != 0)
    {
        return NULL;
    }

    if (test->response == PEER_REJECT && write_all(test->fd, "?;") != 0)
    {
        return NULL;
    }

    if (read_frame(test->fd, command, sizeof(command), 2000) != 0
            || strcmp(command, test->verify_command) != 0)
    {
        return NULL;
    }

    if (test->response != PEER_TIMEOUT
            && write_all(test->fd, test->verify_response) != 0)
    {
        return NULL;
    }

    if (test->response == PEER_TIMEOUT
            && read_frame(test->fd, command, sizeof(command), 400) == 0)
    {
        return NULL;
    }

    test->status = 0;
    return NULL;
}

static void *run_query_peer(void *arg)
{
    struct query_case *test = arg;
    char command[32];

    test->status = -1;

    if (read_frame(test->fd, command, sizeof(command), 2000) != 0
            || strcmp(command, "FA;") != 0
            || write_all(test->fd, "FA014074000;") != 0)
    {
        return NULL;
    }

    test->status = 0;
    return NULL;
}

static int invoke_set(RIG *rig, enum set_action action)
{
    switch (action)
    {
    case ACTION_TUNE:
        return rig->caps->vfo_op(rig, RIG_VFO_A, RIG_OP_TUNE);

    case ACTION_TUNER_OFF:
        return rig->caps->set_func(rig, RIG_VFO_A, RIG_FUNC_TUNER, 0);

    case ACTION_TUNER_ON:
        return rig->caps->set_func(rig, RIG_VFO_A, RIG_FUNC_TUNER, 1);
    }

    return -RIG_EINVAL;
}

static int socket_is_empty(int fd)
{
    return wait_test_socket(fd, 0) == 0;
}

static int run_case(const char *name, rig_model_t model,
                    enum set_action action, const char *set_command,
                    enum peer_response response, int expected,
                    int fast_commands, int verify_followup)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test =
    {
        .fd = -1,
        .set_command = set_command,
        .verify_command = model == RIG_MODEL_FT9000 ? "AI;" : "ID;",
        .verify_response = model == RIG_MODEL_FT9000 ? "AI0;" :
                           model == RIG_MODEL_FT891 ? "ID0650;" : "ID0000;",
        .response = response,
        .status = -1
    };
    RIG *rig;
    int retval;

    if (open_test_connection(sockets) != 0)
    {
        perror("test socket setup");
        return 1;
    }

    rig = rig_init(model);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 100;
    RIGPORT(rig)->retry = 3;
    test.fd = sockets[1];

    if (fast_commands
            && rig_set_conf(rig,
                            rig_token_lookup(rig, "fast_commands_token"),
                            "1") != RIG_OK)
    {
        fprintf(stderr, "%s: failed to enable fast commands\n", name);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    if (pthread_create(&thread, NULL, run_set_peer, &test) != 0)
    {
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    retval = invoke_set(rig, action);
    pthread_join(thread, NULL);

    if (test.status != 0 || retval != expected)
    {
        fprintf(stderr, "%s: expected %d, got %d (peer status %d)\n",
                name, expected, retval, test.status);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    if (response == PEER_REJECT && !socket_is_empty(sockets[0]))
    {
        fprintf(stderr, "%s: synchronization response was left unread\n", name);
        RIGPORT(rig)->fd = -1;
        rig_cleanup(rig);
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    if (verify_followup)
    {
        struct query_case query = { .fd = sockets[1], .status = -1 };
        freq_t frequency = 0;

        if (pthread_create(&thread, NULL, run_query_peer, &query) != 0)
        {
            RIGPORT(rig)->fd = -1;
            rig_cleanup(rig);
            close_test_socket(sockets[0]);
            close_test_socket(sockets[1]);
            return 1;
        }

        retval = rig->caps->get_freq(rig, RIG_VFO_A, &frequency);
        pthread_join(thread, NULL);

        if (query.status != 0 || retval != RIG_OK || frequency != 14074000)
        {
            fprintf(stderr,
                    "%s follow-up: expected 0/14074000, got %d/%g (peer %d)\n",
                    name, retval, frequency, query.status);
            RIGPORT(rig)->fd = -1;
            rig_cleanup(rig);
            close_test_socket(sockets[0]);
            close_test_socket(sockets[1]);
            return 1;
        }
    }

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    return 0;
}

int main(void)
{
    int result;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

#endif

    rig_set_debug(RIG_DEBUG_NONE);
    rig_register(&ft710_caps);
    rig_register(&ft9000_caps);
    rig_register(&ft891_caps);
    rig_register(&ft991_caps);

    result = run_case("FT-891 tune", RIG_MODEL_FT891, ACTION_TUNE,
                      "AC002;", PEER_SUCCESS, RIG_OK, 0, 0) != 0
             || run_case("FT-891 rejected tune", RIG_MODEL_FT891,
                         ACTION_TUNE, "AC002;", PEER_REJECT,
                         -RIG_ERJCTED, 0, 1) != 0
             || run_case("FT-891 tune timeout", RIG_MODEL_FT891,
                         ACTION_TUNE, "AC002;", PEER_TIMEOUT,
                         -RIG_ETIMEOUT, 0, 0) != 0
             || run_case("FT-710 fast tune", RIG_MODEL_FT710,
                         ACTION_TUNE, "AC003;", PEER_SUCCESS,
                         RIG_OK, 1, 0) != 0
             || run_case("FT-891 tuner off", RIG_MODEL_FT891,
                         ACTION_TUNER_OFF, "AC000;", PEER_SUCCESS,
                         RIG_OK, 0, 0) != 0
             || run_case("FT-891 tuner on", RIG_MODEL_FT891,
                         ACTION_TUNER_ON, "AC001;", PEER_SUCCESS,
                         RIG_OK, 0, 0) != 0
             || run_case("FT-891 rejected tuner on", RIG_MODEL_FT891,
                         ACTION_TUNER_ON, "AC001;", PEER_REJECT,
                         -RIG_ERJCTED, 0, 0) != 0
             || run_case("FT-710 tune", RIG_MODEL_FT710, ACTION_TUNE,
                         "AC003;", PEER_SUCCESS, RIG_OK, 0, 0) != 0
             || run_case("FT-9000 tune", RIG_MODEL_FT9000, ACTION_TUNE,
                         "AC002;", PEER_SUCCESS, RIG_OK, 0, 0) != 0
             || run_case("FT-991 tune", RIG_MODEL_FT991, ACTION_TUNE,
                         "AC002;", PEER_SUCCESS, RIG_OK, 0, 0) != 0;

#ifdef _WIN32
    WSACleanup();
#endif

    return result;
}
