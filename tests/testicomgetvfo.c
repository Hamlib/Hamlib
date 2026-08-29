/*
 * Hamlib Icom get_vfo refusal tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * When a rig refuses 07 D2, rig_get_vfo() must remember that on the
 * handle that got the refusal.  rig_caps is one static structure per
 * model, shared by every handle in the process, so writing there costs
 * every other rig its get_vfo as well.  Driven here through two
 * IC-7600s, one refusing and one answering.
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "testicomsock.h"

#define MAX_RECORDED_FRAMES 8
#define MAX_FRAME_LEN 32

struct peer_case
{
    int fd;
    unsigned char frames[MAX_RECORDED_FRAMES][MAX_FRAME_LEN];
    size_t frame_len[MAX_RECORDED_FRAMES];
    /* the peer counts while the main thread takes marks, hence atomic */
    atomic_size_t frame_count;
    /* the band 07 D2 answers with, or -1 to refuse it like the rest */
    int band_sel_answer;
};

static const unsigned char nak[] = { 0xfe, 0xfe, 0xe0, 0x7a, 0xfa, 0xfd };

/* Records every frame; answers 07 D2 with a band, refuses the rest */
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

        if (length == 7 && frame[4] == C_SET_VFO && frame[5] == S_BAND_SEL
                && test->band_sel_answer >= 0)
        {
            unsigned char band[] = { 0xfe, 0xfe, 0xe0, 0x7a, 0x07, 0xd2, 0x00, 0xfd };

            band[6] = (unsigned char) test->band_sel_answer;

            if (write_all(test->fd, band, sizeof(band)) != 0) { break; }

            continue;
        }

        if (write_all(test->fd, nak, sizeof(nak)) != 0) { break; }
    }

    return NULL;
}

/* One rig on one socket pair with one recording peer */
struct handle
{
    RIG *rig;
    int sockets[2];
    pthread_t thread;
    struct peer_case test;
};

static int open_handle(struct handle *h, int band_sel_answer)
{
    struct icom_priv_data *priv;

    h->rig = rig_init(RIG_MODEL_IC7600);

    if (h->rig == NULL) { return -1; }

    if (open_test_connection(h->sockets) != 0)
    {
        rig_cleanup(h->rig);
        return -1;
    }

    h->test.fd = h->sockets[1];
    h->test.frame_count = 0;
    h->test.band_sel_answer = band_sel_answer;

    if (pthread_create(&h->thread, NULL, run_peer, &h->test) != 0)
    {
        close_test_socket(h->sockets[0]);
        close_test_socket(h->sockets[1]);
        rig_cleanup(h->rig);
        return -1;
    }

    RIGPORT(h->rig)->fd = h->sockets[0];
    RIGPORT(h->rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(h->rig)->timeout = 250;
    RIGPORT(h->rig)->retry = 0;
    priv = (struct icom_priv_data *) STATE(h->rig)->priv;
    priv->serial_USB_echo_off = 1;
    STATE(h->rig)->comm_state = 1;
    STATE(h->rig)->current_vfo = RIG_VFO_MAIN;
    rig_set_cache_timeout_ms(h->rig, HAMLIB_CACHE_ALL, 0);
    return 0;
}

static void close_handle(struct handle *h)
{
    close_test_socket(h->sockets[0]);
    close_test_socket(h->sockets[1]);
    pthread_join(h->thread, NULL);
    STATE(h->rig)->comm_state = 0;
    RIGPORT(h->rig)->fd = -1;
    rig_cleanup(h->rig);
}

int main(void)
{
    struct handle refused = { .rig = NULL };
    struct handle answered = { .rig = NULL };
    vfo_t vfo = RIG_VFO_NONE;
    size_t quiet_mark;
    int first_retval, again_retval, other_retval;
    int failed = 0;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

#endif

    rig_register(&ic7600_caps);

    if (open_handle(&refused, -1) != 0)
    {
        fprintf(stderr, "test setup failed\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (open_handle(&answered, 0x01) != 0)
    {
        fprintf(stderr, "test setup failed\n");
        close_handle(&refused);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    first_retval = rig_get_vfo(refused.rig, &vfo);

    /* The refusal may not leak into the caps every handle shares */
    if (ic7600_caps.get_vfo == NULL)
    {
        fprintf(stderr, "one refused 07 D2 cleared get_vfo in ic7600_caps,"
                " which every IC-7600 in the process shares\n");
        failed = 1;
    }

    /* The other handle keeps asking and being answered */
    other_retval = rig_get_vfo(answered.rig, &vfo);

    if (other_retval != RIG_OK || vfo != RIG_VFO_SUB)
    {
        fprintf(stderr, "the answering rig's get_vfo came back %s with %s, so"
                " the refusal spilled over to another handle\n",
                rigerror(other_retval), rig_strvfo(vfo));
        failed = 1;
    }

    if (answered.test.frame_count == 0)
    {
        fprintf(stderr, "the answering rig was never asked 07 D2\n");
        failed = 1;
    }

    /* The refused handle itself is not asked again */
    quiet_mark = refused.test.frame_count;
    again_retval = rig_get_vfo(refused.rig, &vfo);

    if (again_retval != -RIG_ENAVAIL || refused.test.frame_count != quiet_mark)
    {
        fprintf(stderr, "after one refusal the same handle got %s and %d more"
                " frame(s), expected ENAVAIL and silence\n",
                rigerror(again_retval),
                (int)(refused.test.frame_count - quiet_mark));
        failed = 1;
    }

    /* The first refusal answers RIG_OK with VFO_A, as it always has */
    if (first_retval != RIG_OK)
    {
        fprintf(stderr, "the refused get_vfo returned %s, expected the"
                " long-standing RIG_OK with VFO_A\n", rigerror(first_retval));
        failed = 1;
    }

    close_handle(&answered);
    close_handle(&refused);

#ifdef _WIN32
    WSACleanup();
#endif

    return failed;
}
