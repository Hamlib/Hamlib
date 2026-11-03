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

#define MAX_RECORDED_FRAMES 64
#define MAX_FRAME_LEN 32

struct peer_case
{
    int fd;
    unsigned char rig_addr;
    unsigned char frames[MAX_RECORDED_FRAMES][MAX_FRAME_LEN];
    size_t frame_len[MAX_RECORDED_FRAMES];
    /* the peer counts while the main thread takes marks, hence atomic */
    atomic_size_t frame_count;
    /* the band 07 D2 reports: 00 Main, 01 Sub */
    unsigned char band;
    /* set by the main thread between commands, hence atomic */
    atomic_int band_reply;
};

enum { BAND_ANSWER, BAND_REFUSE, BAND_SILENT };

/*
 * Records every frame; the frame itself is the evidence.  VFO selection
 * (command 07) is acknowledged so that rig_set_vfo() records the VFO it
 * selected, everything else is rejected.  As on the rig, only 07 D0 and
 * 07 D1 change the band that 07 D2 reports; 07 00 and 07 01 pick a VFO
 * of the band already selected.  07 D2 itself is answered, refused or
 * left unanswered, as band_reply says.
 */
static void *run_peer(void *arg)
{
    struct peer_case *test = arg;
    unsigned char frame[MAXFRAMELEN];
    unsigned char nak[6] = { 0xfe, 0xfe, CTRLID, 0x00, 0xfa, 0xfd };
    unsigned char ack[6] = { 0xfe, 0xfe, CTRLID, 0x00, 0xfb, 0xfd };
    unsigned char band[8] = { 0xfe, 0xfe, CTRLID, 0x00, C_SET_VFO, S_BAND_SEL,
                              0x00, 0xfd
                            };
    size_t length;

    nak[3] = test->rig_addr;
    ack[3] = test->rig_addr;
    band[3] = test->rig_addr;

    while (read_frame(test->fd, frame, sizeof(frame), &length) == 0)
    {
        const unsigned char *reply = frame[4] == C_SET_VFO ? ack : nak;
        size_t reply_len = sizeof(nak);

        if (length <= MAX_FRAME_LEN && test->frame_count < MAX_RECORDED_FRAMES)
        {
            memcpy(test->frames[test->frame_count], frame, length);
            test->frame_len[test->frame_count] = length;
            test->frame_count++;
        }

        if (length == 7 && frame[4] == C_SET_VFO)
        {
            if (frame[5] == S_MAIN) { test->band = 0x00; }
            else if (frame[5] == S_SUB) { test->band = 0x01; }
            else if (frame[5] == S_BAND_SEL && test->band_reply == BAND_SILENT)
            {
                continue;
            }
            else if (frame[5] == S_BAND_SEL && test->band_reply == BAND_REFUSE)
            {
                reply = nak;
            }
            else if (frame[5] == S_BAND_SEL)
            {
                band[6] = test->band;
                reply = band;
                reply_len = sizeof(band);
            }
        }

        if (write_all(test->fd, reply, reply_len) != 0) { break; }
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
    static const unsigned char read_band[] = { 0x07, 0xd2 };
    static const unsigned char read_frequency[] = { 0x03 };
    size_t vfo_b_mark, split_set_mark, split_read_mark;
    size_t sub_split_set_mark, sub_split_read_mark;
    static const struct
    {
        int reply;
        const char *name;
        int error;
    } lookups[] =
    {
        { BAND_REFUSE, "refused", -RIG_ERJCTED },
        { BAND_SILENT, "unanswered", -RIG_ETIMEOUT },
    };
    size_t lookup_set_mark[2], lookup_read_mark[2], lookup_end_mark[2];
    int lookup_set_ret[2], lookup_read_ret[2];
    vfo_t vfo_after_b = RIG_VFO_NONE, sub_vfo_after_b = RIG_VFO_NONE;
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
        rig_set_current_vfo_state(rig, cases[i].vfo);
        span.i = 50000;
        set_mark[i] = test.frame_count;
        rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, span);
        read_mark[i] = test.frame_count;
        rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, &span);
    }

    /*
     * With split on, selecting VFO B through the public API sends 07 01
     * alone and records a bare RIG_VFO_B: VFO B of whichever band the rig
     * has selected, which only the rig knows.  The scope byte has to come
     * from 07 D2 then: Main at first, Sub once SUB_B has been selected.
     */
    rig_set_current_vfo_state(rig, RIG_VFO_CURR);
    rig_set_cache_split(rig, RIG_SPLIT_ON, RIG_VFO_CURR);
    vfo_b_mark = test.frame_count;
    rig_set_vfo(rig, RIG_VFO_B);
    vfo_after_b = rig_get_current_vfo_state(rig);
    span.i = 50000;
    split_set_mark = test.frame_count;
    rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, span);
    split_read_mark = test.frame_count;
    rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, &span);

    rig_set_vfo(rig, RIG_VFO_SUB_B);
    rig_set_vfo(rig, RIG_VFO_B);
    sub_vfo_after_b = rig_get_current_vfo_state(rig);
    span.i = 50000;
    sub_split_set_mark = test.frame_count;
    rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, span);
    sub_split_read_mark = test.frame_count;
    rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_SPAN, &span);

    /*
     * Still a bare B on the Sub band: if 07 D2 is refused or not answered,
     * nothing tells which scope it is, so the scope command must fail with
     * that error instead of going to a guessed one.
     */
    for (i = 0; i < 2; i++)
    {
        test.band_reply = lookups[i].reply;
        span.i = 50000;
        lookup_set_mark[i] = test.frame_count;
        lookup_set_ret[i] = rig_set_level(rig, RIG_VFO_CURR,
                                          RIG_LEVEL_SPECTRUM_SPAN, span);
        lookup_read_mark[i] = test.frame_count;
        lookup_read_ret[i] = rig_get_level(rig, RIG_VFO_CURR,
                                           RIG_LEVEL_SPECTRUM_SPAN, &span);
        lookup_end_mark[i] = test.frame_count;
    }

    stop_test_peer(sockets, thread);

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

    if (sub_vfo_after_b != RIG_VFO_B)
    {
        fprintf(stderr, "set_vfo B after SUB_B with split on recorded %s,"
                " not VFOB\n", rig_strvfo(sub_vfo_after_b));
        failed = 1;
    }

    span_set[2] = 0x00;
    span_read[2] = 0x00;

    if (!frame_is(&test, split_set_mark, read_band, sizeof(read_band))
            || !frame_is(&test, split_set_mark + 1, span_set, sizeof(span_set)))
    {
        fprintf(stderr, "set_level SPECTRUM_SPAN CURR with VFOB selected in"
                " split on Main did not send 07 D2, then 27 15 00\n");
        failed = 1;
    }

    if (!frame_is(&test, split_read_mark, read_band, sizeof(read_band))
            || !frame_is(&test, split_read_mark + 1, span_read, sizeof(span_read)))
    {
        fprintf(stderr, "get_level SPECTRUM_SPAN CURR with VFOB selected in"
                " split on Main did not send 07 D2, then 27 15 00\n");
        failed = 1;
    }

    span_set[2] = 0x01;
    span_read[2] = 0x01;

    if (!frame_is(&test, sub_split_set_mark, read_band, sizeof(read_band))
            || !frame_is(&test, sub_split_set_mark + 1, span_set, sizeof(span_set)))
    {
        fprintf(stderr, "set_level SPECTRUM_SPAN CURR with VFOB selected in"
                " split after SUB_B did not send 07 D2, then 27 15 01\n");
        failed = 1;
    }

    if (!frame_is(&test, sub_split_read_mark, read_band, sizeof(read_band))
            || !frame_is(&test, sub_split_read_mark + 1, span_read,
                         sizeof(span_read)))
    {
        fprintf(stderr, "get_level SPECTRUM_SPAN CURR with VFOB selected in"
                " split after SUB_B did not send 07 D2, then 27 15 01\n");
        failed = 1;
    }

    for (i = 0; i < 2; i++)
    {
        size_t expected_frames = lookups[i].reply == BAND_REFUSE ? 2 : 1;
        const char *sequence = expected_frames == 2 ? "07 D2, 03" : "07 D2";

        if (lookup_set_ret[i] != lookups[i].error
                || lookup_read_mark[i] != lookup_set_mark[i] + expected_frames
                || !frame_is(&test, lookup_set_mark[i], read_band, sizeof(read_band))
                || (expected_frames == 2
                    && !frame_is(&test, lookup_set_mark[i] + 1,
                                 read_frequency, sizeof(read_frequency))))
        {
            fprintf(stderr, "set_level SPECTRUM_SPAN CURR with VFOB on Sub and"
                            " 07 D2 %s returned %d after %d frames, expected %d after"
                            " %s\n", lookups[i].name, lookup_set_ret[i],
                    (int)(lookup_read_mark[i] - lookup_set_mark[i]),
                    lookups[i].error, sequence);
            failed = 1;
        }

        if (lookup_read_ret[i] != lookups[i].error
                || lookup_end_mark[i] != lookup_read_mark[i] + expected_frames
                || !frame_is(&test, lookup_read_mark[i], read_band, sizeof(read_band))
                || (expected_frames == 2
                    && !frame_is(&test, lookup_read_mark[i] + 1,
                                 read_frequency, sizeof(read_frequency))))
        {
            fprintf(stderr, "get_level SPECTRUM_SPAN CURR with VFOB on Sub and"
                            " 07 D2 %s returned %d after %d frames, expected %d after"
                            " %s\n", lookups[i].name, lookup_read_ret[i],
                    (int)(lookup_end_mark[i] - lookup_read_mark[i]),
                    lookups[i].error, sequence);
            failed = 1;
        }
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
