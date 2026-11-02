/*
 * G90 AF level response regression tests.
 * Socket setup follows testicomfallback.c (Hamlib Team, 2026).
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
#include "icom.h"
#include "misc.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

extern struct rig_caps g90_caps;
extern struct rig_caps ic7100_caps;

struct exchange
{
    int set_value; /* -1 means GET; otherwise a raw AF SET level. */
    setting_t level;
    unsigned char request[16];
    size_t request_len;
    unsigned char response[16];
    size_t response_len;
    int expected_retval;
    int expected_value;
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
            fprintf(stderr, "unexpected CI-V request at step %zu\n", i);
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

static int run_case(const char *name, rig_model_t model,
                    int quirk, const struct exchange *exchanges, size_t count)
{
    int sockets[2], status = 0;
    pthread_t thread;
    struct peer_case test = { exchanges, count, -1, -1 };
    RIG *rig;
    size_t i;

    if (open_connection(sockets) != 0) { return 1; }

    rig = rig_init(model);

    if (rig == NULL)
    {
        close_socket(sockets[0]);
        close_socket(sockets[1]);
        return 1;
    }

    if (model == RIG_MODEL_G90
            && rig_set_conf(rig, rig_token_lookup(rig, "g90_af_quirk"),
                            quirk ? "1" : "0") != RIG_OK)
    {
        close_socket(sockets[0]);
        close_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    STATE(rig)->comm_state = 1;
    rig_set_current_vfo_state(rig, RIG_VFO_A);
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->retry = 0;
    RIGPORT(rig)->timeout = 100;
    ((struct icom_priv_data *) STATE(rig)->priv)->serial_USB_echo_off = 1;
    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        status = 1;
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            const struct exchange *exchange = &exchanges[i];
            value_t value = { .f = -1.0f };
            int retval;

            /* Invoke the backend, avoiding frontend cache effects. */
            if (exchange->set_value >= 0)
            {
                value.f = exchange->set_value / 255.0f;
                retval = rig->caps->set_level(rig, RIG_VFO_CURR,
                                              exchange->level, value);
            }
            else
            {
                retval = rig->caps->get_level(rig, RIG_VFO_CURR,
                                              exchange->level, &value);
            }

            if (retval != exchange->expected_retval
                    || (retval == RIG_OK && exchange->set_value < 0
                        && fabsf(value.f - exchange->expected_value / 255.0f)
                        > 0.00001f)
                    || (retval != RIG_OK && exchange->set_value < 0
                        && value.f != -1.0f))
            {
                fprintf(stderr, "%s step %zu: expected %d/%d, got %d/%.3f\n",
                        name, i, exchange->expected_retval,
                        exchange->expected_value, retval, value.f * 255.0f);
                status = 1;
            }
        }

        /* Unblock the peer even if a request could not be sent. */
#ifdef _WIN32
        shutdown(sockets[0], SD_BOTH);
#else
        shutdown(sockets[0], SHUT_RDWR);
#endif
        pthread_join(thread, NULL);
        if (test.status != 0) { status = 1; }
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    close_socket(sockets[0]);
    close_socket(sockets[1]);
    rig_cleanup(rig);
    printf("%s: %s\n", name, status ? "FAIL" : "PASS");
    return status;
}

#define GET_AF(hi, lo, ret, value) \
    { -1, RIG_LEVEL_AF, \
      { 0xfe, 0xfe, 0x88, 0xe0, 0x14, 0x01, 0xfd }, 7, \
      { 0xfe, 0xfe, 0xe0, 0x88, 0x14, 0x01, hi, lo, 0xfd }, 9, \
      ret, value }

#define SET_AF(value, hi, lo) \
    { value, RIG_LEVEL_AF, \
      { 0xfe, 0xfe, 0x88, 0xe0, 0x14, 0x01, hi, lo, 0xfd }, 9, \
      { 0xfe, 0xfe, 0xe0, 0x88, 0xfb, 0xfd }, 6, RIG_OK, 0 }

