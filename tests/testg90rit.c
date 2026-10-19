/*
 * G90 RIT SET regression tests. No physical radio is needed.
 * Socket setup follows testicomfallback.c.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "misc.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

extern struct rig_caps g90_caps;
extern struct rig_caps ic7100_caps;
extern struct rig_caps x6100_caps;

struct exchange
{
    shortfreq_t offset;
    unsigned char request[16];
    size_t request_len;
    unsigned char response[16];
    size_t response_len;
    int expected_retval;
};

struct peer_case
{
    const struct exchange *exchanges;
    size_t count;
    int fd;
    int status;
};

static int close_socket(int fd)
{
#ifdef _WIN32
    return closesocket(fd);
#else
    return close(fd);
#endif
}

static int read_socket(int fd, void *buffer, size_t length)
{
#ifdef _WIN32
    return recv(fd, buffer, (int) length, 0);
#else
    return (int) read(fd, buffer, length);
#endif
}

static int write_socket(int fd, const void *buffer, size_t length)
{
#ifdef _WIN32
    return send(fd, buffer, (int) length, 0);
#else
    return (int) write(fd, buffer, length);
#endif
}

static int open_connection(int sockets[2])
{
#ifdef _WIN32
    SOCKET listener, client, peer;
    struct sockaddr_in address;
    int length = sizeof(address);

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == INVALID_SOCKET) { return -1; }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listener, (struct sockaddr *) &address, sizeof(address)) != 0
            || listen(listener, 1) != 0
            || getsockname(listener, (struct sockaddr *) &address,
                           &length) != 0)
    {
        closesocket(listener);
        return -1;
    }

    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client == INVALID_SOCKET
            || connect(client, (struct sockaddr *) &address,
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

static int has_input(int fd)
{
    fd_set readable;
    struct timeval timeout = { 0, 0 };

    FD_ZERO(&readable);
    FD_SET(fd, &readable);
    return select(fd + 1, &readable, NULL, NULL, &timeout);
}

static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    size_t i;

    test->status = -1;

    for (i = 0; i < test->count; i++)
    {
        const struct exchange *exchange = &test->exchanges[i];
        unsigned char request[16];
        size_t received = 0, sent = 0;

        do
        {
            if (received == sizeof(request)
                    || read_socket(test->fd, request + received, 1) != 1)
            {
                return NULL;
            }
        }
        while (request[received++] != 0xfd);

        if (received != exchange->request_len
                || memcmp(request, exchange->request, received) != 0)
        {
            fprintf(stderr, "unexpected RIT request at step %zu\n", i);
            return NULL;
        }

        while (sent < exchange->response_len)
        {
            int count = write_socket(test->fd, exchange->response + sent,
                                     exchange->response_len - sent);

            if (count <= 0) { return NULL; }

            sent += (size_t) count;
        }
    }

    test->status = 0;
    return NULL;
}

static RIG *prepare_rig(int fd)
{
    RIG *rig = rig_init(RIG_MODEL_G90);

    if (rig == NULL) { return NULL; }

    STATE(rig)->comm_state = 1;
    rig_set_current_vfo_state(rig, RIG_VFO_A);
    RIGPORT(rig)->fd = fd;
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->retry = 0;
    RIGPORT(rig)->timeout = 100;
    ((struct icom_priv_data *) STATE(rig)->priv)->serial_USB_echo_off = 1;
    return rig;
}

static void cleanup_rig(RIG *rig, int sockets[2])
{
    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    close_socket(sockets[0]);
    close_socket(sockets[1]);
    rig_cleanup(rig);
}

static int run_case(const char *name, const struct exchange *exchanges,
                    size_t count)
{
    int sockets[2], status = 0;
    pthread_t thread;
    struct peer_case test = { exchanges, count, -1, -1 };
    RIG *rig;
    size_t i;

    if (open_connection(sockets) != 0) { return 1; }

    rig = prepare_rig(sockets[0]);

    if (rig == NULL)
    {
        close_socket(sockets[0]);
        close_socket(sockets[1]);
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        status = 1;
    }
    else
    {
        shortfreq_t readback = 123;

        for (i = 0; i < count; i++)
        {
            int retval = rig_set_rit(rig, RIG_VFO_CURR,
                                     exchanges[i].offset);

            if (retval != exchanges[i].expected_retval)
            {
                fprintf(stderr, "%s step %zu: expected %d, got %d\n",
                        name, i, exchanges[i].expected_retval, retval);
                status = 1;
            }
        }

        /* Even after a successful SET, do not expose cached pseudo-GET. */
        if (rig_get_rit(rig, RIG_VFO_CURR, &readback) != -RIG_ENAVAIL
                || readback != 123)
        {
            status = 1;
        }

        /* The SET path must not send any extra GET or ON/OFF command. */
        if (has_input(sockets[1]) != 0) { status = 1; }

#ifdef _WIN32
        shutdown(sockets[0], SD_BOTH);
#else
        shutdown(sockets[0], SHUT_RDWR);
