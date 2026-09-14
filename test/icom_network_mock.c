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

/* In-process mock of an Icom network radio. See icom_network_mock.h. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "icom_network_mock.h"

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "hamlib/rig.h"
#include "network_proto.h"
#include "network_seqbuf.h"

const uint8_t mock_freq_resp[] =
{ 0xfe, 0xfe, 0xe0, 0x98, 0x03, 0x00, 0x60, 0x06, 0x14, 0x00, 0xfd };

/* canned spectrum-scope (async) frame: cmd 0x27 subcmd 0x00 */
const uint8_t mock_spectrum[] =
{ 0xfe, 0xfe, 0xe0, 0x98, 0x27, 0x00, 0x01, 0x02, 0x03, 0x04, 0xfd };

/* canned LPCM16 audio payload (8 little-endian 16-bit samples) */
const uint8_t mock_audio[] =
{
    0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
    0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08
};

uint16_t bind_ephemeral(int *out_fd)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in sa;
    socklen_t addr_length = sizeof(sa);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;
    bind(fd, (struct sockaddr *)&sa, sizeof(sa));
    getsockname(fd, (struct sockaddr *)&sa, &addr_length);
    *out_fd = fd;
    return ntohs(sa.sin_port);
}

static void mock_send_control(int fd, struct sockaddr_in *to, uint16_t ctl,
                              uint32_t rcvd)
{
    uint8_t p[ICOM_NETWORK_HEADER_LEN];
    icom_network_packet_build_control(p, sizeof(p), ctl, 0, MOCK_SERVER_ID,
                                      rcvd);
    sendto(fd, p, sizeof(p), 0, (struct sockaddr *)to, sizeof(*to));
}

/* --- unsolicited server -> client traffic, driven by the test --- */

/* Sent before the select so it fires whether or not the client is talking. */
static void mock_send_unsolicited(struct mock_server *m)
{
    uint8_t packet[64];
    int pl;

    if (m->civ_peer_length > 0 && m->ask_retransmit)
    {
        uint16_t want = (uint16_t)m->retransmit_sequence;
        pl = icom_network_packet_build_retransmit(packet, sizeof(packet), &want, 1,
                MOCK_SERVER_ID, m->civ_client_id);
        m->ask_retransmit = 0;

        if (pl > 0)
        {
            sendto(m->civ_fd, packet, pl, 0, (struct sockaddr *)&m->civ_peer,
                   m->civ_peer_length);
        }
    }

    if (m->civ_peer_length > 0 && m->ask_ping)
    {
        pl = icom_network_packet_build_ping(packet, sizeof(packet), 0x00, 0x1234,
                                            m->civ_sequence++, MOCK_SERVER_ID,
                                            m->civ_client_id);
        m->ask_ping = 0;

        if (pl > 0)
        {
            sendto(m->civ_fd, packet, pl, 0, (struct sockaddr *)&m->civ_peer,
                   m->civ_peer_length);
        }
    }

    if (m->announce_disconnect && m->ctrl_peer_length > 0)
    {
        uint8_t st[0x50];
        memset(st, 0, sizeof(st));
        icom_network_packet_build_header(st, sizeof(st), sizeof(st), 0, 0,
                                         MOCK_SERVER_ID, m->ctrl_client_id);
        icom_network_put_le32(st + 0x30, 0);
        st[0x40] = 0x01;                  /* disconnect flag */
        m->announce_disconnect = 0;
        sendto(m->ctrl_fd, st, sizeof(st), 0,
               (struct sockaddr *)&m->ctrl_peer, m->ctrl_peer_length);
    }
}

/* Read and discard whatever is ready, so the client sees no answers at all. */
static void mock_drain(struct mock_server *m, fd_set *r, uint8_t *buf,
                       size_t buf_size)
{
    int fds[3] = { m->ctrl_fd, m->civ_fd, m->audio_fd };
    int i;

    for (i = 0; i < 3; i++)
    {
        if (FD_ISSET(fds[i], r))
        {
            recvfrom(fds[i], buf, buf_size, 0, NULL, NULL);
        }
    }
}

