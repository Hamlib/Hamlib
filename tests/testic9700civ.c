/*
 * Frame test for the IC-9700's spectrum scope addressing.
 *
 * The IC-9700 has a Main and a Sub receiver, each with its own VFO A
 * and B, so the selected VFO can be MAIN_A, MAIN_B, SUB_A or SUB_B.
 * The scope commands take 00 for the Main scope and 01 for the Sub
 * scope: the byte follows the receiver, never the VFO letter.  A fake
 * rig on a socket pair records every frame the backend sends.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "cache.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "testicomsock.h"

#define MAX_RECORDED_FRAMES 32
#define MAX_FRAME_LEN 32

struct peer_case
{
    int fd;
    unsigned char rig_addr;
    unsigned char frames[MAX_RECORDED_FRAMES][MAX_FRAME_LEN];
    size_t frame_len[MAX_RECORDED_FRAMES];
    /* the peer counts while the main thread takes marks, hence atomic */
    atomic_size_t frame_count;
};

/*
 * Records every frame; the frame itself is the evidence.  VFO selection
 * (command 07) is acknowledged so that rig_set_vfo() records the VFO it
 * selected, everything else is rejected.
 */
static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    unsigned char frame[MAXFRAMELEN];
    unsigned char nak[6] = { 0xfe, 0xfe, CTRLID, 0x00, 0xfa, 0xfd };
    unsigned char ack[6] = { 0xfe, 0xfe, CTRLID, 0x00, 0xfb, 0xfd };
    size_t length;

    nak[3] = test->rig_addr;
    ack[3] = test->rig_addr;

    while (read_frame(test->fd, frame, sizeof(frame), &length) == 0)
    {
        if (length <= MAX_FRAME_LEN && test->frame_count < MAX_RECORDED_FRAMES)
        {
            memcpy(test->frames[test->frame_count], frame, length);
            test->frame_len[test->frame_count] = length;
            test->frame_count++;
        }

        if (write_all(test->fd, frame[4] == C_SET_VFO ? ack : nak,
                      sizeof(nak)) != 0) { break; }
    }

    return NULL;
}

/* Frame layout is FE FE <rig> <ctrl> <payload...> FD. */
static int frame_is(const struct peer_case *test, size_t index,
                    const unsigned char *payload, size_t len)
{
    return index < test->frame_count
           && test->frame_len[index] == len + 5
           && memcmp(&test->frames[index][4], payload, len) == 0;
}

int main(void)
{
    static const struct
    {
        vfo_t vfo;
        const char *name;
        unsigned char scope;
    } cases[] =
    {
        { RIG_VFO_MAIN_A, "MAIN_A", 0x00 },
        { RIG_VFO_MAIN_B, "MAIN_B", 0x00 },
        { RIG_VFO_SUB_A,  "SUB_A",  0x01 },
        { RIG_VFO_SUB_B,  "SUB_B",  0x01 },
    };
    /* 27 15 <scope> <span/2 in BCD>: 50 kHz goes on the wire as 25 kHz */
    unsigned char span_set[] = { 0x27, 0x15, 0x00, 0x00, 0x50, 0x02, 0x00, 0x00 };
    unsigned char span_read[] = { 0x27, 0x15, 0x00 };
    size_t set_mark[4], read_mark[4];
    static const unsigned char select_vfo_b[] = { 0x07, 0x01 };
    size_t vfo_b_mark, split_set_mark, split_read_mark;
    vfo_t vfo_after_b = RIG_VFO_NONE;
    int sockets[2];
    pthread_t thread;
    struct peer_case test = { .fd = -1, .frame_count = 0 };
    struct icom_priv_data *priv;
    value_t span;
    RIG *rig;
    size_t i;
    int failed = 0;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

#endif

    rig_register(&ic9700_caps);
    rig = rig_init(RIG_MODEL_IC9700);

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
    test.rig_addr = ((const struct icom_priv_caps *) ic9700_caps.priv)->re_civ_addr;

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

    for (i = 0; i < 4; i++)
    {
        STATE(rig)->current_vfo = cases[i].vfo;
        span.i = 50000;
        set_mark[i] = test.frame_count;
        rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, span);
        read_mark[i] = test.frame_count;
        rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, &span);
    }

    /*
     * With split on, selecting VFO B through the public API lands on the
     * Main receiver's VFO B and is recorded as a bare RIG_VFO_B, since
     * the rig had not named a receiver yet.  A bare VFO letter is a Main
     * band VFO on this rig; only Main/Sub-only rigs know B as Sub.
     */
    STATE(rig)->current_vfo = RIG_VFO_CURR;
    CACHE(rig)->split = RIG_SPLIT_ON;
    vfo_b_mark = test.frame_count;
    rig_set_vfo(rig, RIG_VFO_B);
    vfo_after_b = STATE(rig)->current_vfo;
    span.i = 50000;
    split_set_mark = test.frame_count;
    rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, span);
    split_read_mark = test.frame_count;
    rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, &span);

    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    pthread_join(thread, NULL);

    /*
     * rig_set_vfo() ends with a frequency read the peer refuses, so its
     * return value says nothing here; the selection frame and the VFO it
     * recorded do.
     */
    if (!frame_is(&test, vfo_b_mark, select_vfo_b, sizeof(select_vfo_b))
            || vfo_after_b != RIG_VFO_B)
    {
        size_t f, b;

        fprintf(stderr, "set_vfo B with split on did not select VFO B with"
                " 07 01 and record it as VFOB (current %s)\n",
                rig_strvfo(vfo_after_b));

        for (f = vfo_b_mark; f < split_set_mark && f < test.frame_count; f++)
        {
            fprintf(stderr, "  sent:");

            for (b = 0; b < test.frame_len[f]; b++)
            {
                fprintf(stderr, " %02x", test.frames[f][b]);
            }

            fprintf(stderr, "\n");
        }

        failed = 1;
    }

    span_set[2] = 0x00;
    span_read[2] = 0x00;

    if (!frame_is(&test, split_set_mark, span_set, sizeof(span_set)))
    {
        fprintf(stderr, "set_level SPECTRUM_SPAN CURR with VFOB selected in"
                " split did not send 27 15 00\n");
        failed = 1;
    }

    if (!frame_is(&test, split_read_mark, span_read, sizeof(span_read)))
    {
        fprintf(stderr, "get_level SPECTRUM_SPAN CURR with VFOB selected in"
                " split did not send 27 15 00\n");
        failed = 1;
    }

    for (i = 0; i < 4; i++)
    {
        span_set[2] = cases[i].scope;
        span_read[2] = cases[i].scope;

        if (!frame_is(&test, set_mark[i], span_set, sizeof(span_set)))
        {
            fprintf(stderr, "set_level SPECTRUM_SPAN CURR with %s selected did"
                    " not send 27 15 %02X\n", cases[i].name, cases[i].scope);
            failed = 1;
        }

        if (!frame_is(&test, read_mark[i], span_read, sizeof(span_read)))
        {
            fprintf(stderr, "get_level SPECTRUM_SPAN CURR with %s selected did"
                    " not send 27 15 %02X\n", cases[i].name, cases[i].scope);
            failed = 1;
        }
    }

    rig_cleanup(rig);
#ifdef _WIN32
    WSACleanup();
#endif
    return failed;
}
