/*
 * Hamlib Icom optional command fallback tests
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
#include "icom.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

extern struct rig_caps ic7100_caps;
extern struct rig_caps ic7300_caps;

struct exchange
{
    const unsigned char *request;
    size_t request_len;
    const unsigned char *response;
    size_t response_len;
};

struct peer_case
{
    const char *name;
    const struct exchange *exchanges;
    size_t exchange_count;
    int fd;
    int status;
};

static const unsigned char request_x26_ic7100[] =
{
    0xfe, 0xfe, 0x88, 0xe0, 0x26, 0x00, 0xfd
};
static const unsigned char request_x26_ic7300[] =
{
    0xfe, 0xfe, 0x94, 0xe0, 0x26, 0x00, 0xfd
};
static const unsigned char request_mode[] =
{
    0xfe, 0xfe, 0x88, 0xe0, 0x04, 0xfd
};
static const unsigned char request_filter[] =
{
    0xfe, 0xfe, 0x88, 0xe0, 0x1a, 0x03, 0xfd
};
static const unsigned char request_data_mode[] =
{
    0xfe, 0xfe, 0x88, 0xe0, 0x1a, 0x06, 0xfd
};
static const unsigned char request_x25[] =
{
    0xfe, 0xfe, 0x88, 0xe0, 0x25, 0x00, 0xfd
};
static const unsigned char request_frequency[] =
{
    0xfe, 0xfe, 0x88, 0xe0, 0x03, 0xfd
};

static const unsigned char response_nak_ic7100[] =
{
    0xfe, 0xfe, 0xe0, 0x88, 0xfa, 0xfd
};
static const unsigned char response_nak_ic7300[] =
{
    0xfe, 0xfe, 0xe0, 0x94, 0xfa, 0xfd
};
static const unsigned char response_x26[] =
{
    0xfe, 0xfe, 0xe0, 0x88, 0x26, 0x00, 0x01, 0x00, 0x01, 0xfd
};
static const unsigned char response_mode[] =
{
    0xfe, 0xfe, 0xe0, 0x88, 0x04, 0x01, 0x01, 0xfd
};
static const unsigned char response_filter[] =
{
    0xfe, 0xfe, 0xe0, 0x88, 0x1a, 0x03, 0x14, 0xfd
};
static const unsigned char response_data_mode[] =
{
    0xfe, 0xfe, 0xe0, 0x88, 0x1a, 0x06, 0x01, 0xfd
};
static const unsigned char response_x25[] =
{
    0xfe, 0xfe, 0xe0, 0x88, 0x25, 0x00, 0x00, 0x40, 0x07, 0x14,
    0x00, 0xfd
};
static const unsigned char response_frequency[] =
{
    0xfe, 0xfe, 0xe0, 0x88, 0x03, 0x00, 0x40, 0x07, 0x14, 0x00,
    0xfd
};
static const unsigned char response_malformed[] = { 0x00, 0xfd };

static const struct exchange old_mode_exchanges[] =
{
    {
        request_x26_ic7100, sizeof(request_x26_ic7100),
        response_nak_ic7100, sizeof(response_nak_ic7100)
    },
    {
        request_mode, sizeof(request_mode),
        response_mode, sizeof(response_mode)
    },
    {
        request_filter, sizeof(request_filter),
        response_filter, sizeof(response_filter)
    },
    {
        request_data_mode, sizeof(request_data_mode),
        response_data_mode, sizeof(response_data_mode)
    }
};

static const struct exchange new_mode_exchanges[] =
{
    {
        request_x26_ic7100, sizeof(request_x26_ic7100),
        response_x26, sizeof(response_x26)
    },
    {
        request_filter, sizeof(request_filter),
        response_filter, sizeof(response_filter)
    }
};

static const struct exchange malformed_mode_exchanges[] =
{
    {
        request_x26_ic7100, sizeof(request_x26_ic7100),
        response_malformed, sizeof(response_malformed)
    }
};

static const struct exchange required_mode_exchanges[] =
{
    {
        request_x26_ic7300, sizeof(request_x26_ic7300),
        response_nak_ic7300, sizeof(response_nak_ic7300)
    }
};

static const struct exchange old_frequency_exchanges[] =
{
    {
        request_x25, sizeof(request_x25),
        response_nak_ic7100, sizeof(response_nak_ic7100)
    },
    {
        request_frequency, sizeof(request_frequency),
        response_frequency, sizeof(response_frequency)
    },
    {
        request_frequency, sizeof(request_frequency),
        response_frequency, sizeof(response_frequency)
    }
};

static const struct exchange new_frequency_exchanges[] =
{
    {
        request_x25, sizeof(request_x25),
        response_x25, sizeof(response_x25)
    }
};

static const struct exchange malformed_frequency_exchanges[] =
{
    {
        request_x25, sizeof(request_x25),
        response_malformed, sizeof(response_malformed)
    }
};

static int read_frame(int fd, unsigned char *frame, size_t capacity)
{
    size_t length = 0;

    while (length < capacity)
    {
        ssize_t count = read(fd, frame + length, 1);

        if (count != 1)
        {
            return -1;
        }

        if (frame[length++] == 0xfd)
        {
            return (int) length;
        }
    }

    return -1;
}

static int write_all(int fd, const unsigned char *buffer, size_t length)
{
    size_t written = 0;

    while (written < length)
    {
        ssize_t count = write(fd, buffer + written, length - written);

        if (count <= 0)
        {
            return -1;
        }

        written += (size_t) count;
    }

    return 0;
}

static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    unsigned char request[32];
    size_t i;

    test->status = -1;

    for (i = 0; i < test->exchange_count; i++)
    {
        const struct exchange *exchange = &test->exchanges[i];
        int request_len = read_frame(test->fd, request, sizeof(request));

        if (request_len != (int) exchange->request_len
                || memcmp(request, exchange->request,
                          exchange->request_len) != 0)
        {
            fprintf(stderr, "%s: unexpected request %zu\n", test->name, i + 1);
            return NULL;
        }

        if (write_all(test->fd, exchange->response,
                      exchange->response_len) != 0)
        {
            return NULL;
        }
    }

    test->status = 0;
    return NULL;
}

static RIG *prepare_rig(rig_model_t model, int fd)
{
    RIG *rig = rig_init(model);
    struct icom_priv_data *priv;

    if (rig == NULL)
    {
        return NULL;
    }

    STATE(rig)->comm_state = 1;
    STATE(rig)->current_vfo = RIG_VFO_A;
    RIGPORT(rig)->fd = fd;
    RIGPORT(rig)->retry = 0;
    RIGPORT(rig)->timeout = 100;
    priv = STATE(rig)->priv;
    priv->serial_USB_echo_off = 1;
    return rig;
}

static void release_rig(RIG *rig, int fd)
{
    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    close(fd);
    rig_cleanup(rig);
}

static int run_mode_case(const char *name, rig_model_t model,
                         const struct exchange *exchanges,
                         size_t exchange_count, int expected_retval,
                         rmode_t expected_mode)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test =
    {
        .name = name,
        .exchanges = exchanges,
        .exchange_count = exchange_count,
        .fd = -1,
        .status = -1
    };
    RIG *rig;
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = 0;
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

    rig = prepare_rig(model, sockets[0]);

    if (rig == NULL)
    {
        close(sockets[0]);
        close(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    retval = rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
    release_rig(rig, sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (test.status != 0 || retval != expected_retval
            || (retval == RIG_OK && mode != expected_mode))
    {
        fprintf(stderr, "%s: expected %d/%s, got %d/%s\n", name,
                expected_retval, rig_strrmode(expected_mode), retval,
                rig_strrmode(mode));
        return 1;
    }

    return 0;
}

static int run_frequency_case(const char *name,
                              const struct exchange *exchanges,
                              size_t exchange_count, int expected_retval)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test =
    {
        .name = name,
        .exchanges = exchanges,
        .exchange_count = exchange_count,
        .fd = -1,
        .status = -1
    };
    RIG *rig;
    freq_t frequency = 0;
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

    rig = prepare_rig(RIG_MODEL_IC7100, sockets[0]);

    if (rig == NULL)
    {
        close(sockets[0]);
        close(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    retval = rig_get_freq(rig, RIG_VFO_CURR, &frequency);
    release_rig(rig, sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (test.status != 0 || retval != expected_retval
            || (retval == RIG_OK && frequency != 14074000))
    {
        fprintf(stderr, "%s: expected %d/14074000, got %d/%.0f\n", name,
                expected_retval, retval, frequency);
        return 1;
    }

    return 0;
}

int main(void)
{
    rig_register(&ic7100_caps);
    rig_register(&ic7300_caps);

    if (run_mode_case("optional 0x26 NAK", RIG_MODEL_IC7100,
                      old_mode_exchanges, ARRAY_SIZE(old_mode_exchanges),
                      RIG_OK, RIG_MODE_PKTUSB) != 0
            || run_mode_case("supported 0x26", RIG_MODEL_IC7100,
                             new_mode_exchanges,
                             ARRAY_SIZE(new_mode_exchanges), RIG_OK,
                             RIG_MODE_USB) != 0
            || run_mode_case("malformed 0x26 reply", RIG_MODEL_IC7100,
                             malformed_mode_exchanges,
                             ARRAY_SIZE(malformed_mode_exchanges),
                             -RIG_EPROTO, RIG_MODE_NONE) != 0
            || run_mode_case("required 0x26 NAK", RIG_MODEL_IC7300,
                             required_mode_exchanges,
                             ARRAY_SIZE(required_mode_exchanges),
                             -RIG_ERJCTED, RIG_MODE_NONE) != 0)
    {
        return 1;
    }

    if (run_frequency_case("optional 0x25 NAK", old_frequency_exchanges,
                           ARRAY_SIZE(old_frequency_exchanges), RIG_OK) != 0
            || run_frequency_case("supported 0x25", new_frequency_exchanges,
                                  ARRAY_SIZE(new_frequency_exchanges),
                                  RIG_OK) != 0
            || run_frequency_case("malformed 0x25 reply",
                                  malformed_frequency_exchanges,
                                  ARRAY_SIZE(malformed_frequency_exchanges),
                                  -RIG_EPROTO) != 0)
    {
        return 1;
    }

    return 0;
}