/* --- per-packet responders --- */

/* PROBE and READY are answered the same way on all three sockets. Returns
 * non-zero when the packet was one of them and has been answered. */
static int mock_reply_common_control(int fd, struct sockaddr_in *from,
                                     enum icom_network_packet_kind k,
                                     uint16_t type, uint32_t cli)
{
    if (k != ICOM_NETWORK_PACKET_KIND_CONTROL) { return 0; }

    switch (type)
    {
    case ICOM_NETWORK_CTL_PROBE:
        mock_send_control(fd, from, ICOM_NETWORK_CTL_PRESENT, cli);
        return 1;

    case ICOM_NETWORK_CTL_READY:
        mock_send_control(fd, from, ICOM_NETWORK_CTL_READY, cli);
        return 1;

    default:
        return 0;
    }
}

/* Turn a ping request into its reply, in place, as a radio does. */
static void mock_reply_ping(int fd, uint8_t *buf, int n,
                            struct sockaddr_in *from, socklen_t from_length,
                            uint32_t cli)
{
    buf[0x10] = 0x01;
    icom_network_put_be32(buf + ICOM_NETWORK_OFF_LOCAL_ID, MOCK_SERVER_ID);
    icom_network_put_be32(buf + ICOM_NETWORK_OFF_REMOTE_ID, cli);
    sendto(fd, buf, n, 0, (struct sockaddr *)from, from_length);
}

static void mock_reply_login(struct mock_server *m, struct sockaddr_in *from,
                             socklen_t from_length, uint32_t cli)
{
    uint8_t lr[0x60];
    memset(lr, 0, sizeof(lr));
    icom_network_packet_build_header(lr, sizeof(lr), 0x60, 0, 0,
                                     MOCK_SERVER_ID, cli);
    icom_network_put_be16(lr + 0x1a, 0x1234);
    icom_network_put_be32(lr + 0x1c, 0x9999); /* token */
    icom_network_put_le32(lr + 0x30, 0);      /* error=ok */
    memcpy(lr + 0x40, "FTTH", 4);
    sendto(m->ctrl_fd, lr, sizeof(lr), 0, (struct sockaddr *)from, from_length);
}

/* A token create is answered with the advertised radio list. */
static void mock_reply_capabilities(struct mock_server *m,
                                    struct sockaddr_in *from,
                                    socklen_t from_length, uint32_t cli)
{
    static const uint8_t mac[6] = { 0x00, 0x90, 0xc7, 0x15, 0xae, 0x02 };
    uint8_t cp[0x42 + 4 * 0x66];
    size_t cp_length = 0x42 + m->radio_count * 0x66;
    int i;

    memset(cp, 0, sizeof(cp));
    icom_network_packet_build_header(cp, sizeof(cp), cp_length, 0, 0,
                                     MOCK_SERVER_ID, cli);
    icom_network_put_be16(cp + 0x40, m->radio_count);

    for (i = 0; i < m->radio_count; i++)
    {
        uint8_t *r = cp + 0x42 + i * 0x66;

        /* MAC-mode identity, as an IC-7610 reports it */
        icom_network_put_le16(r + 0x07, ICOM_NETWORK_COMMONCAP_MAC);
        memcpy(r + 0x0a, mac, sizeof(mac));
        r[0x0f] = (uint8_t)i;
        strcpy((char *)r + 0x10, m->radios[i].name);
        strcpy((char *)r + 0x30, "ICOM_VAUDIO");
        icom_network_put_le16(r + 0x50, 0x073f); /* Ethernet */
        r[0x52] = m->radios[i].civ;
        icom_network_put_le16(r + 0x53, m->radios[i].rx_rate);
        icom_network_put_le16(r + 0x55, m->radios[i].tx_rate);
        icom_network_put_be32(r + 0x5a, 19200);
    }

    sendto(m->ctrl_fd, cp, cp_length, 0, (struct sockaddr *)from, from_length);
}

