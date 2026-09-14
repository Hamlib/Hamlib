/*
 *  Hamlib Icom network radio mock, shared by the network test suites
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* In-process mock of an Icom network radio: three UDP sockets on ephemeral
 * ports plus a thread that answers the handshake, so the session and backend
 * layers can be exercised end to end without hardware. Shared by the session
 * and stream test binaries. */

#ifndef _ICOM_NETWORK_MOCK_H
#define _ICOM_NETWORK_MOCK_H 1

#include <stdint.h>
#include <pthread.h>
#include <hamlib/rig.h>
/* Socket headers come from stream_proto.h, which picks the right set for the
 * host; do not include them directly. */
#include "../src/stream_proto.h"

#define MOCK_SERVER_ID 0xA1A2A3A4u

/* Every rate the protocol can express, which is what an IC-7610 advertises. */
#define MOCK_ALL_RATES 0x8b01

struct mock_server
{
    int ctrl_fd;
    int civ_fd;
    int audio_fd;
    uint16_t ctrl_port;
    uint16_t civ_port;
    uint16_t audio_port;
    pthread_t thread;
    volatile int stop;
    volatile int saw_civ_cmd;
    volatile int send_spectrum;  /* emit a spectrum frame before each response */
    volatile int stale_nak;      /* flush a stale NAK at CI-V stream-open */
    /* Advance the CI-V sequence by this much before the next response, to
     * simulate a burst of loss larger than the replay window. */
    volatile int civ_sequence_jump;

    /* Unsolicited traffic the radio can send that the client must answer.
     * The client address is captured from the CI-V stream-open packet. */
    struct sockaddr_in civ_peer;
    socklen_t civ_peer_length;
    uint32_t civ_client_id;
    /* The control socket has its own client address; unsolicited control
     * traffic must go there, not to the CI-V peer. */
    struct sockaddr_in ctrl_peer;
    socklen_t ctrl_peer_length;
    uint32_t ctrl_client_id;
    volatile int ask_retransmit;     /* send a retransmit request for below */
    volatile int retransmit_sequence;
    volatile int saw_retransmit_reply;
    /* Report audio_port 0 in the status reply, as a radio with no audio
     * path would. Makes the backend's audio_start fail after stream_open
     * has already allocated its buffers. */
    volatile int no_audio_port;
    /* Go silent: keep the sockets bound but stop replying, as a radio that
     * lost power or fell off the network does. */
    volatile int go_silent;
    /* Send an unsolicited status with the disconnect flag, as a radio does
     * when another client takes the session. */
    volatile int announce_disconnect;
    volatile int ask_ping;           /* send a ping *request* (reply flag 0) */
    volatile int saw_ping_reply;
    /* Advertised radio list. Defaults to one IC-7610 offering every rate the
     * protocol can express, matching what real hardware reports. */
    int radio_count;
    struct
    {
        const char *name;
        uint8_t civ;
        uint16_t rx_rate, tx_rate;
    } radios[4];
    /* Copy of the connection-info request the client sent, so a test can check
     * which radio it selected and whether it asked for a TX audio path. */
    volatile int saw_connection_info;
    uint8_t connection_info[0x90];
    uint16_t civ_sequence;
    uint16_t audio_sequence;
};

/* Canned CI-V and audio payloads the mock replies with. Sizes are spelled out
 * because the tests take sizeof() on them. */
extern const uint8_t mock_freq_resp[11];
extern const uint8_t mock_spectrum[11];
extern const uint8_t mock_audio[16];

uint16_t bind_ephemeral(int *out_fd);
void mock_start(struct mock_server *m);
void mock_stop(struct mock_server *m);

#endif /* _ICOM_NETWORK_MOCK_H */
