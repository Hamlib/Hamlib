/*
 * Hamlib ID-5100 VFO transaction tests
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
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "cache.h"

struct civ_step
{
    const unsigned char *request;
    size_t request_len;
    const unsigned char *response;
    size_t response_len;
    const char *name;
};

struct peer_case
{
    int fd;
    const struct civ_step *steps;
    size_t step_count;
    int status;
    const char *failed_step;
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

static const unsigned char read_freq[] =
{ 0xfe, 0xfe, 0x8c, 0xe0, 0x03, 0xfd };
static const unsigned char select_a[] =
{ 0xfe, 0xfe, 0x8c, 0xe0, 0x07, 0xd0, 0xfd };
static const unsigned char select_b[] =
{ 0xfe, 0xfe, 0x8c, 0xe0, 0x07, 0xd1, 0xfd };
static const unsigned char ack[] =
{ 0xfe, 0xfe, 0xe0, 0x8c, 0xfb, 0xfd };
static const unsigned char nak[] =
{ 0xfe, 0xfe, 0xe0, 0x8c, 0xfa, 0xfd };
static const unsigned char freq_a[] =
{ 0xfe, 0xfe, 0xe0, 0x8c, 0x03, 0x00, 0x00, 0x76, 0x46, 0x01, 0xfd };
static const unsigned char freq_b[] =
{ 0xfe, 0xfe, 0xe0, 0x8c, 0x03, 0x00, 0x00, 0x49, 0x45, 0x01, 0xfd };

#define STEP(request_, response_, name_) \
    { request_, sizeof(request_), response_, sizeof(response_), name_ }

static const struct civ_step steps[] =
{
    STEP(read_freq, freq_b, "current frequency"),
    STEP(select_a, ack, "select A"),
    STEP(read_freq, freq_a, "refresh A after selection"),
    STEP(select_b, ack, "target B"),
    STEP(read_freq, freq_b, "read B"),
    STEP(select_a, ack, "restore A"),
    STEP(select_b, ack, "select B"),
    STEP(read_freq, freq_b, "refresh B after selection"),
    STEP(select_a, ack, "target A"),
    STEP(read_freq, freq_a, "read A"),
    STEP(select_b, ack, "restore B"),
    STEP(select_a, ack, "target A for rejected read"),
    STEP(read_freq, nak, "reject A frequency read"),
    STEP(select_b, ack, "restore B after rejected read"),
    STEP(select_a, nak, "reject A selection"),
};

static int read_frame(int fd, unsigned char *buffer, size_t capacity,
                      size_t *length)
{
    size_t used = 0;

    while (used < capacity)
    {
        int count = read_test_socket(fd, buffer + used, 1);

        if (count != 1) { return -1; }

        if (buffer[used++] == 0xfd)
        {
            *length = used;
            return 0;
        }
    }

    return -1;
}

static int write_all(int fd, const unsigned char *buffer, size_t length)
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
    unsigned char frame[64];
    size_t i;

    test->status = -1;

    for (i = 0; i < test->step_count; i++)
    {
        const struct civ_step *step = &test->steps[i];
        size_t length;

        if (read_frame(test->fd, frame, sizeof(frame), &length) != 0)
        {
            test->failed_step = step->name;
            return NULL;
        }

        if (length != step->request_len ||
                memcmp(frame, step->request, length) != 0)
        {
            test->failed_step = step->name;
            write_all(test->fd, nak, sizeof(nak));
            return NULL;
        }

        if (write_all(test->fd, step->response, step->response_len) != 0)
        {
            test->failed_step = step->name;
            return NULL;
        }
    }

    test->status = 0;
    return NULL;
}

static int check_result(const char *name, int actual, int expected)
{
    if (actual == expected) { return 0; }

    fprintf(stderr, "%s: expected %d, got %d (%s)\n", name, expected, actual,
            rigerror(actual));
    return 1;
}

int main(void)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test =
    {
        .fd = -1,
        .steps = steps,
        .step_count = sizeof(steps) / sizeof(steps[0]),
        .status = -1,
        .failed_step = NULL,
    };
    struct icom_priv_data *priv;
    RIG *rig;
    freq_t freq = 0;
    int failed = 0;
    int retval;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    rig_register(&id5100_caps);
    rig = rig_init(RIG_MODEL_ID5100);

    if (rig == NULL)
    {
        fprintf(stderr, "rig_init failed\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "test socket setup failed\n");
        rig_cleanup(rig);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 250;
    RIGPORT(rig)->retry = 0;
    priv = (struct icom_priv_data *) STATE(rig)->priv;
    priv->serial_USB_echo_off = 1;
    STATE(rig)->comm_state = 1;
    STATE(rig)->dual_watch = 1;
    rig_set_current_vfo_state(rig, RIG_VFO_CURR);
    rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 0);

    retval = rig_get_freq(rig, RIG_VFO_CURR, &freq);
    failed |= check_result("current read", retval, RIG_OK);
    failed |= check_result("current frequency", (int) freq, 145490000);
    failed |= check_result("current VFO unchanged", rig_get_current_vfo_state(rig),
                           RIG_VFO_CURR);
    failed |= check_result("dual watch unchanged", STATE(rig)->dual_watch, 1);

    retval = rig_get_freq(rig, RIG_VFO_A, &freq);
    failed |= check_result("unknown side is not targetable", retval,
                           -RIG_ENTARGET);
    failed |= check_result("unknown side remains unchanged", rig_get_current_vfo_state(rig),
                           RIG_VFO_CURR);

    retval = rig_set_freq(rig, RIG_VFO_A, 146760000);
    failed |= check_result("unknown side cannot be written", retval,
                           -RIG_ENTARGET);
    failed |= check_result("unknown side remains unchanged after write",
                           rig_get_current_vfo_state(rig), RIG_VFO_CURR);

    retval = rig_set_vfo(rig, RIG_VFO_A);
    failed |= check_result("select A", retval, RIG_OK);

    retval = rig_get_freq(rig, RIG_VFO_B, &freq);
    failed |= check_result("read B", retval, RIG_OK);
    failed |= check_result("B frequency", (int) freq, 145490000);
    failed |= check_result("restore A", rig_get_current_vfo_state(rig), RIG_VFO_A);

    retval = rig_set_vfo(rig, RIG_VFO_B);
    failed |= check_result("select B", retval, RIG_OK);

    retval = rig_get_freq(rig, RIG_VFO_A, &freq);
    failed |= check_result("read A", retval, RIG_OK);
    failed |= check_result("A frequency", (int) freq, 146760000);
    failed |= check_result("restore B", rig_get_current_vfo_state(rig), RIG_VFO_B);

    retval = rig_get_freq(rig, RIG_VFO_A, &freq);
    failed |= check_result("rejected frequency read", retval, -RIG_ERJCTED);
    failed |= check_result("restore B after rejected read", rig_get_current_vfo_state(rig),
                           RIG_VFO_B);

    retval = rig_get_freq(rig, RIG_VFO_A, &freq);
    failed |= check_result("rejected A selection", retval, -RIG_ERJCTED);
    failed |= check_result("keep B after rejected selection", rig_get_current_vfo_state(rig),
                           RIG_VFO_B);
    failed |= check_result("dual watch remains enabled", STATE(rig)->dual_watch, 1);

    RIGPORT(rig)->fd = -1;
    STATE(rig)->comm_state = 0;
    close_test_socket(sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);
    rig_cleanup(rig);

    if (test.status != 0)
    {
        fprintf(stderr, "CI-V transcript failed at %s\n",
                test.failed_step ? test.failed_step : "unknown step");
        failed = 1;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return failed;
}
