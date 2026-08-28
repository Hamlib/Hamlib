/*
 * Hamlib IC-7760 CI-V transaction tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * Drives ic7760_caps over a socket pair and records the command byte of
 * every CI-V frame the backend emits, so that commands the IC-7760 does
 * not implement can be asserted against.
 *
 * Command numbers refer to the IC-7760 CI-V REFERENCE GUIDE, revision
 * A7788-8EX-2 (May 2025).
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "frame.h"
#include "testicomsock.h"

#define MAX_RECORDED_FRAMES 32
#define MAX_FRAME_LEN 32

struct peer_case
{
    int fd;
    unsigned char frames[MAX_RECORDED_FRAMES][MAX_FRAME_LEN];
    size_t frame_len[MAX_RECORDED_FRAMES];
    size_t frame_count;
};

static const unsigned char nak[] = { 0xfe, 0xfe, 0xe0, 0xb2, 0xfa, 0xfd };

/*
 * Records the command byte of every frame and rejects it, so the backend
 * exercises its full command sequence without the test having to script
 * plausible answers for each one.
 */
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

        if (write_all(test->fd, nak, sizeof(nak)) != 0) { break; }
    }

    return NULL;
}

static int saw_command(const struct peer_case *test, unsigned char command)
{
    size_t i;

    for (i = 0; i < test->frame_count; i++)
    {
        if (test->frames[i][4] == command) { return 1; }
    }

    return 0;
}

/* Frame layout is FE FE <rig> <ctrl> <command> <subcommand> ... FD */
static int saw_subcommand(const struct peer_case *test, unsigned char command,
                          unsigned char subcommand)
{
    size_t i;

    for (i = 0; i < test->frame_count; i++)
    {
        if (test->frame_len[i] > 5 && test->frames[i][4] == command
                && test->frames[i][5] == subcommand)
        {
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test = { .fd = -1, .frame_count = 0 };
    struct icom_priv_data *priv;
    powerstat_t status = RIG_POWER_OFF;
    value_t backlight;
    RIG *rig;
    int parm_retval;
    int failed = 0;

    rig_register(&ic7760_caps);
    rig = rig_init(RIG_MODEL_IC7760);

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

    rig_get_powerstat(rig, &status);

    backlight.f = 1.0f;
    parm_retval = rig_set_parm(rig, RIG_PARM_BACKLIGHT, backlight);

    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    pthread_join(thread, NULL);

    /*
     * The command table defines 18 00 and 18 01 to switch the transceiver
     * off and on; there is no form of 18 that reads the power state.
     */
    if (saw_command(&test, 0x18))
    {
        fprintf(stderr, "get_powerstat sent command 18, which the rig has no"
                " read form of\n");
        failed = 1;
    }

    if (test.frame_count == 0)
    {
        fprintf(stderr, "get_powerstat sent nothing at all\n");
        failed = 1;
    }

    /*
     * The backlight brightness is command 14 19, data 00 00 ~ 02 55.
     * rig_set_parm() must reach the rig rather than being rejected by the
     * backend for lack of a mapping.
     */
    if (parm_retval == -RIG_EINVAL)
    {
        fprintf(stderr, "set_parm BACKLIGHT was rejected by the backend\n");
        failed = 1;
    }

    if (!saw_subcommand(&test, 0x14, 0x19))
    {
        fprintf(stderr, "set_parm BACKLIGHT did not send 14 19\n");
        failed = 1;
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);

    return failed;
}