static int check_configuration(void)
{
    RIG *first = rig_init(RIG_MODEL_G90);
    RIG *second = rig_init(RIG_MODEL_G90);
    RIG *other = rig_init(RIG_MODEL_IC7100);
    char buffer[128];
    hamlib_token_t token;
    int status = 0;

    if (first == NULL || second == NULL || other == NULL)
    {
        if (first != NULL) { rig_cleanup(first); }
        if (second != NULL) { rig_cleanup(second); }
        if (other != NULL) { rig_cleanup(other); }

        return 1;
    }

    token = rig_token_lookup(first, "g90_af_quirk");

    if (rig_get_conf2(first, token, buffer, sizeof(buffer)) != RIG_OK
            || strcmp(buffer, "0") != 0
            || rig_set_conf(first, token, "1") != RIG_OK
            || rig_get_conf2(first, token, buffer, sizeof(buffer)) != RIG_OK
            || strcmp(buffer, "1") != 0
            || rig_get_conf2(second, token, buffer, sizeof(buffer)) != RIG_OK
            || strcmp(buffer, "0") != 0
            || rig_set_conf(first, token, "2") != -RIG_EINVAL
            || rig_set_conf(first, token, "garbage") != -RIG_EINVAL
            || rig_set_conf(other, token, "1") != -RIG_EINVAL
            || rig_set_conf(first, token, "0") != RIG_OK)
    {
        status = 1;
    }

    rig_cleanup(first);
    rig_cleanup(second);
    rig_cleanup(other);
    printf("configuration and per-instance isolation: %s\n",
           status ? "FAIL" : "PASS");
    return status;
}

