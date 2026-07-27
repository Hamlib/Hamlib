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

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "stream_net.h"
#include "stream_proto.h"
#include "stream_convert.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Portable socket headers and socket_close() come from stream_proto.h. */


int rig_stream_net_parse_open_response(const char *buf, int *stream_id,
                                       int *source_id, int *udp_port)
{
    char *endptr;
    long val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered\n", __func__);

    *source_id = 0;
    *udp_port = -1;

    if (buf == NULL || *buf == '\0')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: null or empty response buffer\n", __func__);
        return -1;
    }

    /* Parse stream_id from the first line */
    errno = 0;
    val = strtol(buf, &endptr, 10);

    /* stream_id is a uint16_t on the wire; bound it so it round-trips through
     * the (uint16_t) casts in every header build/compare. */
    if (endptr == buf || errno != 0 || val < 0 || val > 65535)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse stream_id\n", __func__);
        return -1;
    }

    *stream_id = (int)val;

    /* Try to parse source_id from the next line, if present */
    if (*endptr == '\n' && *(endptr + 1) != '\0')
    {
        const char *src_str = endptr + 1;
        errno = 0;
        val = strtol(src_str, &endptr, 10);

        if (endptr != src_str && errno == 0 && val >= 0 && val <= 65535)
        {
            *source_id = (int)val;
        }
    }

    /* Try to parse udp_port from the next line, if present */
    if (*endptr == '\n' && *(endptr + 1) != '\0')
    {
        const char *port_str = endptr + 1;
        errno = 0;
        val = strtol(port_str, &endptr, 10);

        if (endptr != port_str && errno == 0 && val > 0 && val <= 65535)
        {
            *udp_port = (int)val;
        }
    }

    return 0;
}


/* True if two socket addresses share the same IP (family + address, ignoring
 * port). The client UDP socket is not connect()ed, so datagrams from any host
 * reach it; the RX path drops those whose source IP is not the negotiated
 * server. Port is not compared: a server may legitimately send data from a
 * different source port than the control port. Non-static so the drop decision
 * is unit-testable without spoofing a source address. */
int rig_stream_net_source_ip_equal(const struct sockaddr *a,
                                   const struct sockaddr *b)
{
    if (a->sa_family != b->sa_family)
    {
        return 0;
    }

    if (a->sa_family == AF_INET)
    {
        const struct sockaddr_in *a4 = (const struct sockaddr_in *)a;
        const struct sockaddr_in *b4 = (const struct sockaddr_in *)b;
        return a4->sin_addr.s_addr == b4->sin_addr.s_addr;
    }

    if (a->sa_family == AF_INET6)
    {
        const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)a;
        const struct sockaddr_in6 *b6 = (const struct sockaddr_in6 *)b;
        return memcmp(&a6->sin6_addr, &b6->sin6_addr,
                      sizeof(a6->sin6_addr)) == 0;
    }

    return 0;
}


/* Receive one datagram and drop it (returning -1) if its source IP is not the
 * negotiated server. Returns the byte count on an accepted datagram, 0 on an
 * empty datagram, and -1 on a rejected, would-block, or errored datagram. */
static ssize_t recv_from_server(struct rig_stream_net_session *sess,
                                unsigned char *buf, size_t buflen)
{
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);
    ssize_t n = recvfrom(sess->udp_sock, buf, buflen, 0,
                         (struct sockaddr *)&from, &from_len);

    if (n <= 0)
    {
        return n;
    }

    if (!rig_stream_net_source_ip_equal((struct sockaddr *)&from,
                                        (struct sockaddr *)&sess->server_addr))
    {
        if (!sess->bad_source_logged)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: dropping datagram(s) from unexpected source\n",
                      __func__);
            sess->bad_source_logged = 1;
        }

        return -1;
    }

    return n;
}


