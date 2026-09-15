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
    /* Answer the connection-info request with this status error (0 = OK), as
     * a radio still holding another session's slot does. */
    volatile uint32_t status_error;
    /* Behave like the radio on the control socket: number the handshake
     * replies and idles as tracked packets, keep them for retransmission, and
     * answer retransmit requests. Off by default. */
    volatile int ctrl_tracked;
    uint16_t ctrl_sequence;
    int64_t ctrl_last_idle_ms;
    struct
    {
        uint16_t sequence;
        uint16_t length;
        uint8_t data[0x42 + 4 * 0x66];
    } ctrl_sent[16];
    int ctrl_sent_head;
    volatile int ctrl_retransmit_requests;
    /* Lose this many capabilities replies on the way (they still consume a
     * sequence number, so the client sees a gap). */
    volatile int drop_capabilities_replies;
    /* Ignore a login/token/connection-info request whose sequence was already
     * received, as the radio does with a resent packet. */
    volatile int ctrl_ignore_resends;
    uint16_t ctrl_seen[32];
    int ctrl_seen_count;
    /* Session clean-up seen on the control socket. */
    volatile int saw_token_remove;
    volatile int saw_ctrl_disconnect;
    uint8_t connection_info[0x90];
    uint16_t civ_sequence;
    uint16_t audio_sequence;

    /* Scripted audio. Setting audio_script_go sends audio_script_count data
     * packets, in the order listed, with the sequence numbers in audio_script;
     * each payload is audio_script_bytes[i] bytes (0 = 16) of 16-bit samples
     * whose value is that packet's sequence number, so a test can see order,
     * duplicates and gaps in what comes out. The flag clears once sent. */
    uint16_t audio_script[256];
    uint16_t audio_script_bytes[256];
    volatile int audio_script_count;
    volatile int audio_script_go;
    /* One sequence number held back: sent only in answer to a retransmit
     * request for it (-1 = none). audio_withheld_bytes as in the script. */
    volatile int audio_withheld;
    uint16_t audio_withheld_bytes;
    /* Requests for the withheld packet to ignore before answering one, as if
     * the first resends were lost on the way. */
    volatile int audio_withheld_ignore;
    volatile int audio_retransmit_requests;  /* requests seen on audio socket */
    struct sockaddr_in audio_peer;
    socklen_t audio_peer_length;
    uint32_t audio_client_id;
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
