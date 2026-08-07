/*
 *  Hamlib streaming subsystem
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

/* Client-side UDP streaming session management for network rig backends. */
/* Handles UDP connection, subscribe handshake, keepalive, and data transfer. */

#ifndef HAMLIB_STREAM_NET_H
#define HAMLIB_STREAM_NET_H

#include <hamlib/rig.h>
#include "stream.h"
#include "stream_proto.h"
#include <pthread.h>
#include <time.h>

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#elif defined(HAVE_WS2TCPIP_H)
#  include <ws2tcpip.h>
#endif

/* Default keepalive interval (seconds). The server drops a stream that has
 * been silent for its keepalive timeout (30 s by default), so this interval
 * also sets how many consecutive lost PINGs a stream survives: pinging well
 * inside the timeout keeps a lossy link alive instead of tearing it down. */
#define RIG_STREAM_NET_KEEPALIVE_INTERVAL 5

/* SUBSCRIBE transmissions before the handshake is abandoned. The request or
 * its ACK may be lost, so the caller's timeout is divided across attempts. */
#define RIG_STREAM_NET_SUBSCRIBE_ATTEMPTS 3


/* Per-stream network session (stored in stream->backend_priv). */
struct rig_stream_net_session
{
    int udp_sock;                       /* UDP socket fd */
    struct sockaddr_storage server_addr; /* rigctld UDP endpoint */
    socklen_t server_addr_len;
    int remote_stream_id;               /* stream_id assigned by rigctld */
    int remote_source_id;               /* stream source ID reported by rigctld
                                           (0 = unset, tuple identity) */
    int remote_udp_port;                /* UDP port assigned by rigctld */
    uint32_t subscribe_token;           /* Anti-hijack token from rigctld */

    /* RX receiver thread */
    pthread_t rx_thread;
    HAMLIB_ATOMIC int rx_running;

    /* TX packet state */
    uint32_t tx_seq;                    /* outgoing sequence number */
    uint64_t tx_timestamp;              /* sample counter */

    /* RX loss classification state (rx thread only) */
    uint32_t rx_expected_seq;           /* next expected seq */
    uint64_t rx_expected_timestamp;     /* next expected producer index */
    int rx_first;                       /* 1 until the first data packet */
    int bad_source_logged;              /* 1 after warning once about a datagram
                                           from an unexpected source, so a flood
                                           cannot amplify the log */

    /* Keepalive */
    time_t last_ping_sent;
    int keepalive_interval_s;           /* RIG_STREAM_NET_KEEPALIVE_INTERVAL,
                                           or the stream_keepalive_interval
                                           conf token when set */

    /* Cached invariants (computed once at connect time) */
    uint8_t format_id;               /* Wire format index for headers */
    int sample_size;                     /* Bytes per sample (0 = unknown) */

    /* Rig-level socket-buffer defaults (from the rig conf tokens; 0 = unset).
     * Per-stream rig_stream_config.transport_buffer_* overrides these. */
    unsigned int transport_buffer_ms;
    unsigned int transport_buffer_bytes;

    /* Back-pointer for rx_thread access */
    struct rig_stream *stream;
};


/* True if two socket addresses share the same IP (family + address), ignoring
 * port. The RX path uses this to drop datagrams whose source is not the
 * negotiated server; exposed for unit testing the drop decision. */
int rig_stream_net_source_ip_equal(const struct sockaddr *a,
                                   const struct sockaddr *b);

/* Parse the response from a \stream_open command.
 * buf holds the whole response, one value per line: "stream_id\n"
 * "source_id\n" "udp_port\n". stream_id is required; source_id and udp_port
 * are filled when their lines are present.
 * Returns 0 on success, -1 on parse error. */
int rig_stream_net_parse_open_response(const char *buf, int *stream_id,
                                       int *source_id, int *udp_port);

/* Create a UDP socket and set the server address for sending.
 * host: rigctld hostname/IP, port: UDP port from stream_open response.
 * Returns 0 on success, -1 on failure. */
int rig_stream_net_udp_connect(struct rig_stream_net_session *sess,
                               const char *host, int port);

/* Send a SUBSCRIBE packet and wait for SUBSCRIBE_ACK.
 * Returns 0 on success, -1 on timeout or error. */
int rig_stream_net_subscribe(struct rig_stream_net_session *sess,
                             struct rig_stream *stream, int timeout_ms);

/* Send a PING keepalive packet.
 * Returns 0 on success, -1 on failure. */
int rig_stream_net_send_ping(struct rig_stream_net_session *sess,
                             struct rig_stream *stream);

/* RX receiver thread: receives UDP packets, unpacks, writes to ring buffer.
 * arg is a pointer to struct rig_stream. */
void *rig_stream_net_rx_thread(void *arg);

/* Process one received UDP packet (pkt, n bytes): validate the header,
 * route control/metadata/data frames, run seq/timestamp loss accounting,
 * and write samples to the ring buffer. Performs no socket I/O, so it is
 * directly unit-testable. Returns 0 when the packet was accepted (incl.
 * control frames), -1 when it was rejected. */
int rig_stream_net_process_packet(struct rig_stream_net_session *sess,
                                  struct rig_stream *stream,
                                  const unsigned char *pkt, size_t n);

/* Send one data packet for TX streams.
 * Packs header + payload and sends via UDP. When info carries a burst
 * target (time_valid and/or SOB/EOB flags), an embedded time block is
 * prefixed and RIG_STREAM_CTRL_TIME set; info may be NULL.
 * Returns bytes sent on success, -1 on failure. */
int rig_stream_net_send_data(struct rig_stream_net_session *sess,
                             struct rig_stream *stream,
                             const void *data, size_t len,
                             const struct rig_stream_write_info *info);

/* Clean up session: close socket, free struct.
 * Does NOT join rx_thread (caller must stop it first). */
void rig_stream_net_session_cleanup(struct rig_stream_net_session *sess);

/* Parse one stream_caps response line into a rig_stream_caps struct.
 * Expected format: "type=AUDIO_RX formats=PCM_S16,PCM_F32 rates=8000,48000 channels=1-2 max=4"
 * Returns 0 on success, -1 on parse error. */
int rig_stream_net_parse_caps_line(const char *line,
                                   struct rig_stream_caps *caps);


#endif /* HAMLIB_STREAM_NET_H */
