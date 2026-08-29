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
#include <stdatomic.h>
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
    /* the peer counts while the main thread takes marks, hence atomic */
    atomic_size_t frame_count;
    /* which band 07 D2 answers with, or -1 to reject it like the rest */
    atomic_int band_sel_answer;
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

        /*
         * 07 D2 reads the selected band, so it has to come back with a
         * band rather than a rejection for the answer to be decoded.
         */
        if (length == 7 && frame[4] == 0x07 && frame[5] == 0xd2
                && test->band_sel_answer >= 0)
        {
            unsigned char band[] = { 0xfe, 0xfe, 0xe0, 0xb2, 0x07, 0xd2, 0x00, 0xfd };

            band[6] = (unsigned char) test->band_sel_answer;

            if (write_all(test->fd, band, sizeof(band)) != 0) { break; }

            continue;
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

/*
 * Frame layout is FE FE <rig> <ctrl> <payload...> FD, so a frame is
 * identified by the bytes between the header and the terminator.
 */
static int saw_payload(const struct peer_case *test,
                       const unsigned char *payload, size_t len)
{
    size_t i;

    for (i = 0; i < test->frame_count; i++)
    {
        if (test->frame_len[i] == len + 5
                && memcmp(&test->frames[i][4], payload, len) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/*
 * Each of the calls below sends exactly one frame, so the frame recorded
 * at the mark taken before a call is that call's own and can be matched
 * whole rather than searched for.
 */
static int frame_is(const struct peer_case *test, size_t index,
                    const unsigned char *payload, size_t len)
{
    return index < test->frame_count
           && test->frame_len[index] == len + 5
           && memcmp(&test->frames[index][4], payload, len) == 0;
}

int main(void)
{
    int sockets[2];
    pthread_t thread;
    struct peer_case test = { .fd = -1, .frame_count = 0, .band_sel_answer = -1 };
    struct icom_priv_data *priv;
    static const unsigned char backlight_full[] = { 0x14, 0x19, 0x02, 0x55 };
    static const unsigned char dual_watch_on[] = { 0x07, 0xc1 };
    static const unsigned char transceive_on[] = { 0x1a, 0x05, 0x01, 0x50, 0x01 };
    static const unsigned char ant1_rx_on[] = { 0x12, 0x00, 0x01 };
    static const unsigned char voice_mem_3[] = { 0x28, 0x00, 0x03 };
    static const unsigned char voice_mem_stop[] = { 0x28, 0x00, 0x00 };
    static const unsigned char read_main_freq[] = { 0x25, 0x00 };
    static const unsigned char read_sub_freq[] = { 0x25, 0x01 };
    static const unsigned char read_main_mode[] = { 0x26, 0x00 };
    static const unsigned char read_sub_mode[] = { 0x26, 0x01 };
    static const unsigned char read_band_sel[] = { 0x07, 0xd2 };
    static const unsigned char read_ovf[] = { 0x15, 0x07 };
    static const unsigned char ip_plus_on[] = { 0x16, 0x65, 0x01 };
    powerstat_t status = RIG_POWER_OFF;
    size_t main_freq_mark, sub_freq_mark, main_mode_mark, sub_mode_mark;
    size_t band_sel_mark, ovf_mark, ip_plus_mark;
    vfo_t selected_main = RIG_VFO_NONE, selected_sub = RIG_VFO_NONE;
    int main_sel_retval, sub_sel_retval;
    int ovf_retval, ip_plus_retval;
    int ovf_status = 0;
    rmode_t mode;
    pbwidth_t width;
    freq_t freq;
    int voice_retval;
    value_t backlight;
    value_t ant_option;
    RIG *rig;
    int parm_retval;
    int failed = 0;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

#endif

    rig_register(&ic7760_caps);
    rig = rig_init(RIG_MODEL_IC7760);

    if (rig == NULL)
    {
        fprintf(stderr, "rig_init failed\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "test socket setup failed\n");
        rig_cleanup(rig);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
#ifdef _WIN32
        WSACleanup();
#endif
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
    rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, 1);
    rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_TRANSCEIVE, 1);
    ant_option.i = 1;
    rig_set_ant(rig, RIG_VFO_CURR, RIG_ANT_1, ant_option);
    voice_retval = rig_send_voice_mem(rig, RIG_VFO_CURR, 3);
    rig_stop_voice_mem(rig, RIG_VFO_CURR);

    /*
     * Commands 25 and 26 name the band outright, so the byte they carry
     * does not depend on which band is selected.  Ask for both bands
     * with Sub selected, where reading that byte as selected and
     * unselected instead comes out reversed.
     */
    STATE(rig)->current_vfo = RIG_VFO_SUB;
    main_freq_mark = test.frame_count;
    rig_get_freq(rig, RIG_VFO_MAIN, &freq);
    sub_freq_mark = test.frame_count;
    rig_get_freq(rig, RIG_VFO_SUB, &freq);
    main_mode_mark = test.frame_count;
    rig_get_mode(rig, RIG_VFO_MAIN, &mode, &width);
    sub_mode_mark = test.frame_count;
    rig_get_mode(rig, RIG_VFO_SUB, &mode, &width);

    /*
     * 07 D2 reads which band the front panel has selected, answering 00
     * for Main and 01 for Sub.  rig_get_vfo() caches its answer, so both
     * bands are only seen with the cache switched off.
     */
    rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 0);
    band_sel_mark = test.frame_count;
    test.band_sel_answer = 0x00;
    main_sel_retval = rig_get_vfo(rig, &selected_main);
    test.band_sel_answer = 0x01;
    sub_sel_retval = rig_get_vfo(rig, &selected_sub);

    /*
     * The OVF indicator is read with 15 07, and IP Plus is switched with
     * 16 65.  Both are implemented by the shared Icom code, so all the
     * rig has to do is advertise them.
     */
    ovf_mark = test.frame_count;
    ovf_retval = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_OVF_STATUS, &ovf_status);
    ip_plus_mark = test.frame_count;
    ip_plus_retval = rig_set_ext_func(rig, RIG_VFO_CURR,
                                      rig_ext_token_lookup(rig, "IPP"), 1);

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

    if (!saw_payload(&test, backlight_full, sizeof(backlight_full)))
    {
        fprintf(stderr, "set_parm BACKLIGHT did not send 14 19 02 55\n");
        failed = 1;
    }

    /* Dual watch is 07 C1 to switch on, 07 C0 off and 07 C2 to read. */
    if (!saw_payload(&test, dual_watch_on, sizeof(dual_watch_on)))
    {
        fprintf(stderr, "set_func DUAL_WATCH did not send 07 C1\n");
        failed = 1;
    }

    /* CI-V transceive is 1A 05 01 50, data 00 or 01. */
    if (!saw_payload(&test, transceive_on, sizeof(transceive_on)))
    {
        fprintf(stderr, "set_func TRANSCEIVE did not send 1A 05 01 50 01\n");
        failed = 1;
    }

    /*
     * Selecting an antenna is 12 with the antenna as its subcommand and
     * the RX antenna as its data byte, so ANT1 with the RX antenna on is
     * 12 00 01.  The rig acknowledges the three-byte form.
     */
    if (!saw_payload(&test, ant1_rx_on, sizeof(ant1_rx_on)))
    {
        fprintf(stderr, "set_ant ANT1 did not send 12 00 01\n");
        failed = 1;
    }

    /*
     * The voice memories are advertised as eight channels, so a client
     * has to be able to transmit them: 28 00 with the memory as its data
     * byte, 01 through 08, and 28 00 00 to stop.
     */
    if (voice_retval == -RIG_ENAVAIL)
    {
        fprintf(stderr, "send_voice_mem is not wired up, so the advertised"
                " voice memories cannot be transmitted\n");
        failed = 1;
    }

    if (!saw_payload(&test, voice_mem_3, sizeof(voice_mem_3)))
    {
        fprintf(stderr, "send_voice_mem 3 did not send 28 00 03\n");
        failed = 1;
    }

    if (!saw_payload(&test, voice_mem_stop, sizeof(voice_mem_stop)))
    {
        fprintf(stderr, "stop_voice_mem did not send 28 00 00\n");
        failed = 1;
    }

    /* 25 and 26 take 00 for Main and 01 for Sub. */
    if (!frame_is(&test, main_freq_mark, read_main_freq, sizeof(read_main_freq)))
    {
        fprintf(stderr, "get_freq MAIN did not send 25 00 with Sub selected\n");
        failed = 1;
    }

    if (!frame_is(&test, sub_freq_mark, read_sub_freq, sizeof(read_sub_freq)))
    {
        fprintf(stderr, "get_freq SUB did not send 25 01 with Sub selected\n");
        failed = 1;
    }

    if (!frame_is(&test, main_mode_mark, read_main_mode, sizeof(read_main_mode)))
    {
        fprintf(stderr, "get_mode MAIN did not send 26 00 with Sub selected\n");
        failed = 1;
    }

    if (!frame_is(&test, sub_mode_mark, read_sub_mode, sizeof(read_sub_mode)))
    {
        fprintf(stderr, "get_mode SUB did not send 26 01 with Sub selected\n");
        failed = 1;
    }

    /*
     * Without get_vfo wired up the backend cannot follow the front
     * panel, and dumpcaps reports the rig cannot read its VFO at all.
     */
    if (main_sel_retval == -RIG_ENAVAIL || sub_sel_retval == -RIG_ENAVAIL)
    {
        fprintf(stderr, "get_vfo is not wired up, so the Main/Sub selection"
                " cannot be read\n");
        failed = 1;
    }

    if (!frame_is(&test, band_sel_mark, read_band_sel, sizeof(read_band_sel)))
    {
        fprintf(stderr, "get_vfo did not send 07 D2\n");
        failed = 1;
    }

    if (selected_main != RIG_VFO_MAIN)
    {
        fprintf(stderr, "get_vfo read 07 D2 00 as %s, not Main\n",
                rig_strvfo(selected_main));
        failed = 1;
    }

    if (selected_sub != RIG_VFO_SUB)
    {
        fprintf(stderr, "get_vfo read 07 D2 01 as %s, not Sub\n",
                rig_strvfo(selected_sub));
        failed = 1;
    }

    if (ovf_retval == -RIG_ENAVAIL)
    {
        fprintf(stderr, "get_func OVF_STATUS was rejected by the backend\n");
        failed = 1;
    }

    if (!frame_is(&test, ovf_mark, read_ovf, sizeof(read_ovf)))
    {
        fprintf(stderr, "get_func OVF_STATUS did not send 15 07\n");
        failed = 1;
    }

    if (ip_plus_retval == -RIG_EINVAL)
    {
        fprintf(stderr, "set_ext_func IPP was rejected by the backend\n");
        failed = 1;
    }

    if (!frame_is(&test, ip_plus_mark, ip_plus_on, sizeof(ip_plus_on)))
    {
        fprintf(stderr, "set_ext_func IPP did not send 16 65 01\n");
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