int rig_stream_net_udp_connect(struct rig_stream_net_session *sess,
                               const char *host, int port)
{
    struct addrinfo hints, *res, *rp;
    char port_str[16];
    int sock;

    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered host=%s port=%d\n",
              __func__, host ? host : "(null)", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    snprintf(port_str, sizeof(port_str), "%d", port);

    int err = getaddrinfo(host, port_str, &hints, &res);

    if (err != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: getaddrinfo failed: %s\n", __func__,
                  gai_strerror(err));
        return -1;
    }

    /* Try each resolved address until one works */
    for (rp = res; rp != NULL; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sock < 0)
        {
            continue;
        }

        /* Store the server address for sendto() */
        memcpy(&sess->server_addr, rp->ai_addr, rp->ai_addrlen);
        sess->server_addr_len = rp->ai_addrlen;
        sess->udp_sock = sock;

        /* Size the receive buffer (RX: client receives) or send buffer
         * (TX: client sends) from the negotiated rate so a burst of
         * scheduling latency does not overflow the kernel buffer. */
        if (sess->stream)
        {
            const struct rig_stream_config *cfg = &sess->stream->config;
            int frame_bytes = rig_stream_format_sample_size(cfg->format);
            frame_bytes = (frame_bytes > 0 ? frame_bytes : 1)
                          * (cfg->channels > 0 ? cfg->channels : 1);
            /* Layered: per-stream config > rig conf token > built-in. */
            size_t transport_bytes = stream_transport_buffer_effective(
                                         cfg->transport_buffer_ms,
                                         cfg->transport_buffer_bytes,
                                         sess->transport_buffer_ms,
                                         sess->transport_buffer_bytes,
                                         cfg->sample_rate, frame_bytes);
            int which = stream_type_is_rx(sess->stream->type)
                        ? SO_RCVBUF : SO_SNDBUF;
            stream_apply_transport_buffer(sock, which, transport_bytes);
        }

        freeaddrinfo(res);
        return 0;
    }

    freeaddrinfo(res);
    rig_debug(RIG_DEBUG_ERR, "%s: all addresses failed for %s:%d\n",
              __func__, host ? host : "(null)", port);
    return -1;
}


/* Build and send a header-only control packet (PING, SUBSCRIBE, etc.). */
static int send_control_packet(struct rig_stream_net_session *sess,
                               struct rig_stream *stream,
                               uint16_t control)
{
    struct rig_stream_packet_header hdr;
    unsigned char buf[RIG_STREAM_HEADER_SIZE];

    stream_control_header_init(&hdr, (uint8_t)stream->type,
                               (uint16_t)sess->remote_stream_id,
                               sess->subscribe_token, control);
    hdr.sample_rate = stream->config.sample_rate;
    hdr.format = sess->format_id;
    hdr.channels = stream->config.channels;

    stream_packet_header_pack(&hdr, buf);

    ssize_t sent = sendto(sess->udp_sock, buf, RIG_STREAM_HEADER_SIZE, 0,
                          (struct sockaddr *)&sess->server_addr,
                          sess->server_addr_len);

    if (sent != RIG_STREAM_HEADER_SIZE)
    {
        return -1;
    }

    return 0;
}


int rig_stream_net_send_ping(struct rig_stream_net_session *sess,
                             struct rig_stream *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered\n", __func__);

    int ret = send_control_packet(sess, stream, RIG_STREAM_CTRL_PING);

    if (ret == 0)
    {
        sess->last_ping_sent = time(NULL);
    }

    return ret;
}


/* Wait until a UDP socket is readable. Returns >0 if readable, 0 on timeout,
 * -1 on error (including an fd at or above FD_SETSIZE). A signal does not end
 * the wait: the time already spent is deducted and the wait resumes, so a
 * caller cannot mistake an interruption for an expired timeout. */
static int wait_readable(int sock, int timeout_ms)
{
    /* POSIX fd_set is a value-indexed bitmask, so an fd at or above FD_SETSIZE
     * cannot be polled; Winsock's fd_set is a count-bounded handle array whose
     * handles routinely exceed FD_SETSIZE, so the guard applies off-Windows. */
#ifndef _WIN32
    if (sock >= FD_SETSIZE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: fd %d >= FD_SETSIZE\n", __func__, sock);
        return -1;
    }

#endif

    struct timespec start;
    int remaining_ms = timeout_ms;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (;;)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        struct timeval tv;
        tv.tv_sec = remaining_ms / 1000;
        tv.tv_usec = (remaining_ms % 1000) * 1000;

        int ready = select(sock + 1, &fds, NULL, NULL, &tv);

        if (ready >= 0)
        {
            return ready;
        }

        if (errno != EINTR)
        {
            return -1;
        }

        struct timespec now;

        clock_gettime(CLOCK_MONOTONIC, &now);
        long spent_ms = (now.tv_sec - start.tv_sec) * 1000
                        + (now.tv_nsec - start.tv_nsec) / 1000000;

        if (spent_ms >= timeout_ms)
        {
            return 0;
        }

        remaining_ms = timeout_ms - (int)spent_ms;
    }
}