static void mock_reply_conninfo(struct mock_server *m, const uint8_t *buf, int n,
                                struct sockaddr_in *from,
                                socklen_t from_length, uint32_t cli)
{
    uint8_t stp[0x50];

    if (n >= (int)sizeof(m->connection_info))
    {
        memcpy(m->connection_info, buf, sizeof(m->connection_info));
        m->saw_connection_info = 1;
    }

    memset(stp, 0, sizeof(stp));
    icom_network_packet_build_header(stp, sizeof(stp), 0x50, 0, 0,
                                     MOCK_SERVER_ID, cli);
    icom_network_put_le32(stp + 0x30, 0); /* error=ok */
    icom_network_put_be16(stp + 0x42, m->civ_port);
    icom_network_put_be16(stp + 0x46, m->no_audio_port ? 0 : m->audio_port);
    sendto(m->ctrl_fd, stp, sizeof(stp), 0, (struct sockaddr *)from, from_length);
}

/* The CI-V stream opening. The radio flushes frames left undelivered by the
 * previous client as soon as a new stream opens; a stale NAK is the poisonous
 * variant, so a test can ask for one. */
static void mock_civ_stream_open(struct mock_server *m, struct sockaddr_in *from,
                                 socklen_t from_length, uint32_t cli)
{
    static const uint8_t nak[] = { 0xfe, 0xfe, 0xe0, 0x98, 0xfa, 0xfd };
    uint8_t packet[64];
    int pl;

    m->civ_peer = *from;
    m->civ_peer_length = from_length;
    m->civ_client_id = cli;

    if (!m->stale_nak) { return; }

    pl = icom_network_packet_build_civ(packet, sizeof(packet), nak, sizeof(nak),
                                       0xc0, 0, m->civ_sequence++,
                                       MOCK_SERVER_ID, cli);

    if (pl > 0)
    {
        sendto(m->civ_fd, packet, pl, 0, (struct sockaddr *)from, from_length);
    }
}

/* Answer a CI-V command, optionally preceded by an unsolicited spectrum frame
 * and/or a sequence jump, both of which a test can ask for. */
static void mock_reply_civ(struct mock_server *m, struct sockaddr_in *from,
                           socklen_t from_length, uint32_t cli)
{
    uint8_t packet[64];
    int pl;

    if (m->send_spectrum)
    {
        pl = icom_network_packet_build_civ(packet, sizeof(packet), mock_spectrum,
                                           sizeof(mock_spectrum), 0xc0, 0,
                                           m->civ_sequence++, MOCK_SERVER_ID, cli);

        if (pl > 0)
        {
            sendto(m->civ_fd, packet, pl, 0, (struct sockaddr *)from, from_length);
        }
    }

    if (m->civ_sequence_jump)
    {
        m->civ_sequence += (uint16_t)m->civ_sequence_jump;
        m->civ_sequence_jump = 0;
    }

    pl = icom_network_packet_build_civ(packet, sizeof(packet), mock_freq_resp,
                                       sizeof(mock_freq_resp), 0xc0, 0,
                                       m->civ_sequence++, MOCK_SERVER_ID, cli);

    if (pl > 0)
    {
        sendto(m->civ_fd, packet, pl, 0, (struct sockaddr *)from, from_length);
    }

    m->saw_civ_cmd = 1;
}

/* The audio stream opens with the plain probe + ready handshake (no openclose);
 * the stream is live after i-am-ready, so emit one audio data packet. */