int main(void)
{
    int status = 0;
    size_t i;
    const struct exchange valid[] =
    {
        GET_AF(0x00, 0x00, RIG_OK, 0),
        GET_AF(0x01, 0x38, RIG_OK, 138),
        GET_AF(0x01, 0x63, RIG_OK, 163),
        GET_AF(0x02, 0x55, RIG_OK, 255)
    };
    /* Literal recorded codes and independently inferred candidate values.
     * This fixture does not generate expected GET values from the last SET.
     */
    const struct exchange quirk_values[] =
    {
        GET_AF(0x00, 0x00, RIG_OK, 0),
        GET_AF(0x00, 0x09, RIG_OK, 9),
        GET_AF(0x00, 0x12, RIG_OK, 18),
        GET_AF(0x00, 0x2b, RIG_OK, 27),
        GET_AF(0x00, 0x34, RIG_OK, 36),
        GET_AF(0x00, 0x4d, RIG_OK, 45),
        GET_AF(0x00, 0x56, RIG_OK, 54),
        GET_AF(0x00, 0x6f, RIG_OK, 63),
        GET_AF(0x00, 0x78, RIG_OK, 72),
        GET_AF(0x00, 0x81, RIG_OK, 81),
        GET_AF(0x00, 0x9b, RIG_OK, 91),
        GET_AF(0x01, 0x04, RIG_OK, 100),
        GET_AF(0x01, 0x0d, RIG_OK, 109),
        GET_AF(0x01, 0x16, RIG_OK, 118),
        GET_AF(0x01, 0x2f, RIG_OK, 127),
        GET_AF(0x01, 0x38, RIG_OK, 136),
        GET_AF(0x01, 0x41, RIG_OK, 145),
        GET_AF(0x01, 0x5a, RIG_OK, 154),
        GET_AF(0x01, 0x63, RIG_OK, 163),
        GET_AF(0x01, 0x7d, RIG_OK, 173),
        GET_AF(0x01, 0x86, RIG_OK, 182),
        GET_AF(0x01, 0x9f, RIG_OK, 191),
        GET_AF(0x02, 0x08, RIG_OK, 200),
        GET_AF(0x02, 0x01, RIG_OK, 209),
        GET_AF(0x02, 0x1a, RIG_OK, 218),
        GET_AF(0x02, 0x23, RIG_OK, 227),
        GET_AF(0x02, 0x3c, RIG_OK, 236),
        GET_AF(0x02, 0x45, RIG_OK, 245),
        GET_AF(0x02, 0x5f, RIG_OK, 255)
    };
    const struct exchange invalid[] =
    {
        GET_AF(0x00, 0x0f, -RIG_EPROTO, 0), /* No units digit <=9. */
        GET_AF(0x01, 0xa0, -RIG_EPROTO, 0), /* Invalid decimal tens. */
        GET_AF(0x03, 0x00, -RIG_EPROTO, 0), /* Invalid hundreds. */
        GET_AF(0xff, 0xff, -RIG_EPROTO, 0),
        GET_AF(0x02, 0x59, -RIG_EPROTO, 0)  /* Outside candidate range. */
    };
    const struct exchange observed[] =
    {
        GET_AF(0x00, 0x00, RIG_OK, 0),
        SET_AF(172, 0x01, 0x72),
        GET_AF(0x01, 0x63, RIG_OK, 163),
        SET_AF(192, 0x01, 0x92),
        GET_AF(0x01, 0x9f, RIG_OK, 191),
        SET_AF(200, 0x02, 0x00),
        GET_AF(0x01, 0x9f, RIG_OK, 191),
        SET_AF(208, 0x02, 0x08),
        GET_AF(0x02, 0x08, RIG_OK, 200),
        SET_AF(224, 0x02, 0x24),
        GET_AF(0x02, 0x1a, RIG_OK, 218),
        SET_AF(255, 0x02, 0x55),
        GET_AF(0x02, 0x5f, RIG_OK, 255),
        SET_AF(0, 0x00, 0x00),
        GET_AF(0x00, 0x00, RIG_OK, 0)
    };
    const struct exchange other_model[] =
    {
        GET_AF(0x01, 0x9f, RIG_OK, 205)
    };
    const struct exchange short_reply =
    {
        -1, RIG_LEVEL_AF,
        { 0xfe, 0xfe, 0x88, 0xe0, 0x14, 0x01, 0xfd }, 7,
        { 0xfe, 0xfe, 0xe0, 0x88, 0x14, 0x01, 0x01, 0xfd }, 8,
        -RIG_EPROTO, 0
    };
    const struct exchange nak =
    {
        -1, RIG_LEVEL_AF,
        { 0xfe, 0xfe, 0x88, 0xe0, 0x14, 0x01, 0xfd }, 7,
        { 0xfe, 0xfe, 0xe0, 0x88, 0xfa, 0xfd }, 6,
        -RIG_ERJCTED, 0
    };
    const struct exchange rf_power =
    {
        -1, RIG_LEVEL_RFPOWER,
        { 0xfe, 0xfe, 0x88, 0xe0, 0x14, 0x0a, 0xfd }, 7,
        { 0xfe, 0xfe, 0xe0, 0x88, 0x14, 0x0a, 0x02, 0x5f, 0xfd }, 9,
        RIG_OK, 255
    };
#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) { return 1; }
#endif
    rig_register(&g90_caps);
    rig_register(&ic7100_caps);
    status |= check_configuration();
    status |= run_case("default AF BCD unchanged", RIG_MODEL_G90, 0,
                       valid, ARRAY_SIZE(valid));
    status |= run_case("opt-in recorded AF codes", RIG_MODEL_G90, 1,
                       quirk_values, ARRAY_SIZE(quirk_values));

    for (i = 0; i < ARRAY_SIZE(invalid); i++)
    {
        status |= run_case("invalid opt-in AF response", RIG_MODEL_G90, 1,
                           &invalid[i], 1);
    }

    status |= run_case("SET/ACK/GET and restore", RIG_MODEL_G90, 1,
                       observed, ARRAY_SIZE(observed));
    status |= run_case("short AF response", RIG_MODEL_G90, 1, &short_reply, 1);
    status |= run_case("AF NAK", RIG_MODEL_G90, 1, &nak, 1);
    status |= run_case("RFPOWER quirk unchanged", RIG_MODEL_G90, 1,
                       &rf_power, 1);
    status |= run_case("default non-BCD behavior unchanged", RIG_MODEL_G90, 0,
                       other_model, ARRAY_SIZE(other_model));
    status |= run_case("other model unchanged", RIG_MODEL_IC7100, 0,
                       other_model, ARRAY_SIZE(other_model));
#ifdef _WIN32
    WSACleanup();
#endif
    return status;
}
