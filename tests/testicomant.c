/*
 * Hamlib Icom antenna command tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * icom_set_ant() is shared by every Icom backend.  On a rig whose
 * antenna command carries three bytes it selects the antenna and sets
 * the RX antenna flag in one frame, so exactly one frame belongs on the
 * wire.  Driven here through the IC-7600, which is such a rig.
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "testicomsock.h"

#define MAX_RECORDED_FRAMES 16
#define MAX_FRAME_LEN 32

struct peer_case
{
    int fd;
    unsigned char frames[MAX_RECORDED_FRAMES][MAX_FRAME_LEN];
    size_t frame_len[MAX_RECORDED_FRAMES];
    size_t frame_count;
};

static const unsigned char ack[] = { 0xfe, 0xfe, 0xe0, 0x7a, 0xfb, 0xfd };

/* Records every frame and acknowledges it, so the whole sequence runs */
static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    unsigned char frame[MAXFRAMELEN];
    size_t length;

    while (read_frame(test->fd, frame, sizeof(frame), &length) == 0)
    {
        if (length > 4 && length <= MAX_FRAME_LEN
                && test->frame_count < MAX_RECORDED_FRAMES)
        {
            memcpy(test->frames[test->frame_count], frame, length);
            test->frame_len[test->frame_count] = length;
            test->frame_count++;
        }

        if (write_all(test->fd, ack, sizeof(ack)) != 0) { break; }
    }

    return NULL;
}

/*
 * Frame layout is FE FE <rig> <ctrl> <payload...> FD, so a frame is
 * identified by the bytes between the header and the terminator.
 */
static size_t count_payload(const struct peer_case *test,
                            const unsigned char *payload, size_t len)
{
    size_t i;
    size_t seen = 0;

    for (i = 0; i < test->frame_count; i++)
    {
        if (test->frame_len[i] == len + 5
                && memcmp(&test->frames[i][4], payload, len) == 0)
        {
            seen++;
        }
    }

    return seen;
}

int main(void)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test = { .fd = -1, .frame_count = 0 };
    struct icom_priv_data *priv;
    static const unsigned char ant1_rx_on[] = { 0x12, 0x00, 0x01 };
    size_t ant_frames;
    value_t option;
    RIG *rig;
    int set_retval;
    int bad_option_retval;
    int failed = 0;

    rig_register(&ic7600_caps);
    rig = rig_init(RIG_MODEL_IC7600);

    if (rig == NULL)
    {
        fprintf(stderr, "rig_init failed\n");
        return 1;
    }

    if (open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "test socket setup failed\n");
        rig_cleanup(rig);
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 250;
    RIGPORT(rig)->retry = 0;
    priv = (struct icom_priv_data *) STATE(rig)->priv;
    priv->serial_USB_echo_off = 1;
    STATE(rig)->comm_state = 1;
    STATE(rig)->current_vfo = RIG_VFO_MAIN;

    option.i = 1;
    set_retval = rig_set_ant(rig, RIG_VFO_CURR, RIG_ANT_1, option);

    /* The option byte is the RX antenna flag, so only 0 and 1 exist */
    option.i = 2;
    bad_option_retval = rig_set_ant(rig, RIG_VFO_CURR, RIG_ANT_1, option);

    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    pthread_join(thread, NULL);

    if (set_retval != RIG_OK)
    {
        fprintf(stderr, "set_ant ANT1 failed: %s\n", rigerror(set_retval));
        failed = 1;
    }

    /*
     * Selecting an antenna is 12 with the antenna as its subcommand and
     * the RX antenna as its data byte, so ANT1 with the RX antenna on is
     * 12 00 01 - one frame, carrying both.
     */
    ant_frames = count_payload(&test, ant1_rx_on, sizeof(ant1_rx_on));

    if (ant_frames != 1)
    {
        fprintf(stderr, "set_ant ANT1 sent 12 00 01 %d times, expected once\n",
                (int) ant_frames);
        failed = 1;
    }

    if (bad_option_retval != -RIG_EINVAL)
    {
        fprintf(stderr, "set_ant took an out of range RX antenna option: %s\n",
                rigerror(bad_option_retval));
        failed = 1;
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);

    return failed;
}
