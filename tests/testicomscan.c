/*
 * Hamlib Icom scan command tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * icom_scan() is shared by every Icom backend.  Each scan the frontend
 * asks for has its own subcommand of 0E, so RIG_SCAN_SLCT and
 * RIG_SCAN_MEM must not put the same frame on the wire.  Driven here
 * through the IC-7300, one of the backends offering both.
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

static const unsigned char ack[] = { 0xfe, 0xfe, 0xe0, 0x94, 0xfb, 0xfd };
/* 14.100.000 Hz in little endian BCD, answering the 25 00 read */
static const unsigned char freq_reply[] =
{
    0xfe, 0xfe, 0xe0, 0x94, 0x25, 0x00, 0x00, 0x00, 0x10, 0x14, 0x00, 0xfd
};

/*
 * Records every frame and acknowledges it, so the whole sequence runs.
 * Reading the frequency needs a real answer, because rig_set_vfo() reads
 * it back and gives up on the VFO change when that read fails.
 */
static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    unsigned char frame[MAXFRAMELEN];
    size_t length;

    while (read_frame(test->fd, frame, sizeof(frame), &length) == 0)
    {
        const unsigned char *reply = ack;
        size_t reply_len = sizeof(ack);

        if (length > 4 && length <= MAX_FRAME_LEN
                && test->frame_count < MAX_RECORDED_FRAMES)
        {
            memcpy(test->frames[test->frame_count], frame, length);
            test->frame_len[test->frame_count] = length;
            test->frame_count++;
        }

        if (length > 4 && frame[4] == C_SEND_SEL_FREQ)
        {
            reply = freq_reply;
            reply_len = sizeof(freq_reply);
        }

        if (write_all(test->fd, reply, reply_len) != 0) { break; }
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
    static const unsigned char select_memory_scan[] = { 0x0e, 0x23 };
    static const unsigned char memory_scan[] = { 0x0e, 0x01 };
    RIG *rig;
    int slct_retval;
    int failed = 0;

    rig_register(&ic7300_caps);
    rig = rig_init(RIG_MODEL_IC7300);

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
    STATE(rig)->current_vfo = RIG_VFO_A;

    slct_retval = rig_scan(rig, RIG_VFO_CURR, RIG_SCAN_SLCT, 0);

    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    pthread_join(thread, NULL);

    if (slct_retval != RIG_OK)
    {
        fprintf(stderr, "select memory scan failed: %s\n",
                rigerror(slct_retval));
        failed = 1;
    }

    /* 0E 23 starts a select memory scan, 0E 01 a programmed/memory one */
    if (count_payload(&test, select_memory_scan,
                      sizeof(select_memory_scan)) != 1)
    {
        fprintf(stderr, "RIG_SCAN_SLCT did not send 0E 23\n");
        failed = 1;
    }

    if (count_payload(&test, memory_scan, sizeof(memory_scan)) != 0)
    {
        fprintf(stderr, "RIG_SCAN_SLCT sent 0E 01, the plain memory scan\n");
        failed = 1;
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);

    return failed;
}