int rig_stream_net_subscribe(struct rig_stream_net_session *sess,
                             struct rig_stream *stream, int timeout_ms)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered timeout_ms=%d\n",
              __func__, timeout_ms);

    /* The datagram carrying SUBSCRIBE, or the one carrying its ACK, may be
     * dropped: spread the caller's budget over several transmissions instead
     * of spending it all waiting on one. Repeating SUBSCRIBE is safe — the
     * server treats a second one as a re-subscribe and answers it again. */
    int attempt_ms = timeout_ms / RIG_STREAM_NET_SUBSCRIBE_ATTEMPTS;
    int attempt;

    if (attempt_ms < 1)
    {
        attempt_ms = 1;
    }

    for (attempt = 0; attempt < RIG_STREAM_NET_SUBSCRIBE_ATTEMPTS; attempt++)
    {
        if (send_control_packet(sess, stream, RIG_STREAM_CTRL_SUBSCRIBE) != 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: failed to send SUBSCRIBE packet\n",
                      __func__);
            return -1;
        }

        struct timespec start;

        int remaining_ms = attempt_ms;

        clock_gettime(CLOCK_MONOTONIC, &start);

        while (remaining_ms > 0)
        {
            int ready = wait_readable(sess->udp_sock, remaining_ms);

            if (ready < 0)
            {
                return -1;
            }

            if (ready > 0)
            {
                unsigned char buf[RIG_STREAM_MAX_DATAGRAM];
                struct rig_stream_packet_header hdr;
                ssize_t n = recv_from_server(sess, buf, sizeof(buf));

                /* Whatever is not our ACK — a PONG, an early data frame, a
                 * short or malformed datagram — is discarded rather than
                 * treated as a failed handshake. */
                if (n >= RIG_STREAM_HEADER_SIZE
                        && stream_packet_header_unpack(buf, (size_t)n, &hdr) == 0
                        && (hdr.control & RIG_STREAM_CTRL_SUBSCRIBE_ACK)
                        && hdr.stream_id == (uint16_t)sess->remote_stream_id)
                {
                    return 0;
                }
            }

            struct timespec now;

            clock_gettime(CLOCK_MONOTONIC, &now);
            long spent_ms = (now.tv_sec - start.tv_sec) * 1000
                            + (now.tv_nsec - start.tv_nsec) / 1000000;
            remaining_ms = attempt_ms - (int)spent_ms;
        }

        rig_debug(RIG_DEBUG_WARN,
                  "%s: no SUBSCRIBE_ACK in %d ms (attempt %d of %d)\n",
                  __func__, attempt_ms, attempt + 1,
                  RIG_STREAM_NET_SUBSCRIBE_ATTEMPTS);
    }

    rig_debug(RIG_DEBUG_ERR, "%s: timed out waiting for SUBSCRIBE_ACK\n",
              __func__);
    return -1;
}


