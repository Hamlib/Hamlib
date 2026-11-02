/*
 * Hamlib rig_send_raw capacity tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/riglist.h"
#include "hamlib/port.h"

static int test_dummy_reply(void)
{
    unsigned char request[255];
    unsigned char reply[256];
    RIG *rig;
    int count;

    for (size_t i = 0; i < sizeof(request); ++i)
    {
        request[i] = (unsigned char)i;
    }

    rig = rig_init(RIG_MODEL_DUMMY);

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize Dummy rig\n");
        return 1;
    }

    count = rig_send_raw(rig, request, sizeof(request), reply, sizeof(reply),
                         NULL);
    rig_cleanup(rig);

    if (count != sizeof(request) || memcmp(request, reply, count) != 0)
    {
        fprintf(stderr, "expected a %zu-byte echoed reply, got %d\n",
                sizeof(request), count);
        return 1;
    }

    return 0;
}

#ifndef _WIN32
static int test_transport_reply(const char *name,
                                const unsigned char *response,
                                size_t response_len, int reply_len,
                                unsigned char *term, int expected)
{
    int sockets[2];
    unsigned char reply[514];
    size_t written = 0;
    RIG *rig;
    int count;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return 1;
    }

    rig = rig_init(RIG_MODEL_FT991);

    if (rig == NULL)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->rig = rig;
    RIGPORT(rig)->timeout = 50;
    RIGPORT(rig)->retry = 0;

    while (written < response_len)
    {
        ssize_t result = write(sockets[1], response + written,
                               response_len - written);

        if (result <= 0)
        {
            RIGPORT(rig)->fd = -1;
            rig_cleanup(rig);
            close(sockets[0]);
            close(sockets[1]);
            return 1;
        }

        written += (size_t)result;
    }

    memset(reply, 0xa5, sizeof(reply));
    count = rig_send_raw(rig, (const unsigned char *)"FA;", 3,
                         reply, reply_len, term);
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    close(sockets[1]);

    if (count != expected
            || (count > 0 && memcmp(response, reply, (size_t)count) != 0))
    {
        fprintf(stderr, "%s: expected %d bytes, got %d\n",
                name, expected, count);
        return 1;
    }

    for (size_t i = count > 0 ? (size_t)count : 0; i < sizeof(reply); i++)
    {
        if (reply[i] != 0xa5)
        {
            fprintf(stderr, "%s: modified reply byte %zu\n", name, i);
            return 1;
        }
    }

    return 0;
}

static int test_transport_replies(void)
{
    static const unsigned char response[] = "FA014074000;";
    static const unsigned char icom_response[] =
    {
        0xfe, 0xfe, 0xe0, 0x94, 0x03, 0, 0, 0, 0, 0, 0xfd
    };
    unsigned char term[] = ";";
    unsigned char icom_term[] = {0xfd};
    unsigned char large_response[512];
    int failed = 0;

    failed |= test_transport_reply("terminated reply exceeds capacity",
                                   response, sizeof(response) - 1, 8, term,
                                   -RIG_EINVAL);
    failed |= test_transport_reply("terminated reply fills capacity",
                                   response, sizeof(response) - 1, 12, term,
                                   -RIG_EINVAL);
    failed |= test_transport_reply("terminated reply fits capacity",
                                   response, sizeof(response) - 1, 13, term, 12);

    memset(large_response, 'X', sizeof(large_response));
    large_response[254] = ';';
    failed |= test_transport_reply("large terminated reply",
                                   large_response, 255, 256, term, 255);
    failed |= test_transport_reply("large binary reply",
                                   large_response, 255, 256, NULL, 255);
    failed |= test_transport_reply("short binary reply times out",
                                   response, 5, 256, NULL, -RIG_ETIMEOUT);

    large_response[254] = 'X';
    large_response[299] = ';';
    failed |= test_transport_reply("large reply exceeds capacity",
                                   large_response, 300, 256, term, -RIG_EINVAL);

    large_response[299] = 'X';
    large_response[511] = ';';
    failed |= test_transport_reply("reply exceeds binding capacity",
                                   large_response, sizeof(large_response),
                                   513, term, 512);
    failed |= test_transport_reply("Icom reply exceeds capacity",
                                   icom_response, sizeof(icom_response),
                                   5, icom_term, -RIG_EINVAL);
    failed |= test_transport_reply("Icom reply fits capacity",
                                   icom_response, sizeof(icom_response),
                                   12, icom_term, 11);
    return failed;
}
#endif

int main(void)
{
    int failed = test_dummy_reply();

#ifndef _WIN32
    failed |= test_transport_replies();
#endif

    return failed;
}