static void mock_send_audio(struct mock_server *m, struct sockaddr_in *from,
                            socklen_t from_length, uint32_t cli)
{
    uint8_t packet[64];
    uint16_t audio_sequence = m->audio_sequence++;
    int pl = icom_network_packet_build_audio(packet, sizeof(packet), mock_audio,
             sizeof(mock_audio), 0x0080,
             audio_sequence, audio_sequence,
             MOCK_SERVER_ID, cli);

    if (pl > 0)
    {
        sendto(m->audio_fd, packet, pl, 0, (struct sockaddr *)from, from_length);
    }
}

/* --- per-socket service --- */

static void mock_service_control(struct mock_server *m, uint8_t *buf, int n,
                                 struct sockaddr_in *from,
                                 socklen_t from_length)
{
    uint32_t cli = icom_network_get_be32(buf + ICOM_NETWORK_OFF_LOCAL_ID);
    enum icom_network_packet_kind k = icom_network_packet_classify(buf, n);
    uint16_t type = icom_network_get_le16(buf + ICOM_NETWORK_OFF_TYPE);

    m->ctrl_peer = *from;
    m->ctrl_peer_length = from_length;
    m->ctrl_client_id = cli;

    if (mock_reply_common_control(m->ctrl_fd, from, k, type, cli)) { return; }

    switch (k)
    {
    case ICOM_NETWORK_PACKET_KIND_LOGIN:
        mock_reply_login(m, from, from_length, cli);
        break;

    case ICOM_NETWORK_PACKET_KIND_TOKEN:
        mock_reply_capabilities(m, from, from_length, cli);
        break;

    case ICOM_NETWORK_PACKET_KIND_CONNINFO:
        mock_reply_conninfo(m, buf, n, from, from_length, cli);
        break;

    case ICOM_NETWORK_PACKET_KIND_PING:
        mock_reply_ping(m->ctrl_fd, buf, n, from, from_length, cli);
        break;

    default:
        break;
    }
}

static void mock_service_civ(struct mock_server *m, uint8_t *buf, int n,
                             struct sockaddr_in *from, socklen_t from_length)
{
    uint32_t cli = icom_network_get_be32(buf + ICOM_NETWORK_OFF_LOCAL_ID);
    enum icom_network_packet_kind k = icom_network_packet_classify(buf, n);
    uint16_t type = icom_network_get_le16(buf + ICOM_NETWORK_OFF_TYPE);

    if (!mock_reply_common_control(m->civ_fd, from, k, type, cli))
    {
        switch (k)
        {
        case ICOM_NETWORK_PACKET_KIND_OPENCLOSE:
            mock_civ_stream_open(m, from, from_length, cli);
            break;

        case ICOM_NETWORK_PACKET_KIND_CIV:
            mock_reply_civ(m, from, from_length, cli);
            break;

        case ICOM_NETWORK_PACKET_KIND_PING:
            if (buf[0x10] == 0x01)
            {
                /* the client answering the ping request we sent */
                m->saw_ping_reply = 1;
            }
            else
            {
                mock_reply_ping(m->civ_fd, buf, n, from, from_length, cli);
            }

            break;

        default:
            break;
        }
    }

    /* A replay carries the sequence we asked for; the client only sends it
     * again in answer to a retransmit request. Checked whatever the kind, so
     * it must sit outside the dispatch above. */
    if (m->retransmit_sequence >= 0
            && icom_network_get_le16(buf + ICOM_NETWORK_OFF_SEQUENCE)
            == (uint16_t)m->retransmit_sequence
            && k != ICOM_NETWORK_PACKET_KIND_PING)
    {
        m->saw_retransmit_reply++;
    }
}

static void mock_service_audio(struct mock_server *m, uint8_t *buf, int n,
                               struct sockaddr_in *from, socklen_t from_length)
{
    uint32_t cli = icom_network_get_be32(buf + ICOM_NETWORK_OFF_LOCAL_ID);
    enum icom_network_packet_kind k = icom_network_packet_classify(buf, n);
    uint16_t type = icom_network_get_le16(buf + ICOM_NETWORK_OFF_TYPE);