int rig_stream_net_send_data(struct rig_stream_net_session *sess,
                             struct rig_stream *stream,
                             const void *data, size_t len,
                             const struct rig_stream_write_info *info)
{
    struct rig_stream_packet_header hdr;
    unsigned char pkt[RIG_STREAM_MAX_DATAGRAM];
    int with_target = info && (info->time_valid || info->flags);
    size_t block = with_target ? RIG_STREAM_TIME_BLOCK_SIZE : 0;

    rig_debug(RIG_DEBUG_TRACE, "%s() entered len=%zu\n", __func__, len);

    /* Reject against both the negotiated budget and the hard datagram-buffer
     * capacity, so a bogus max_payload can never drive an oversized memcpy. The
     * subtraction form avoids an overflow if len is pathologically large. */
    if (len + block > (size_t)stream->max_payload
            || len > sizeof(pkt) - RIG_STREAM_HEADER_SIZE - block)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: payload too large: %zu > %d\n",
                  __func__, len + block, stream->max_payload);
        return -1;
    }

    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = (uint8_t)stream->type;
    hdr.stream_id = (uint16_t)sess->remote_stream_id;
    hdr.subscribe_token = sess->subscribe_token;
    hdr.seq = sess->tx_seq;
    hdr.timestamp = sess->tx_timestamp;
    hdr.sample_rate = stream->config.sample_rate;
    hdr.format = sess->format_id;
    hdr.channels = stream->config.channels;
    hdr.control = with_target ? RIG_STREAM_CTRL_TIME : 0;
    hdr.payload_len = (uint16_t)(block + len);

    stream_packet_header_pack(&hdr, pkt);

    if (with_target)
    {
        /* Embedded burst target: the server's TX feeder extracts it into
         * the backend's target channel. source/accuracy are meaningless
         * for a target and stay zero. */
        struct rig_stream_time_anchor blk;
        memset(&blk, 0, sizeof(blk));
        blk.seconds = info->seconds;
        blk.picoseconds = info->picoseconds;
        blk.flags = info->flags & (RIG_STREAM_TIME_FLAG_SOB
                                   | RIG_STREAM_TIME_FLAG_EOB);

        if (info->time_valid)
        {
            blk.flags |= RIG_STREAM_TIME_FLAG_TX_TIMED;
        }

        stream_time_block_pack(&blk, pkt + RIG_STREAM_HEADER_SIZE);
    }

    memcpy(pkt + RIG_STREAM_HEADER_SIZE + block, data, len);

    size_t total = RIG_STREAM_HEADER_SIZE + block + len;
    ssize_t sent = sendto(sess->udp_sock, pkt, total, 0,
                          (struct sockaddr *)&sess->server_addr,
                          sess->server_addr_len);

    if (sent != (ssize_t)total)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: sendto failed: sent %zd of %zu bytes\n",
                  __func__, sent, total);
        return -1;
    }

    sess->tx_seq++;

    /* Advance timestamp by number of samples sent */
    if (stream->config.channels > 0)
    {
        uint64_t frame_size = (uint64_t)sess->sample_size * stream->config.channels;

        if (frame_size > 0)
        {
            sess->tx_timestamp += len / frame_size;
        }
    }

    return (int)len;
}


/* Advance the RX seq/timestamp expectation for a seq-bearing frame (data,
 * time-only, or metadata) and classify any hole that precedes it. Control
 * frames (PING/PONG/ACK) carry seq=0 and must NOT pass through here.
 * have_blk/blk carry the frame's time block when present (data path); pass
 * 0/NULL for metadata (no samples, no time block). */
static void account_seq(struct rig_stream_net_session *sess,
                        struct rig_stream *stream,
                        const struct rig_stream_packet_header *hdr,
                        uint64_t data_frames,
                        int have_blk,
                        const struct rig_stream_time_anchor *blk)
{
    if (!sess->rx_first)
    {
        uint64_t jump = hdr->timestamp > sess->rx_expected_timestamp
                        ? hdr->timestamp - sess->rx_expected_timestamp
                        : 0;

        /* Clamp an implausible forward jump (reordered/corrupt/spoofed frame
         * with a far-future timestamp) so it cannot inflate the loss counters
         * by up to 2^64 in one step. Ceiling ~10 s of samples at the rate. */
        uint64_t max_jump = (uint64_t)(stream->config.sample_rate > 0
                                       ? stream->config.sample_rate
                                       : 192000) * 10u;

        if (jump > max_jump)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: clamping implausible timestamp jump\n", __func__);
            jump = max_jump;
        }

        if (hdr->seq != sess->rx_expected_seq)
        {
            /* App-link UDP loss; the timestamp delta sizes the hole (a
             * concurrent upstream loss in the same window is attributed to
             * the link). */
            stream_skip_samples(stream, jump, RIG_STREAM_DROP_LINK);
        }
        else if (jump > 0)
        {
            uint8_t cause = (have_blk
                             && (blk->flags & RIG_STREAM_TIME_FLAG_DISC_OVERRUN))
                            ? RIG_STREAM_DROP_OVERRUN
                            : RIG_STREAM_DROP_GAP;
            stream_skip_samples(stream, jump, cause);
        }
        else if (have_blk && (blk->flags & RIG_STREAM_TIME_FLAG_DISCONTINUITY))
        {
            /* Discontinuity announced without an index jump: an unsized
             * upstream gap. */
            stream_skip_samples(stream, 0, RIG_STREAM_DROP_GAP);
        }
    }

    sess->rx_first = 0;
    sess->rx_expected_seq = hdr->seq + 1;
    sess->rx_expected_timestamp = hdr->timestamp + data_frames;
}