#endif
        pthread_join(thread, NULL);
        if (test.status != 0) { status = 1; }
    }

    cleanup_rig(rig, sockets);
    printf("%s: %s\n", name, status ? "FAIL" : "PASS");
    return status;
}

static int check_bounds_and_capabilities(void)
{
    const shortfreq_t invalid[] = { -501, 501, -9999, 9999,
                                    LONG_MIN, LONG_MAX };
    int sockets[2], status = 0;
    shortfreq_t readback = 123;
    RIG *rig;
    size_t i;

    if (open_connection(sockets) != 0) { return 1; }

    rig = prepare_rig(sockets[0]);

    if (rig == NULL)
    {
        close_socket(sockets[0]);
        close_socket(sockets[1]);
        return 1;
    }

    if (rig->caps->set_rit == NULL || rig->caps->get_rit != NULL
            || rig->caps->max_rit != 500 || rig->caps->set_xit != NULL
            || rig->caps->get_xit != NULL || ic7100_caps.max_rit != 9999
            || x6100_caps.max_rit != 9999
            || rig_get_rit(rig, RIG_VFO_CURR, &readback) != -RIG_ENAVAIL
            || readback != 123)
    {
        status = 1;
    }

    for (i = 0; i < ARRAY_SIZE(invalid); i++)
    {
        if (rig_set_rit(rig, RIG_VFO_CURR, invalid[i]) != -RIG_EINVAL)
        {
            status = 1;
        }
    }

    if (has_input(sockets[1]) != 0) { status = 1; }

    cleanup_rig(rig, sockets);
    printf("bounds, unavailable GET, no I/O or XIT: %s\n",
           status ? "FAIL" : "PASS");
    return status;
}

#define SET_RIT(value, lo, hi, sign) \
    { value, \
      { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, lo, hi, sign, 0xfd }, 10, \
      { 0xfe, 0xfe, 0xe0, 0x88, 0xfb, 0xfd }, 6, RIG_OK }

int main(void)
{
    int status = 0;
    size_t i;
    const struct exchange valid[] =
    {
        SET_RIT(0, 0x00, 0x00, 0),
        SET_RIT(100, 0x00, 0x01, 0),
        SET_RIT(-100, 0x00, 0x01, 1),
        SET_RIT(101, 0x01, 0x01, 0),
        SET_RIT(-101, 0x01, 0x01, 1),
        SET_RIT(111, 0x11, 0x01, 0),
        SET_RIT(-111, 0x11, 0x01, 1),
        SET_RIT(1, 0x01, 0x00, 0),
        SET_RIT(-1, 0x01, 0x00, 1),
        SET_RIT(-50, 0x50, 0x00, 1),
        SET_RIT(-110, 0x10, 0x01, 1),
        SET_RIT(-200, 0x00, 0x02, 1),
        SET_RIT(500, 0x00, 0x05, 0),
        SET_RIT(-500, 0x00, 0x05, 1),
        SET_RIT(0, 0x00, 0x00, 0)
    };
    const struct exchange failures[] =
    {
        { 100,
          { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, 0, 1, 0, 0xfd }, 10,
          { 0xfe, 0xfe, 0xe0, 0x88, 0xfa, 0xfd }, 6, -RIG_ERJCTED },
        { 100,
          { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, 0, 1, 0, 0xfd }, 10,
          { 0xfe, 0xfe, 0xe0, 0x88, 0xfb, 0, 0xfd }, 7, -RIG_EPROTO },
        { 100,
          { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, 0, 1, 0, 0xfd }, 10,
          { 0xfe, 0xfe, 0xe0, 0x88, 0x00, 0xfd }, 6, -RIG_EPROTO },
        { 100,
          { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, 0, 1, 0, 0xfd }, 10,
          { 0xfe, 0xfe, 0xe0, 0x88, 0xfc }, 5, -RIG_BUSBUSY },
        { 100,
          { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, 0, 1, 0, 0xfd }, 10,
          { 0xfe, 0xfe, 0xe0, 0x88, 0x21, 0, 0, 1, 0, 0xfd }, 10,
          -RIG_EPROTO },
        { 100,
          { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, 0, 1, 0, 0xfd }, 10,
          { 0xfe, 0xfe, 0xe0, 0x88, 0xfd }, 5, -RIG_EPROTO },
        { 100,
          { 0xfe, 0xfe, 0x88, 0xe0, 0x21, 0x00, 0, 1, 0, 0xfd }, 10,
          { 0 }, 0, -RIG_ETIMEOUT }
    };
#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) { return 1; }
#endif
    rig_set_debug(RIG_DEBUG_NONE);
    rig_register(&g90_caps);
    rig_register(&ic7100_caps);
    status |= check_bounds_and_capabilities();
    status |= run_case("RIT signed BCD SET/ACK and zero reset",
                       valid, ARRAY_SIZE(valid));

    for (i = 0; i < ARRAY_SIZE(failures); i++)
    {
        status |= run_case("RIT NAK/malformed ACK/collision/timeout",
                           &failures[i], 1);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return status;
}