    if (!mock_reply_common_control(m->audio_fd, from, k, type, cli))
    {
        return;
    }

    /* i-am-ready is also what makes the stream live */
    if (type == ICOM_NETWORK_CTL_READY)
    {
        mock_send_audio(m, from, from_length, cli);
    }
}

/* Receive one datagram, or return 0 when there is nothing usable. */
static int mock_recv(int fd, uint8_t *buf, size_t buf_size,
                     struct sockaddr_in *from, socklen_t *from_length)
{
    int n;

    *from_length = sizeof(*from);
    n = recvfrom(fd, buf, buf_size, 0, (struct sockaddr *)from, from_length);

    return n >= ICOM_NETWORK_HEADER_LEN ? n : 0;
}

static void *mock_run(void *arg)
{
    struct mock_server *m = arg;
    uint8_t buf[2048];

    while (!m->stop)
    {
        struct sockaddr_in from;
        socklen_t from_length;
        struct timeval tv;
        fd_set r;
        int maxfd = m->ctrl_fd;
        int n;

        if (m->civ_fd > maxfd) { maxfd = m->civ_fd; }

        if (m->audio_fd > maxfd) { maxfd = m->audio_fd; }

        mock_send_unsolicited(m);

        FD_ZERO(&r);
        FD_SET(m->ctrl_fd, &r);
        FD_SET(m->civ_fd, &r);
        FD_SET(m->audio_fd, &r);
        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        if (select(maxfd + 1, &r, NULL, NULL, &tv) <= 0) { continue; }

        if (m->go_silent)
        {
            mock_drain(m, &r, buf, sizeof(buf));
            continue;
        }

        if (FD_ISSET(m->ctrl_fd, &r))
        {
            n = mock_recv(m->ctrl_fd, buf, sizeof(buf), &from, &from_length);

            if (n > 0) { mock_service_control(m, buf, n, &from, from_length); }
        }

        if (FD_ISSET(m->civ_fd, &r))
        {
            n = mock_recv(m->civ_fd, buf, sizeof(buf), &from, &from_length);

            if (n > 0) { mock_service_civ(m, buf, n, &from, from_length); }
        }

        if (FD_ISSET(m->audio_fd, &r))
        {
            n = mock_recv(m->audio_fd, buf, sizeof(buf), &from, &from_length);

            if (n > 0) { mock_service_audio(m, buf, n, &from, from_length); }
        }
    }

    return NULL;
}

/* Every rate the protocol can express, which is what an IC-7610 advertises. */

void mock_start(struct mock_server *m)
{
#ifdef __MINGW32__
    /* The mock opens its sockets before the library opens any of its own, so
     * on Windows it cannot rely on the library having started Winsock. */
    static int wsa_started;

    if (!wsa_started)
    {
        WSADATA wsadata;

        if (WSAStartup(MAKEWORD(2, 2), &wsadata) == 0) { wsa_started = 1; }
    }

#endif
    memset(m, 0, sizeof(*m));
    m->radio_count = 1;
    m->radios[0].name = "IC-7610";
    m->radios[0].civ = 0x98;
    m->radios[0].rx_rate = MOCK_ALL_RATES;
    m->radios[0].tx_rate = MOCK_ALL_RATES;
    m->retransmit_sequence = -1;   /* no replay expected unless a test asks */
    m->ctrl_port  = bind_ephemeral(&m->ctrl_fd);
    m->civ_port   = bind_ephemeral(&m->civ_fd);
    m->audio_port = bind_ephemeral(&m->audio_fd);
    pthread_create(&m->thread, NULL, mock_run, m);
}

void mock_stop(struct mock_server *m)
{
    m->stop = 1;
    pthread_join(m->thread, NULL);
    socket_close(m->ctrl_fd);
    socket_close(m->civ_fd);
    socket_close(m->audio_fd);
}