/* Async write-status: a server-reported late-burst / under- / overrun on a
 * TX stream. Record it (marked remote) for rig_stream_wait_write_status(); the
 * record helper also bumps the remote_* aggregate counters. A write-status
 * frame is only meaningful on a TX stream, so a spoofed one on an RX stream is
 * dropped rather than injecting bogus remote counters. */
static int handle_write_status_frame(struct rig_stream_net_session *sess,
                                     struct rig_stream *stream,
                                     const unsigned char *pkt,
                                     const struct rig_stream_packet_header *hdr)
{
    if (!stream_type_is_tx(stream->type))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: WRITE_STATUS on non-TX stream %d dropped\n",
                  __func__, hdr->stream_id);
        return 0;
    }

    struct rig_stream_write_status st;

    if (hdr->payload_len >= RIG_STREAM_WRITE_STATUS_WIRE_SIZE
            && stream_write_status_unpack(pkt + RIG_STREAM_HEADER_SIZE,
                                          hdr->payload_len, &st) == 0)
    {
        st.sample_index = hdr->timestamp;
        stream_record_write_status(stream, &st, 1);
    }

    /* Consumes a seq value on the wire; keep seq continuous. */
    account_seq(sess, stream, hdr, 0, 0, NULL);
    return 0;
}


/* Ingest a metadata frame's fields and account for the seq value it consumes
 * so the following data packet's seq stays continuous (otherwise it would be
 * misclassified as app-link loss). */
static int handle_metadata_frame(struct rig_stream_net_session *sess,
                                 struct rig_stream *stream,
                                 const unsigned char *pkt,
                                 const struct rig_stream_packet_header *hdr)
{
    if (hdr->payload_len >= RIG_STREAM_METADATA_WIRE_SIZE)
    {
        struct rig_stream_metadata meta;
        stream_metadata_unpack(pkt + RIG_STREAM_HEADER_SIZE,
                               hdr->payload_len, &meta);
        meta.sample_index = hdr->timestamp;
        stream->last_metadata = meta;
    }

    account_seq(sess, stream, hdr, 0, 0, NULL);
    return 0;
}


/* Data / time packet — classify losses three ways and replay upstream holes
 * into the local index domain so a netrigctl consumer's enriched read and stats
 * match direct mode:
 *   seq gap                          -> app-link UDP loss
 *   timestamp jump, DISC_OVERRUN set -> server ring overrun
 *   timestamp jump, otherwise        -> radio/network gap
 */
static int handle_data_frame(struct rig_stream_net_session *sess,
                             struct rig_stream *stream,
                             const unsigned char *pkt,
                             const struct rig_stream_packet_header *hdr)
{
    if (!stream_ctrl_time_valid(hdr->control))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: dropping packet with invalid TIME combination\n",
                  __func__);
        return -1;
    }

    const unsigned char *data = pkt + RIG_STREAM_HEADER_SIZE;
    size_t data_len = hdr->payload_len;
    struct rig_stream_time_anchor blk;
    int have_blk = 0;

    if (hdr->control & RIG_STREAM_CTRL_TIME)
    {
        if (stream_time_block_unpack(data, data_len, &blk) != 0)
        {
            return -1;
        }

        have_blk = 1;
        data += RIG_STREAM_TIME_BLOCK_SIZE;
        data_len -= RIG_STREAM_TIME_BLOCK_SIZE;
    }

    uint64_t data_frames = stream->frame_bytes > 0
                           ? data_len / (uint64_t)stream->frame_bytes
                           : 0;

    account_seq(sess, stream, hdr, data_frames, have_blk, &blk);

    /* Ingest the embedded capture time as a local anchor (skip info-less
     * discontinuity markers). */
    if (have_blk && blk.source != RIG_STREAM_TIME_SRC_NONE)
    {
        blk.sample_index = hdr->timestamp;
        rig_stream_push_time_anchor(stream, &blk);
    }

    if (data_len > 0)
    {
        stream_ringbuf_write(&stream->ringbuf, data, data_len);
    }

    return 0;
}


