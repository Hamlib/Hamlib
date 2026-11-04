/*
 * Hamlib Xiegu G90 SWR meter tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * The G90 sometimes answers the SWR meter query (15 12) with a byte
 * that is not BCD, e.g. 02 2c.  Decoding it anyway reports an SWR near
 * the top of the calibration table, so such a reply must be rejected
 * while valid readings still go through the calibration table.
 */

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "frame.h"
#include "misc.h"
#include "testicomsock.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

extern struct rig_caps g90_caps;

struct swr_case
{
    unsigned char hi, lo;
    int expected_retval;
    float expected_swr;
};

static const struct swr_case cases[] =
{
    { 0x00, 0x00, RIG_OK, 1.0f },
    { 0x01, 0x84, RIG_OK, 4.6f },
    { 0x02, 0x2c, -RIG_EPROTO, 0 }, /* low nibble 0xc, seen in #2257 */
    { 0x01, 0xa0, -RIG_EPROTO, 0 },
    { 0x0a, 0x00, -RIG_EPROTO, 0 }
};

struct peer_case
{
    int fd;
    int bad_request;
};

static void *run_peer(void *arg)
{
    static const unsigned char query[] =
    {
        0xfe, 0xfe, 0x88, 0xe0, 0x15, 0x12, 0xfd
    };
    struct peer_case *test = arg;
    unsigned char frame[MAXFRAMELEN];
    size_t i, length;

    for (i = 0; i < ARRAY_SIZE(cases); i++)
    {
        unsigned char reply[] =
        {
            0xfe, 0xfe, 0xe0, 0x88, 0x15, 0x12, cases[i].hi, cases[i].lo, 0xfd
        };

        if (read_frame(test->fd, frame, sizeof(frame), &length) != 0)
        {
            break;
        }

        if (length != sizeof(query) || memcmp(frame, query, length) != 0)
        {
            test->bad_request = 1;
        }

        if (write_all(test->fd, reply, sizeof(reply)) != 0) { break; }
    }

    return NULL;
}

int main(void)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test = { -1, 0 };
    RIG *rig;
    size_t i;
    int failed = 0;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) { return 1; }

#endif

    rig_register(&g90_caps);
    rig = rig_init(RIG_MODEL_G90);

    if (rig == NULL || open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "test setup failed\n");
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 250;
    RIGPORT(rig)->retry = 0;
    ((struct icom_priv_data *) STATE(rig)->priv)->serial_USB_echo_off = 1;
    STATE(rig)->comm_state = 1;
    rig_set_current_vfo_state(rig, RIG_VFO_A);

    for (i = 0; i < ARRAY_SIZE(cases); i++)
    {
        value_t val = { .f = -1.0f };
        int retval = rig->caps->get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SWR,
                                          &val);

        if (retval != cases[i].expected_retval
                || (retval == RIG_OK
                    && fabsf(val.f - cases[i].expected_swr) > 0.001f))
        {
            fprintf(stderr, "SWR reply %02x %02x: expected %d/%.2f, got %d/%.2f\n",
                    cases[i].hi, cases[i].lo, cases[i].expected_retval,
                    cases[i].expected_swr, retval, val.f);
            failed = 1;
        }
    }

    stop_test_peer(sockets, thread);

    if (test.bad_request)
    {
        fprintf(stderr, "unexpected SWR query frame\n");
        failed = 1;
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);

#ifdef _WIN32
    WSACleanup();
#endif

    return failed;
}