int rig_stream_net_process_packet(struct rig_stream_net_session *sess,
                                  struct rig_stream *stream,
                                  const unsigned char *pkt, size_t n)
{
    if (n < RIG_STREAM_HEADER_SIZE)
    {
        return -1;
    }

    struct rig_stream_packet_header hdr;

    if (stream_packet_header_unpack(pkt, n, &hdr) != 0)
    {
        return -1;
    }

    /* Reject frames whose claimed payload exceeds the bytes received, so
     * payload reads (metadata or data) stay within the datagram. */
    if (hdr.payload_len > (uint16_t)(n - RIG_STREAM_HEADER_SIZE))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: payload_len %u exceeds %zd received body bytes\n",
                  __func__, hdr.payload_len,
                  (ssize_t)(n - RIG_STREAM_HEADER_SIZE));
        return -1;
    }

    if (hdr.stream_id != (uint16_t)sess->remote_stream_id)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: unexpected stream_id %d (expected %d)\n",
                  __func__, hdr.stream_id, sess->remote_stream_id);
        return -1;
    }

    if (hdr.subscribe_token != sess->subscribe_token)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: token mismatch on stream %d\n",
                  __func__, hdr.stream_id);
        return -1;
    }

    /* When the server advertised a source discriminator, the identity is the
     * (source_id, stream_id) pair: drop frames bearing a different source_id
     * so a second server sharing the stream_id namespace cannot be confused
     * for this one. */
    if (sess->remote_source_id != 0
            && hdr.source_id != (uint16_t)sess->remote_source_id)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: unexpected source_id %u (expected %d) on stream %d\n",
                  __func__, hdr.source_id, sess->remote_source_id,
                  hdr.stream_id);
        return -1;
    }

    /* Control frames are header-only replies sent with seq=0; they must not
     * enter seq accounting, or the next data packet would be misread as a
     * gap. */
    if (hdr.control & (RIG_STREAM_CTRL_PONG | RIG_STREAM_CTRL_SUBSCRIBE_ACK))
    {
        return 0;
    }

    /* ERROR frames are reserved (not emitted yet). Drop defensively so an
     * error payload is never misinterpreted as sample data. */
    if (hdr.control & RIG_STREAM_CTRL_ERROR)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: received ERROR frame on stream %d\n",
                  __func__, hdr.stream_id);
        return 0;
    }

    if (hdr.control & RIG_STREAM_CTRL_WRITE_STATUS)
    {
        return handle_write_status_frame(sess, stream, pkt, &hdr);
    }

    if (hdr.control & RIG_STREAM_CTRL_METADATA)
    {
        return handle_metadata_frame(sess, stream, pkt, &hdr);
    }

    return handle_data_frame(sess, stream, pkt, &hdr);
}


/* Send a keepalive PING if the configured interval has elapsed. */
static void maybe_keepalive(struct rig_stream_net_session *sess,
                            struct rig_stream *stream)
{
    if (sess->keepalive_interval_s > 0
            && time(NULL) - sess->last_ping_sent >= sess->keepalive_interval_s)
    {
        rig_stream_net_send_ping(sess, stream);
    }
}


void *rig_stream_net_rx_thread(void *arg)
{
    struct rig_stream *stream = (struct rig_stream *)arg;
    struct rig_stream_net_session *sess =
        (struct rig_stream_net_session *)stream->backend_priv;
    /* Sized to the jumbo ceiling so a larger-MTU sender is not truncated. */
    unsigned char pkt[RIG_STREAM_MAX_DATAGRAM];

    while (sess->rx_running)
    {
        if (wait_readable(sess->udp_sock, 1000) <= 0)
        {
            /* Timeout or error — check keepalive */
            maybe_keepalive(sess, stream);
            continue;
        }

        ssize_t n = recv_from_server(sess, pkt, sizeof(pkt));

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            continue;
        }

        rig_stream_net_process_packet(sess, stream, pkt, (size_t)n);

        /* Periodic keepalive */
        maybe_keepalive(sess, stream);
    }

    return NULL;
}


void rig_stream_net_session_cleanup(struct rig_stream_net_session *sess)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered\n", __func__);

    if (sess == NULL)
    {
        return;
    }

    if (sess->udp_sock >= 0)
    {
        socket_close(sess->udp_sock);
        sess->udp_sock = -1;
    }

    free(sess);
}


/* Helper: find value for a key in "key=value" within a space-separated line.
 * Returns pointer into line at start of value, or NULL if not found.
 * val_len is set to the length of the value (up to next space or end). */
/* Caps lines are parsed in place with this small key=value scanner rather
 * than the shared hamlib_parse_kv_args(): that helper consumes a FILE* and
 * returns an array of pairs, whereas here we look up individual keys in a
 * single already-received line without copying or stream I/O. */
static const char *find_kv(const char *line, const char *key, int *val_len)
{
    size_t klen = strlen(key);
    const char *p = line;

    while (*p)
    {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        if (strncmp(p, key, klen) == 0 && p[klen] == '=')
        {
            const char *val = p + klen + 1;
            const char *end = val;

            while (*end && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r')
            {
                end++;
            }

            *val_len = (int)(end - val);
            return val;
        }

        /* Skip to next whitespace */
        while (*p && *p != ' ' && *p != '\t')
        {
            p++;
        }
    }

    return NULL;
}


/* Copy a key-value string into a NUL-terminated stack buffer, clamping length. */
static void copy_kv_value(const char *val, int vlen, char *buf, size_t bufsize)
{
    if (bufsize == 0)
    {
        return;
    }

    int n = (vlen < (int)bufsize - 1) ? vlen : (int)bufsize - 1;

    if (n < 0)
    {
        n = 0;
    }

    memcpy(buf, val, n);
    buf[n] = '\0';
}


/* Parse a non-negative decimal integer from server-supplied text.
 * Returns the value, or -1 if the text is empty, malformed, or out of range. */
static int parse_uint_field(const char *s)
{
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(s, &end, 10);

    if (end == s || *end != '\0' || errno != 0 || v < 0 || v > INT_MAX)
    {
        return -1;
    }

    return (int)v;
}


int rig_stream_net_parse_caps_line(const char *line,
                                   struct rig_stream_caps *caps)
{
    const char *val;
    int vlen;

    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered\n", __func__);

    if (line == NULL || caps == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: null line or caps pointer\n", __func__);
        return -1;
    }

    memset(caps, 0, sizeof(*caps));

    /* Parse type= (required) */
    val = find_kv(line, "type", &vlen);

    if (val == NULL || vlen == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: missing required 'type' field\n",
                  __func__);
        return -1;
    }

    {
        char tbuf[32];
        copy_kv_value(val, vlen, tbuf, sizeof(tbuf));

        if (stream_type_parse(tbuf, &caps->type) < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unknown stream type '%s'\n",
                      __func__, tbuf);
            return -1;
        }
    }

    /* Parse formats= (comma-separated format names) */
    val = find_kv(line, "formats", &vlen);

    if (val != NULL && vlen > 0)
    {
        char fbuf[256];
        copy_kv_value(val, vlen, fbuf, sizeof(fbuf));

        char *saveptr = NULL;
        char *tok = strtok_r(fbuf, ",", &saveptr);

        while (tok != NULL)
        {
            rig_stream_format_t fmt = stream_format_parse(tok);

            if (fmt != 0)
            {
                caps->formats |= fmt;
            }

            tok = strtok_r(NULL, ",", &saveptr);
        }
    }

    /* Parse rates= (comma-separated integers) */
    val = find_kv(line, "rates", &vlen);

    if (val != NULL && vlen > 0)
    {
        char rbuf[128];
        copy_kv_value(val, vlen, rbuf, sizeof(rbuf));

        int ri = 0;
        char *saveptr2 = NULL;
        char *tok = strtok_r(rbuf, ",", &saveptr2);

        while (tok != NULL && ri < HAMLIB_MAX_STREAM_RATES - 1)
        {
            int rate = parse_uint_field(tok);

            if (rate > 0)
            {
                caps->sample_rates[ri++] = rate;
            }

            tok = strtok_r(NULL, ",", &saveptr2);
        }

        caps->sample_rates[ri] = 0;  /* Sentinel */
    }

    /* Parse channels=MIN-MAX */
    val = find_kv(line, "channels", &vlen);

    if (val != NULL && vlen > 0)
    {
        char cbuf[32];
        copy_kv_value(val, vlen, cbuf, sizeof(cbuf));

        if (sscanf(cbuf, "%d-%d", &caps->channels_min, &caps->channels_max) != 2)
        {
            /* Try single value */
            int ch = parse_uint_field(cbuf);
            caps->channels_min = (ch >= 0) ? ch : 0;
            caps->channels_max = caps->channels_min;
        }
    }

    /* Parse max= */
    val = find_kv(line, "max", &vlen);

    if (val != NULL && vlen > 0)
    {
        char mbuf[16];
        int mx;
        copy_kv_value(val, vlen, mbuf, sizeof(mbuf));
        mx = parse_uint_field(mbuf);
        caps->max_streams = (mx >= 0) ? mx : 0;
    }

    return 0;
}
