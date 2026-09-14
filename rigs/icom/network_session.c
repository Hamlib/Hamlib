/*
 *  Hamlib Icom network backend - session and state machine
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

/* Icom network protocol Layer 2: session/state machine over the 3 UDP sockets. */
/* Runs the handshake, owns per-socket keepalive/retransmit threads, and bridges */
/* complete CI-V frames to the icom transaction layer. */

#include "hamlib/config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/* Windows has no BSD socket headers; pull in winsock the same way src/network.c
 * does, including the _WIN32_WINNT bump that inet_pton needs. */
#if defined(HAVE_WS2TCPIP_H) && !defined(HAVE_SYS_SOCKET_H)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#include <ws2tcpip.h>
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0502
#endif
#ifdef __MINGW32__
#include <winsock2.h>
#endif

/* Sockets are closed with closesocket() on Windows and close() elsewhere;
 * wrapped once so the three call sites stay readable. */
static void icom_network_session_close_fd(int fd)
{
#ifdef __MINGW32__
    closesocket(fd);
#else
    close(fd);
#endif
}

#include "hamlib/rig.h"
#include "network_proto.h"
#include "network_seqbuf.h"
#include "network_session.h"
#include "network_utils.h"

#define ICOM_NETWORK_SESSION_PACKET_MAX         2048
#define ICOM_NETWORK_SESSION_CIV_FRAME_MAX   256
#define ICOM_NETWORK_SESSION_CIV_QUEUE_LENGTH   32
#define ICOM_NETWORK_SESSION_AUDIO_FRAME_MAX 1600
#define ICOM_NETWORK_SESSION_AUDIO_QUEUE_LENGTH 64

#define ICOM_NETWORK_SESSION_IDLE_MS        100
#define ICOM_NETWORK_SESSION_PING_MS        500
/* audio stream pings fast, inside the TX freshness window */
#define ICOM_NETWORK_SESSION_AUDIO_PING_MS 100

/* CI-V stream-open drain: the radio flushes frames left undelivered by the
 * previous client as soon as a new stream opens; connect() discards them
 * until the response queue stays quiet for QUIET_MS (bounded by TOTAL_MS). */
#define ICOM_NETWORK_SESSION_CIV_DRAIN_QUIET_MS  50
#define ICOM_NETWORK_SESSION_CIV_DRAIN_TOTAL_MS 250
#define ICOM_NETWORK_SESSION_TOKEN_RENEW_MS 60000
#define ICOM_NETWORK_SESSION_RETRANSMIT_MS  100
#define ICOM_NETWORK_SESSION_PURGE_MS       10000
#define ICOM_NETWORK_SESSION_HANDSHAKE_TIMEOUT_MS 500
/* How long disconnect() keeps the threads alive after sending the stream
 * closes, so the radio's retransmit requests for those packets can still be
 * answered. Chosen to comfortably cover the radio's own retransmit latency
 * (observed well under 100 ms) without stalling rig_close(). */
#define ICOM_NETWORK_SESSION_DISCONNECT_GRACE_MS 300
/* Auto-reconnect backoff. The radio holds a lost session's slot for its own
 * timeout, so the early retries are expected to fail; the ramp gets past that
 * quickly and then keeps trying indefinitely at the cap. */
#define ICOM_NETWORK_SESSION_RECONNECT_MIN_MS  1000
#define ICOM_NETWORK_SESSION_RECONNECT_MAX_MS 30000
#define ICOM_NETWORK_SESSION_HANDSHAKE_TRIES 20

/* One UDP socket with its sequence bookkeeping. */
struct icom_network_session_socket
{
    int      fd;
    uint32_t local_id;          /* our id, derived from the bound local address */
    uint32_t remote_id;         /* server's id, learned from first reply */
    uint16_t send_sequence;          /* next TRACKED data-packet sequence (the radio
                                   gap-tracks these; only data packets advance it) */
    uint16_t untracked_sequence;     /* sequence for untracked control/idle/ping packets,
                                   which must NOT advance the tracked send_sequence */
    struct icom_network_txbuf txbuf;
    struct icom_network_rxtrack rxtrack;
    pthread_mutex_t lock;       /* guards send_sequence + txbuf + socket writes */
    pthread_t thread;
    int      thread_running;
};

struct icom_network_session
{
    struct icom_network_session_config config;
    icom_network_async_cb async_cb;
    void *async_ctx;

    struct icom_network_session_socket control;
    struct icom_network_session_socket civ;
    struct icom_network_session_socket audio;

    /* handshake-derived state */
    uint16_t auth_inner_sequence;
    uint16_t token_request;
    uint32_t token;
    /* Liveness. last_heard_ms is stamped by every socket thread on any inbound
     * packet; the control thread compares it against the configured timeout.
     * lost is set once and never cleared except by a successful reconnect. */
    volatile int64_t last_heard_ms;
    volatile int     lost;
    volatile unsigned loss_reason;   /* RIG_COMM_REASON_* */
    icom_network_lost_cb lost_cb;
    void *lost_ctx;

    pthread_t reconnect_thread;
    volatile int reconnect_running;
    volatile int stop_reconnect;

    /* Counts of sequence resyncs, i.e. how often the missing set had to be
     * abandoned because the link lost more than the replay window can recover. */
    unsigned civ_resyncs;
    unsigned audio_resyncs;

    uint8_t  radio_identity[16];  /* echoed back to select the radio */
    char     radio_name[33];      /* selected radio's name, as the server
                                     reported it; echoed on connect */
    int      tx_audio_available;  /* selected radio advertises a TX rate */
    uint16_t civ_server_port;
    uint16_t audio_server_port;
    uint16_t civ_local_port;
    uint16_t audio_local_port;
    uint16_t civ_send_sequence; /* counts CI-V frames, separate from the
                                   socket's packet sequence */
    uint8_t  radio_civ_addr;    /* radio's CI-V address, from capabilities;
                                   frames addressed to it are our own echoed
                                   commands and are dropped */

    /* CI-V receive queue (complete FE FE..FD frames) */
    pthread_mutex_t civ_rx_lock;
    pthread_cond_t  civ_rx_cond;
    struct
    {
        uint8_t data[ICOM_NETWORK_SESSION_CIV_FRAME_MAX];
        size_t  length;
    } civ_rx_q[ICOM_NETWORK_SESSION_CIV_QUEUE_LENGTH];
    int civ_rx_head, civ_rx_tail, civ_rx_count;

    /* audio receive queue (encoded codec payloads from the radio) */
    pthread_mutex_t audio_rx_lock;
    pthread_cond_t  audio_rx_cond;
    struct { uint8_t data[ICOM_NETWORK_SESSION_AUDIO_FRAME_MAX]; size_t length; }
    audio_rx_q[ICOM_NETWORK_SESSION_AUDIO_QUEUE_LENGTH];
    int audio_rx_head, audio_rx_tail, audio_rx_count;
    volatile int audio_active;
    uint16_t audio_send_sequence;

    int connected;
    volatile int stop;
};

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void icom_network_session_sleep_ms(int ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* Connection id from a bound socket's local address. */
static uint32_t icom_network_session_local_id(int fd)
{
    struct sockaddr_in sa;
    socklen_t addr_length = sizeof(sa);
    uint8_t ip[4];

    memset(&sa, 0, sizeof(sa));

    if (getsockname(fd, (struct sockaddr *)&sa, &addr_length) != 0)
    {
        return 0;
    }

    memcpy(ip, &sa.sin_addr.s_addr, 4);
    return icom_network_make_id(ip, ntohs(sa.sin_port));
}

/* Create a UDP socket connected to host:port. */
static int icom_network_session_socket_open(struct icom_network_session_socket
        *s, const char *host, uint16_t port)
{
    struct sockaddr_in sa;
    int fd;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0)
    {
        return -RIG_EIO;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1)
    {
        icom_network_session_close_fd(fd);
        return -RIG_EINVAL;
    }

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0)
    {
        icom_network_session_close_fd(fd);
        return -RIG_EIO;
    }

    s->fd = fd;
    s->local_id = icom_network_session_local_id(fd);
    s->remote_id = 0;
    s->send_sequence =
        1;   /* tracked data sequence starts at 1: the radio expects the first DATA packet (login) at sequence 1 */
    icom_network_txbuf_init(&s->txbuf);
    icom_network_rxtrack_init(&s->rxtrack);
    s->thread_running = 0;

    return RIG_OK;
}

static void icom_network_session_socket_close(struct icom_network_session_socket
        *s)
{
    if (s->fd >= 0)
    {
        icom_network_session_close_fd(s->fd);
        s->fd = -1;
    }
}

/* Send a fully-built packet; track data packets for replay. */
static int icom_network_session_send_tracked(struct icom_network_session_socket
        *s, const uint8_t *packet,
        size_t length,
        int trackable)
{
    ssize_t w;

    pthread_mutex_lock(&s->lock);

    if (trackable)
    {
        uint16_t sequence = icom_network_get_le16(packet + ICOM_NETWORK_OFF_SEQUENCE);

        /* Only the ability to answer a later retransmit request for this
         * sequence is lost; the send itself still goes ahead. Worth saying so,
         * because the symptom appears later and elsewhere -- as the radio
         * asking for a sequence we cannot produce. */
        if (icom_network_txbuf_add(&s->txbuf, sequence, packet, length,
                                   icom_network_now_ms()) != 0)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: sequence %u not retained for retransmit\n",
                      __func__, (unsigned)sequence);
        }
    }

    w = send(s->fd, packet, length, 0);
    pthread_mutex_unlock(&s->lock);

    return w == (ssize_t)length ? RIG_OK : -RIG_EIO;
}

/* Receive one packet with a timeout. Returns bytes, 0 on timeout, <0 error. */
static int icom_network_session_recv_timeout(int fd, uint8_t *buf,
        size_t buffer_length,
        int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    int r;
    ssize_t got;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (long)(timeout_ms % 1000) * 1000;
    r = select(fd + 1, &rfds, NULL, NULL, &tv);

    if (r == 0) { return 0; }

    if (r < 0) { return -RIG_EIO; }

    got = recv(fd, buf, buffer_length, 0);

    if (got < 0) { return -RIG_EIO; }

    return (int)got;
}

/* Build only the 16-byte header into packet and send (control opcodes/idle). */
static int icom_network_session_send_control(struct icom_network_session_socket
        *s, uint16_t control)
{
    uint8_t packet[ICOM_NETWORK_HEADER_LEN];
    uint16_t sequence;

    /* Untracked control/idle packets use sequence 0; only tracked data packets
     * advance send_sequence, which the radio gap-tracks for retransmission. */
    sequence = 0;

    icom_network_packet_build_control(packet, sizeof(packet), control, sequence,
                                      s->local_id, s->remote_id);
    return icom_network_session_send_tracked(s, packet, sizeof(packet), 0);
}

/* defined below; used by icom_network_session_await to keep the link
 * alive during the handshake */
static void icom_network_session_handle_ping(struct icom_network_session_socket
        *s, uint8_t *buf, int length);
static void icom_network_session_handle_retransmit_request(
    struct icom_network_session_socket *s,
    const uint8_t *buf,
    int length);
static void icom_network_session_mark_lost(struct icom_network_session *s,
        unsigned reason);
static int icom_network_session_rx_common(struct icom_network_session *s,
        struct icom_network_session_socket *sock,
        uint8_t *buf, int n,
        enum icom_network_packet_kind *kind);
static void *icom_network_session_reconnect_thread(void *arg);

/*
 * Read packets on socket s until one matches, within timeout_ms. want_ctl >= 0
 * demands that exact control opcode; want_ctl < 0 matches on want_kind alone.
 * The radio interleaves idle, probe and ping traffic during the handshake, so
 * anything that does not match is discarded rather than mistaken for the reply
 * (pings and retransmit requests are still serviced, since the radio will not
 * advance the handshake otherwise).
 * Returns the matching packet length, or 0 on timeout.
 */
static int icom_network_session_await(struct icom_network_session_socket *s,
                                      uint8_t *rbuf,
                                      size_t recv_buffer_length,
                                      enum icom_network_packet_kind want_kind, int want_ctl,
                                      int timeout_ms)
{
    int64_t deadline = icom_network_now_ms() + timeout_ms;
    int64_t last_idle = 0;

    while (icom_network_now_ms() < deadline)
    {
        enum icom_network_packet_kind k;
        int n;

        /* keep a steady idle stream going so the radio treats the link as
         * established and will answer login, token, connection_info, etc. */
        if (icom_network_now_ms() - last_idle >= ICOM_NETWORK_SESSION_IDLE_MS)
        {
            (void)icom_network_session_send_control(s, ICOM_NETWORK_CTL_IDLE);
            last_idle = icom_network_now_ms();
        }

        n = icom_network_session_recv_timeout(s->fd, rbuf, recv_buffer_length, 50);

        if (n <= 0) { continue; }

        if (s->remote_id == 0)
        {
            struct icom_network_packet_header h;

            if (icom_network_packet_parse_header(rbuf, n, &h) == RIG_OK)
            {
                s->remote_id = h.local_id;
            }
        }

        /* Keep the link alive: the radio will not advance (e.g. answer login)
         * unless its pings are answered during the handshake. That is the same
         * servicing the receive threads do, so it is the same function -- with
         * no session yet, hence the NULL. */
        if (icom_network_session_rx_common(NULL, s, rbuf, n, &k))
        {
            continue;
        }

        if (want_ctl >= 0)
        {
            uint16_t ctl = icom_network_get_le16(rbuf + ICOM_NETWORK_OFF_TYPE);

            if (k == ICOM_NETWORK_PACKET_KIND_CONTROL
                    && ctl == (uint16_t)want_ctl)
            {
                return n;
            }
        }
        else if (k == want_kind)
        {
            return n;
        }

        /* idle / ping / unrelated handshake noise — keep reading */
    }

    return 0;
}

/* Send a handshake opcode and wait for the specific reply it earns, resending
 * the request if that reply does not arrive in time. Anything else that shows
 * up meanwhile is discarded rather than accepted, so an unrelated control
 * frame cannot stand in for the answer we need. */
static int icom_network_session_exchange(struct icom_network_session_socket *s,
        uint16_t control, uint16_t expect_control,
        uint8_t *rbuf, size_t recv_buffer_length, int *response_length)
{
    int tries;

    for (tries = 0; tries < ICOM_NETWORK_SESSION_HANDSHAKE_TRIES; tries++)
    {
        int n;
        (void)icom_network_session_send_control(s, control);
        n = icom_network_session_await(s, rbuf, recv_buffer_length,
                                       ICOM_NETWORK_PACKET_KIND_CONTROL,
                                       (int)expect_control,
                                       ICOM_NETWORK_SESSION_HANDSHAKE_TIMEOUT_MS);

        if (n > 0)
        {
            if (response_length) { *response_length = n; }

            return RIG_OK;
        }
    }

    return -RIG_ETIMEOUT;
}

/* Send a data packet, await a reply of the expected kind. */
static int icom_network_session_send_recv(struct icom_network_session_socket *s,
        const uint8_t *packet,
        size_t length,
        enum icom_network_packet_kind want,
        uint8_t *rbuf, size_t recv_buffer_length, int *response_length)
{
    int tries;

    for (tries = 0; tries < ICOM_NETWORK_SESSION_HANDSHAKE_TRIES; tries++)
    {
        int n;
        (void)icom_network_session_send_tracked(s, packet, length, 1);
        n = icom_network_session_await(s, rbuf, recv_buffer_length, want, -1,
                                       ICOM_NETWORK_SESSION_HANDSHAKE_TIMEOUT_MS);

        if (n > 0)
        {
            if (response_length) { *response_length = n; }

            return RIG_OK;
        }
    }

    return -RIG_ETIMEOUT;
}

/* ------------------------------------------------------------------ */
/* CI-V receive queue                                                  */
/* ------------------------------------------------------------------ */

static void icom_network_session_civ_rx_push(struct icom_network_session *s,
        const uint8_t *frame, size_t length)
{
    if (length > ICOM_NETWORK_SESSION_CIV_FRAME_MAX)
    {
        length = ICOM_NETWORK_SESSION_CIV_FRAME_MAX;
    }

    pthread_mutex_lock(&s->civ_rx_lock);

    if (s->civ_rx_count == ICOM_NETWORK_SESSION_CIV_QUEUE_LENGTH)
    {
        /* queue full: drop the oldest frame to keep the freshest */
        s->civ_rx_tail = (s->civ_rx_tail + 1) % ICOM_NETWORK_SESSION_CIV_QUEUE_LENGTH;
        s->civ_rx_count--;
    }

    memcpy(s->civ_rx_q[s->civ_rx_head].data, frame, length);
    s->civ_rx_q[s->civ_rx_head].length = length;
    s->civ_rx_head = (s->civ_rx_head + 1) % ICOM_NETWORK_SESSION_CIV_QUEUE_LENGTH;
    s->civ_rx_count++;
    pthread_cond_signal(&s->civ_rx_cond);

    pthread_mutex_unlock(&s->civ_rx_lock);
}

/* ------------------------------------------------------------------ */
/* handle an incoming packet on a socket (shared by both threads)      */
/* ------------------------------------------------------------------ */

/* Reply to a retransmit request by replaying stored packets. */
static void icom_network_session_handle_retransmit_request(
    struct icom_network_session_socket *s,
    const uint8_t *buf,
    int length)
{
    size_t i;
    size_t n = (length > ICOM_NETWORK_HEADER_LEN)
               ? (size_t)(length - ICOM_NETWORK_HEADER_LEN) / 4 : 0;

    if (n == 0)
    {
        /* single-sequence form: sequence is in the header */
        uint16_t sequence = icom_network_get_le16(buf + ICOM_NETWORK_OFF_SEQUENCE);
        size_t packet_length;
        const uint8_t *p;
        pthread_mutex_lock(&s->lock);
        p = icom_network_txbuf_get(&s->txbuf, sequence, &packet_length);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: rig requests retransmit sequence=%u %s\n",
                  __func__, sequence, p ? "(replaying)" : "(NOT in txbuf)");

        if (p) { send(s->fd, p, packet_length, 0); }

        pthread_mutex_unlock(&s->lock);
        return;
    }

    for (i = 0; i < n; i++)
    {
        uint16_t sequence = icom_network_get_le16(buf + ICOM_NETWORK_HEADER_LEN + i *
                            4);
        size_t packet_length;
        const uint8_t *p;
        pthread_mutex_lock(&s->lock);
        p = icom_network_txbuf_get(&s->txbuf, sequence, &packet_length);

        if (p) { send(s->fd, p, packet_length, 0); }

        pthread_mutex_unlock(&s->lock);
    }
}

/* Reply to a ping request by echoing it with reply=0x01. */
static void icom_network_session_handle_ping(struct icom_network_session_socket
        *s, uint8_t *buf, int length)
{
    if (length < 0x15) { return; }

    if (buf[0x10] == 0x00) /* request -> respond */
    {
        buf[0x10] = 0x01;
        /* swap local_id/remote_id so it routes back */
        icom_network_put_be32(buf + ICOM_NETWORK_OFF_LOCAL_ID, s->local_id);
        icom_network_put_be32(buf + ICOM_NETWORK_OFF_REMOTE_ID, s->remote_id);
        (void)send(s->fd, buf, length, 0);
    }
}

/* ------------------------------------------------------------------ */
/* receive threads                                                     */
/* ------------------------------------------------------------------ */

/* The part of the receive loop all three sockets share: anything arriving at
 * all is proof the radio is alive, and pings and retransmit requests are
 * answered the same way whichever socket they came in on. Returns non-zero
 * when the packet was one of those and has been dealt with, leaving each
 * thread to handle only the kind that is its own.
 *
 * *kind is set for the caller either way, so classification happens once.
 * buf is not const: a ping request is turned into its reply in place.
 *
 * s may be NULL, which is how the handshake uses it: icom_network_session_await
 * runs before the session has threads or a liveness clock to stamp, but the
 * radio still will not advance unless its pings are answered. */
static int icom_network_session_rx_common(struct icom_network_session *s,
        struct icom_network_session_socket *sock,
        uint8_t *buf, int n,
        enum icom_network_packet_kind *kind)
{
    if (s != NULL)
    {
        /* anything at all from the radio counts as proof of life */
        s->last_heard_ms = icom_network_now_ms();
    }

    *kind = icom_network_packet_classify(buf, n);

    switch (*kind)
    {
    case ICOM_NETWORK_PACKET_KIND_PING:
        icom_network_session_handle_ping(sock, buf, n);
        return 1;

    case ICOM_NETWORK_PACKET_KIND_RETRANSMIT:
        icom_network_session_handle_retransmit_request(sock, buf, n);
        return 1;

    default:
        return 0;
    }
}


/* Unsolicited status: the radio is ending the session, most often because
 * another client has taken it. The solicited one is consumed by the handshake
 * before the control thread runs. */
static void icom_network_session_handle_status(struct icom_network_session *s,
        const uint8_t *buf, int n)
{
    struct icom_network_packet_status status;

    int parsed = icom_network_packet_parse_status(buf, n, &status);

    if (parsed == RIG_OK && status.disconnect)
    {
        icom_network_session_mark_lost(s, RIG_COMM_REASON_PEER_DISCONNECT);
    }
}

static void *icom_network_session_control_thread(void *arg)
{
    struct icom_network_session *s = arg;
    int64_t last_idle = 0, last_ping = 0,
            last_token = icom_network_now_ms();
    uint8_t buf[ICOM_NETWORK_SESSION_PACKET_MAX];

    while (!s->stop)
    {
        int64_t now;
        int n = icom_network_session_recv_timeout(s->control.fd, buf, sizeof(buf), 50);

        if (n > 0)
        {
            enum icom_network_packet_kind k;

            int handled = icom_network_session_rx_common(s, &s->control, buf, n,
                          &k);

            if (!handled && k == ICOM_NETWORK_PACKET_KIND_STATUS)
            {
                icom_network_session_handle_status(s, buf, n);
            }
        }

        now = icom_network_now_ms();

        /* One thread does the silence check, on the socket that is guaranteed
         * to be exchanging keepalives regardless of what the app is doing. */
        if (s->config.liveness_timeout_ms > 0
                && now - s->last_heard_ms > s->config.liveness_timeout_ms)
        {
            icom_network_session_mark_lost(s, RIG_COMM_REASON_LINK_TIMEOUT);
        }

        if (now - last_idle >= ICOM_NETWORK_SESSION_IDLE_MS)
        {
            (void)icom_network_session_send_control(&s->control, ICOM_NETWORK_CTL_IDLE);
            last_idle = now;
        }

        if (now - last_ping >= ICOM_NETWORK_SESSION_PING_MS)
        {
            uint8_t packet[0x15];
            uint16_t sequence;
            pthread_mutex_lock(&s->control.lock);
            sequence = s->control.untracked_sequence++; /* ping is untracked */
            pthread_mutex_unlock(&s->control.lock);
            (void)icom_network_packet_build_ping(packet, sizeof(packet), 0x00,
                                           (uint32_t)now, sequence,
                                           s->control.local_id,
                                           s->control.remote_id);
            (void)icom_network_session_send_tracked(&s->control, packet, sizeof(packet), 0);
            last_ping = now;
        }

        if (now - last_token >= ICOM_NETWORK_SESSION_TOKEN_RENEW_MS)
        {
            uint8_t packet[0x40];
            uint16_t sequence;
            pthread_mutex_lock(&s->control.lock);
            sequence = s->control.send_sequence++;
            pthread_mutex_unlock(&s->control.lock);
            icom_network_packet_build_token(packet, sizeof(packet),
                                            ICOM_NETWORK_TOKEN_RENEW,
                                            s->auth_inner_sequence++, s->token_request,
                                            s->token, s->radio_identity, sequence,
                                            s->control.local_id,
                                            s->control.remote_id);
            (void)icom_network_session_send_tracked(&s->control, packet, sizeof(packet), 1);
            last_token = now;
        }
    }

    return NULL;
}

/* Issue retransmit requests for any sequences now due on a socket. */
static void icom_network_session_request_retransmits(struct
        icom_network_session_socket *sock, int64_t now)
{
    uint16_t sequences[ICOM_NETWORK_SEQBUF_MAX];
    size_t n;

    n = icom_network_rxtrack_due(&sock->rxtrack, now,
                                 ICOM_NETWORK_SESSION_RETRANSMIT_MS,
                                 sequences, sizeof(sequences) / sizeof(sequences[0]));

    if (n > 0)
    {
        uint8_t packet[ICOM_NETWORK_HEADER_LEN + ICOM_NETWORK_SEQBUF_MAX * 4];
        int length = icom_network_packet_build_retransmit(packet, sizeof(packet),
                     sequences,
                     n, sock->local_id, sock->remote_id);

        if (length > 0) { send(sock->fd, packet, length, 0); }
    }
}

/* One CI-V data packet: recover the frame, keep the sequence tracker honest,
 * and decide who receives it. */
static void icom_network_session_civ_handle_frame(struct icom_network_session *s,
        const uint8_t *buf, int n)
{
    struct icom_network_packet_civ civ;
    uint8_t frame[ICOM_NETWORK_SESSION_CIV_FRAME_MAX];
    uint16_t sequence;
    size_t frame_length;
    int is_echo, consumed = 0;

    if (icom_network_packet_parse_civ(buf, n, &civ) != RIG_OK)
    {
        return;
    }

    sequence = icom_network_get_le16(buf + ICOM_NETWORK_OFF_SEQUENCE);
    frame_length = civ.payload_length;

    if (frame_length > sizeof(frame)) { frame_length = sizeof(frame); }

    memcpy(frame, civ.data, frame_length);

    /* The radio sometimes drops the second 0xFE preamble byte; restore it so
     * the address check below is reliable. */
    if (frame_length >= 2 && frame[0] == 0xfe && frame[1] != 0xfe
            && frame_length < sizeof(frame))
    {
        memmove(frame + 1, frame, frame_length);
        frame[0] = 0xfe;
        frame_length++;
    }

    int resync = icom_network_rxtrack_observe(&s->civ.rxtrack, sequence,
                 icom_network_now_ms());

    if (resync)
    {
        /* Too many outstanding gaps, or a jump far beyond the replay window:
         * the missing set can no longer be recovered by retransmits, so drop
         * it and resynchronise on the current sequence rather than asking
         * forever. */
        rig_debug(RIG_DEBUG_WARN, "%s: CI-V sequence resync at %u\n", __func__,
                  (unsigned)sequence);
        icom_network_rxtrack_reset(&s->civ.rxtrack);
        s->civ_resyncs++;
    }

    /* The radio echoes our own commands back, addressed to the radio
     * (FE FE <radio> <controller> ...); drop them so only responses and
     * unsolicited frames reach the transaction layer. */
    is_echo = frame_length >= 3 && frame[2] == s->radio_civ_addr;

    if (is_echo) { return; }

    /* Let the backend route async (spectrum) frames; anything it does not
     * consume is a response, queued for civ_recv. */
    if (s->async_cb)
    {
        consumed = s->async_cb(s->async_ctx, frame, frame_length);
    }

    if (!consumed)
    {
        icom_network_session_civ_rx_push(s, frame, frame_length);
    }
}

static void *icom_network_session_civ_thread(void *arg)
{
    struct icom_network_session *s = arg;
    int64_t last_idle = 0, last_ping = 0;
    uint8_t buf[ICOM_NETWORK_SESSION_PACKET_MAX];

    while (!s->stop)
    {
        int64_t now;
        int n = icom_network_session_recv_timeout(s->civ.fd, buf, sizeof(buf), 50);

        if (n > 0)
        {
            enum icom_network_packet_kind k;

            int handled = icom_network_session_rx_common(s, &s->civ, buf, n, &k);

            if (!handled && k == ICOM_NETWORK_PACKET_KIND_CIV)
            {
                icom_network_session_civ_handle_frame(s, buf, n);
            }
        }

        now = icom_network_now_ms();
        icom_network_session_request_retransmits(&s->civ, now);

        if (now - last_idle >= ICOM_NETWORK_SESSION_IDLE_MS)
        {
            (void)icom_network_session_send_control(&s->civ, ICOM_NETWORK_CTL_IDLE);
            last_idle = now;
        }

        /* The radio needs periodic pings on the CI-V stream too (the same as
         * the control and audio streams); without them it treats the stream as
         * unsynced and intermittently stops answering CI-V commands. */
        if (now - last_ping >= ICOM_NETWORK_SESSION_PING_MS)
        {
            uint8_t packet[0x15];
            uint16_t sequence;
            pthread_mutex_lock(&s->civ.lock);
            sequence = s->civ.untracked_sequence++;
            pthread_mutex_unlock(&s->civ.lock);
            (void)icom_network_packet_build_ping(packet, sizeof(packet), 0x00, (uint32_t)now,
                                           sequence, s->civ.local_id,
                                           s->civ.remote_id);
            (void)icom_network_session_send_tracked(&s->civ, packet, sizeof(packet), 0);
            last_ping = now;
        }

        pthread_mutex_lock(&s->civ.lock);
        icom_network_txbuf_purge(&s->civ.txbuf, now, ICOM_NETWORK_SESSION_PURGE_MS);
        pthread_mutex_unlock(&s->civ.lock);
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* audio receive queue + thread                                        */
/* ------------------------------------------------------------------ */

static void icom_network_session_audio_rx_push(struct icom_network_session *s,
        const uint8_t *data, size_t length)
{
    if (length > ICOM_NETWORK_SESSION_AUDIO_FRAME_MAX)
    {
        length = ICOM_NETWORK_SESSION_AUDIO_FRAME_MAX;
    }

    pthread_mutex_lock(&s->audio_rx_lock);

    if (s->audio_rx_count == ICOM_NETWORK_SESSION_AUDIO_QUEUE_LENGTH)
    {
        /* drop the oldest frame to keep the freshest audio */
        s->audio_rx_tail = (s->audio_rx_tail + 1) %
                           ICOM_NETWORK_SESSION_AUDIO_QUEUE_LENGTH;
        s->audio_rx_count--;
    }

    memcpy(s->audio_rx_q[s->audio_rx_head].data, data, length);
    s->audio_rx_q[s->audio_rx_head].length = length;
    s->audio_rx_head = (s->audio_rx_head + 1) %
                       ICOM_NETWORK_SESSION_AUDIO_QUEUE_LENGTH;
    s->audio_rx_count++;
    pthread_cond_signal(&s->audio_rx_cond);

    pthread_mutex_unlock(&s->audio_rx_lock);
}

/* One audio data packet: keep the sequence tracker honest and hand the payload
 * to the ring. */
static void icom_network_session_audio_handle_frame(struct icom_network_session *s,
        const uint8_t *buf, int n)
{
    struct icom_network_packet_audio au;
    uint16_t sequence;

    if (icom_network_packet_parse_audio(buf, n, &au) != RIG_OK)
    {
        return;
    }

    sequence = icom_network_get_le16(buf + ICOM_NETWORK_OFF_SEQUENCE);

    int resync = icom_network_rxtrack_observe(&s->audio.rxtrack, sequence,
                 icom_network_now_ms());

    if (resync)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: audio sequence resync at %u\n", __func__,
                  (unsigned)sequence);
        icom_network_rxtrack_reset(&s->audio.rxtrack);
        s->audio_resyncs++;
    }

    icom_network_session_audio_rx_push(s, au.data, au.payload_length);
}

static void *icom_network_session_audio_thread(void *arg)
{
    struct icom_network_session *s = arg;
    int64_t last_ping = 0;
    uint8_t buf[ICOM_NETWORK_SESSION_PACKET_MAX];

    while (!s->stop && s->audio_active)
    {
        int64_t now;
        int n = icom_network_session_recv_timeout(s->audio.fd, buf, sizeof(buf), 50);

        if (n > 0)
        {
            enum icom_network_packet_kind k;

            int handled = icom_network_session_rx_common(s, &s->audio, buf, n,
                          &k);

            if (!handled && k == ICOM_NETWORK_PACKET_KIND_AUDIO)
            {
                icom_network_session_audio_handle_frame(s, buf, n);
            }
        }

        now = icom_network_now_ms();
        icom_network_session_request_retransmits(&s->audio, now);

        /* The audio stream keeps alive with pings (not idles); the radio also
         * derives this stream's clock from them and uses it to decide whether
         * incoming TX audio is fresh; without pings it drops all
         * our TX audio. */
        if (now - last_ping >= ICOM_NETWORK_SESSION_AUDIO_PING_MS)
        {
            uint8_t packet[0x15];
            uint16_t sequence;
            pthread_mutex_lock(&s->audio.lock);
            sequence = s->audio.untracked_sequence++;
            pthread_mutex_unlock(&s->audio.lock);
            (void)icom_network_packet_build_ping(packet, sizeof(packet), 0x00,
                                           (uint32_t)now, sequence,
                                           s->audio.local_id,
                                           s->audio.remote_id);
            (void)icom_network_session_send_tracked(&s->audio, packet, sizeof(packet), 0);
            last_ping = now;
        }

        pthread_mutex_lock(&s->audio.lock);
        icom_network_txbuf_purge(&s->audio.txbuf, now, ICOM_NETWORK_SESSION_PURGE_MS);
        pthread_mutex_unlock(&s->audio.lock);
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

struct icom_network_session *
icom_network_session_alloc(const struct icom_network_session_config *config)
{
    struct icom_network_session *s = calloc(1, sizeof(*s));

    if (s == NULL) { return NULL; }

    s->config = *config;
    s->control.fd = -1;
    s->civ.fd = -1;
    s->audio.fd = -1;
    /* The socket locks are initialised here rather than in socket_open so that
     * they pair with the destroys in session_free: a connect that fails before
     * a socket is ever opened must not leave an uninitialised mutex to destroy. */
    pthread_mutex_init(&s->control.lock, NULL);
    pthread_mutex_init(&s->civ.lock, NULL);
    pthread_mutex_init(&s->audio.lock, NULL);
    pthread_mutex_init(&s->civ_rx_lock, NULL);
    pthread_cond_init(&s->civ_rx_cond, NULL);
    pthread_mutex_init(&s->audio_rx_lock, NULL);
    pthread_cond_init(&s->audio_rx_cond, NULL);

    if (s->config.control_port == 0)
    {
        s->config.control_port = ICOM_NETWORK_PORT_CONTROL;
    }

    return s;
}

void icom_network_session_set_async_cb(struct icom_network_session *s,
                                       icom_network_async_cb cb, void *ctx)
{
    s->async_cb = cb;
    s->async_ctx = ctx;
}

void icom_network_session_set_lost_cb(struct icom_network_session *s,
                                      icom_network_lost_cb cb, void *ctx)
{
    s->lost_cb = cb;
    s->lost_ctx = ctx;
}

const struct icom_network_session_config *
icom_network_session_config(const struct icom_network_session *s)
{
    return &s->config;
}

/* Record that the session has stopped working. First reason wins: the cause
 * that got here first is the one that actually broke it. */
static void icom_network_session_mark_lost(struct icom_network_session *s,
        unsigned reason)
{
    if (s->lost) { return; }

    s->lost = 1;
    s->loss_reason = reason;

    rig_debug(RIG_DEBUG_ERR, "%s: session lost (%s)\n", __func__,
              rig_strcommreason(reason));

    if (s->lost_cb) { s->lost_cb(s->lost_ctx, reason); }
}

unsigned icom_network_session_loss_reason(const struct icom_network_session *s)
{
    return s->loss_reason;
}

int icom_network_session_is_valid(const struct icom_network_session *s)
{
    return s != NULL && s->connected && !s->lost;
}

void icom_network_session_resync_counts(const struct icom_network_session *s,
                                        unsigned *civ, unsigned *audio)
{
    if (civ) { *civ = s->civ_resyncs; }

    if (audio) { *audio = s->audio_resyncs; }
}

int icom_network_session_tx_audio_available(const struct icom_network_session
        *s)
{
    return s->tx_audio_available;
}

/* Decode the capabilities response, pick the radio this session will use, and
 * check the configured sample rate against what that radio advertises.
 *
 * Selection is by net_radio_index when set, else by net_radio_name, else by the
 * model's own name. A radio's built-in server advertises exactly one radio; a
 * PC running RS-BA1 can advertise several, which is what the index and name
 * selectors are for. */
static int icom_network_session_apply_capabilities(
    struct icom_network_session *s, const uint8_t *buf, size_t length)
{
    struct icom_network_packet_capabilities caps;
    const struct icom_network_packet_radio_cap *radio;
    const char *selector;
    char rates[128];
    char identity[32];
    int index;
    int i;

    int parsed = icom_network_packet_parse_capabilities(buf, length, &caps);

    if (parsed != RIG_OK || caps.radio_parsed == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no usable radio in capabilities response\n",
                  __func__);
        return -RIG_EPROTO;
    }

    if (caps.radio_count > caps.radio_parsed)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: server advertises %u radios, only the first %u are usable\n",
                  __func__, (unsigned)caps.radio_count,
                  (unsigned)caps.radio_parsed);
    }

    for (i = 0; i < (int)caps.radio_parsed; i++)
    {
        char tx_rates[128];
        const char *link;

        /* The link type is worth surfacing: dropouts on a streaming session
         * mean something different over WiFi than over Ethernet. */
        switch (caps.radio[i].connection_type)
        {
        case ICOM_NETWORK_CONNTYPE_WIFI:     link = "wifi";     break;

        case ICOM_NETWORK_CONNTYPE_ETHERNET: link = "ethernet"; break;

        default:                             link = "unknown";  break;
        }

        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: radio %d: name='%s' audio='%s' civ=0x%02x link=%s "
                  "rx_rates=[%s] tx_rates=[%s]\n",
                  __func__, i, caps.radio[i].name, caps.radio[i].audio,
                  caps.radio[i].civ_addr, link,
                  icom_network_rate_list(caps.radio[i].rx_rate, rates,
                                         sizeof(rates)),
                  icom_network_rate_list(caps.radio[i].tx_rate, tx_rates,
                                         sizeof(tx_rates)));
    }

    selector = s->config.radio_select_name[0] ? s->config.radio_select_name
               : s->config.radio_name;
    index = icom_network_select_radio(&caps, s->config.radio_index, selector);

    if (index < 0)
    {
        if (s->config.radio_index >= 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: net_radio_index %d out of range, server "
                      "advertises %u radio(s)\n", __func__,
                      s->config.radio_index, (unsigned)caps.radio_parsed);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: no advertised radio named '%s'; use net_radio_name or "
                      "net_radio_index to select one\n", __func__, selector);
        }

        /* The precise reason is in the log above; to the caller both are the
         * same thing: the configuration names a radio this server has not
         * offered. */
        return -RIG_ECONF;
    }

    radio = &caps.radio[index];

    if (strcasecmp(radio->name, s->config.radio_name) != 0)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: selected radio is '%s' but this backend is for '%s'; "
                  "commands may not match the radio\n",
                  __func__, radio->name, s->config.radio_name);
    }

    int rx_rate_ok = icom_network_rate_supported(radio->rx_rate,
                     (int)s->config.sample_rate);

    if (!rx_rate_ok)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: radio '%s' does not support %u Hz; advertised rates: %s\n",
                  __func__, radio->name, s->config.sample_rate,
                  icom_network_rate_list(radio->rx_rate, rates, sizeof(rates)));
        return -RIG_ECONF;
    }

    s->tx_audio_available = radio->tx_rate != 0
                            && icom_network_rate_supported(radio->tx_rate,
                                (int)s->config.sample_rate);

    if (s->config.tx_enable && !s->tx_audio_available)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: radio '%s' advertises no TX audio at %u Hz (rates: %s); "
                  "transmit streaming disabled\n",
                  __func__, radio->name, s->config.sample_rate,
                  icom_network_rate_list(radio->tx_rate, rates, sizeof(rates)));
    }

    memcpy(s->radio_identity, radio->identity, sizeof(s->radio_identity));
    SNPRINTF(s->radio_name, sizeof(s->radio_name), "%s", radio->name);
    s->radio_civ_addr = radio->civ_addr;

    if (radio->use_mac)
    {
        SNPRINTF(identity, sizeof(identity), "mac %02x:%02x:%02x:%02x:%02x:%02x",
                 radio->mac_address[0], radio->mac_address[1],
                 radio->mac_address[2], radio->mac_address[3],
                 radio->mac_address[4], radio->mac_address[5]);
    }
    else
    {
        SNPRINTF(identity, sizeof(identity), "guid");
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: selected radio %d '%s' civ=0x%02x %s "
              "baud=%u\n", __func__, index, radio->name, radio->civ_addr,
              identity, radio->baudrate);

    return RIG_OK;
}

/* ------------------------------------------------------------------ */
/* connect() stages                                                    */
/*                                                                      */
/* The handshake is a fixed sequence of exchanges with the radio. Each   */
/* stage is its own function so the order stays readable and any one of  */
/* them can be reasoned about in isolation.                             */
/* ------------------------------------------------------------------ */

/* Open the control socket and complete the discovery exchange the radio
 * requires before it will accept a login. */
static int icom_network_session_stage_control(struct icom_network_session *s)
{
    uint8_t rbuf[ICOM_NETWORK_SESSION_PACKET_MAX];
    int response_length, ret;

    ret = icom_network_session_socket_open(&s->control, s->config.host,
                                           s->config.control_port);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_exchange(&s->control, ICOM_NETWORK_CTL_PROBE,
                                        ICOM_NETWORK_CTL_PRESENT, rbuf,
                                        sizeof(rbuf), &response_length);

    if (ret != RIG_OK) { return ret; }

    return icom_network_session_exchange(&s->control, ICOM_NETWORK_CTL_READY,
                                         ICOM_NETWORK_CTL_READY, rbuf,
                                         sizeof(rbuf), &response_length);
}

/* Authenticate. On success the session holds the token every later request
 * must carry. */
static int icom_network_session_stage_login(struct icom_network_session *s)
{
    uint8_t rbuf[ICOM_NETWORK_SESSION_PACKET_MAX];
    uint8_t packet[ICOM_NETWORK_SESSION_PACKET_MAX];
    struct icom_network_packet_login_response lr;
    int response_length, ret, packet_length;
    uint16_t sequence;

    s->auth_inner_sequence =
        0x30;   /* inner auth sequence starts at 0x30 (the value the radio expects) */
    /* Client-chosen request id, echoed back by the radio in every token
     * response so replies can be matched to requests. Any value works; this
     * one is a fixed arbitrary marker because a session only ever has one
     * token exchange in flight. */
    s->token_request = 0x1234;

    pthread_mutex_lock(&s->control.lock);
    sequence = s->control.send_sequence++;
    pthread_mutex_unlock(&s->control.lock);
    packet_length = icom_network_packet_build_login(packet, sizeof(packet),
                    s->config.username,
                    s->config.password, s->config.client_name,
                    s->auth_inner_sequence++, s->token_request, 0,
                    sequence, s->control.local_id,
                    s->control.remote_id);

    if (packet_length < 0)
    {
        return packet_length;
    }

    ret = icom_network_session_send_recv(&s->control, packet, packet_length,
                                         ICOM_NETWORK_PACKET_KIND_LOGIN_RESPONSE, rbuf,
                                         sizeof(rbuf), &response_length);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no login response\n", __func__);
        return ret;
    }

    int parsed = icom_network_packet_parse_login_response(rbuf, response_length,
                 &lr);

    if (parsed != RIG_OK || lr.error != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: login rejected, error=0x%08x token=0x%08x\n",
                  __func__, (unsigned)lr.error, (unsigned)lr.token);
        return -RIG_ESECURITY;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: login OK, token=0x%08x conn='%s'\n",
              __func__, (unsigned)lr.token, lr.connection);

    s->token = lr.token;

    return RIG_OK;
}

/* Create the session token; the radio answers with its capability list, from
 * which the radio to use is selected. */
static int icom_network_session_stage_capabilities(struct icom_network_session *s)
{
    uint8_t rbuf[ICOM_NETWORK_SESSION_PACKET_MAX];
    uint8_t packet[ICOM_NETWORK_SESSION_PACKET_MAX];
    int response_length, ret, packet_length;
    uint16_t sequence;

    pthread_mutex_lock(&s->control.lock);
    sequence = s->control.send_sequence++;
    pthread_mutex_unlock(&s->control.lock);
    packet_length = icom_network_packet_build_token(packet, sizeof(packet),
                    ICOM_NETWORK_TOKEN_CREATE,
                    s->auth_inner_sequence++, s->token_request,
                    s->token, NULL, sequence,
                    s->control.local_id,
                    s->control.remote_id);

    if (packet_length < 0)
    {
        return packet_length;
    }

    ret = icom_network_session_send_recv(&s->control, packet, packet_length,
                                         ICOM_NETWORK_PACKET_KIND_CAPABILITIES, rbuf,
                                         sizeof(rbuf), &response_length);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no capabilities response\n", __func__);
        return ret;
    }

    return icom_network_session_apply_capabilities(s, rbuf, response_length);
}

/* Create one data socket and record the local port, which connection_info has
 * to report to the radio so it knows where to send. */
static int icom_network_session_stage_open_data_socket(
    struct icom_network_session *s,
    struct icom_network_session_socket *sock, uint16_t *local_port)
{
    struct sockaddr_in sa;
    socklen_t addr_length = sizeof(sa);
    int ret = icom_network_session_socket_open(sock, s->config.host,
              s->config.control_port);

    if (ret != RIG_OK) { return ret; }

    getsockname(sock->fd, (struct sockaddr *)&sa, &addr_length);
    *local_port = ntohs(sa.sin_port);

    return RIG_OK;
}

/* Ask for the streams this session wants and read back the ports the radio
 * assigned to them. */
static int icom_network_session_stage_connection_info(
    struct icom_network_session *s)
{
    uint8_t rbuf[ICOM_NETWORK_SESSION_PACKET_MAX];
    uint8_t packet[ICOM_NETWORK_SESSION_PACKET_MAX];
    struct icom_network_connection_info_request connection_request;
    struct icom_network_packet_status st;
    int response_length, ret, packet_length;
    uint16_t sequence;

    memset(&connection_request, 0, sizeof(connection_request));
    connection_request.inner_sequence = s->auth_inner_sequence++;
    connection_request.token_request = s->token_request;
    connection_request.token = s->token;
    connection_request.identity = s->radio_identity;
    connection_request.name = s->radio_name;
    connection_request.username = s->config.username;
    connection_request.rx_enable = 1;
    connection_request.tx_enable = (s->config.tx_enable
                                    && s->tx_audio_available) ? 1 : 0;
    connection_request.rx_codec = s->config.rx_codec;
    connection_request.tx_codec = s->config.tx_codec;
    connection_request.rx_rate = s->config.sample_rate;
    connection_request.tx_rate = s->config.sample_rate;
    connection_request.civ_port = s->civ_local_port;
    connection_request.audio_port = s->audio_local_port;
    /* TX jitter-buffer length (ms): the radio uses it as the freshness window
     * for incoming TX audio. */
    connection_request.txbuffer_ms = s->config.tx_buffer_ms ?
                                     s->config.tx_buffer_ms :
                                     ICOM_NETWORK_DEFAULT_LATENCY_MS;
    connection_request.convert_audio = 1;

    pthread_mutex_lock(&s->control.lock);
    sequence = s->control.send_sequence++;
    pthread_mutex_unlock(&s->control.lock);
    packet_length = icom_network_packet_build_connection_info(packet,
                    sizeof(packet),
                    &connection_request, sequence,
                    s->control.local_id, s->control.remote_id);

    if (packet_length < 0)
    {
        return packet_length;
    }

    ret = icom_network_session_send_recv(&s->control, packet, packet_length,
                                         ICOM_NETWORK_PACKET_KIND_STATUS, rbuf, sizeof(rbuf),
                                         &response_length);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no status response\n", __func__);
        return ret;
    }

    int parsed = icom_network_packet_parse_status(rbuf, response_length, &st);

    if (parsed != RIG_OK || st.error != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: status error=0x%08x\n", __func__,
                  (unsigned)st.error);
        return -RIG_EPROTO;
    }

    s->civ_server_port = st.civ_port;
    s->audio_server_port = st.audio_port;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: status OK, civ_port=%u audio_port=%u\n",
              __func__, s->civ_server_port, s->audio_server_port);

    return RIG_OK;
}

/* Point a data socket at the port the radio assigned it. */
static int icom_network_session_stage_reconnect_data_socket(
    struct icom_network_session *s,
    struct icom_network_session_socket *sock, uint16_t port)
{
    struct sockaddr_in sa;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, s->config.host, &sa.sin_addr);

    if (connect(sock->fd, (struct sockaddr *)&sa, sizeof(sa)) != 0)
    {
        return -RIG_EIO;
    }

    sock->local_id = icom_network_session_local_id(sock->fd);
    sock->remote_id = 0;

    return RIG_OK;
}

/* Open the CI-V stream. The radio needs the full probe + ready handshake on
 * this socket too (same as the control socket) before it will accept the open
 * and start streaming CI-V; probe alone is not enough. */
static int icom_network_session_stage_civ_stream(struct icom_network_session *s)
{
    uint8_t rbuf[ICOM_NETWORK_SESSION_PACKET_MAX];
    uint8_t packet[ICOM_NETWORK_SESSION_PACKET_MAX];
    int response_length, ret, packet_length;
    uint16_t sequence;

    ret = icom_network_session_exchange(&s->civ, ICOM_NETWORK_CTL_PROBE,
                                        ICOM_NETWORK_CTL_PRESENT, rbuf,
                                        sizeof(rbuf),
                                        &response_length);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_exchange(&s->civ, ICOM_NETWORK_CTL_READY,
                                        ICOM_NETWORK_CTL_READY, rbuf,
                                        sizeof(rbuf),
                                        &response_length);

    if (ret != RIG_OK) { return ret; }

    pthread_mutex_lock(&s->civ.lock);
    sequence = s->civ.send_sequence++;
    pthread_mutex_unlock(&s->civ.lock);
    /* the builder returns the packet length, so it is also the send length */
    packet_length = icom_network_packet_build_openclose(packet, sizeof(packet),
                    0x04, s->civ_send_sequence, sequence,
                    s->civ.local_id, s->civ.remote_id);

    if (packet_length < 0)
    {
        return packet_length;
    }

    ret = icom_network_session_send_tracked(&s->civ, packet, packet_length, 1);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: cannot open the CI-V stream\n", __func__);
    }

    return ret;
}

/* Start the keepalive/retransmit threads. Without these the session never pings
 * and never receives, so the radio drops it seconds later; a failure here is
 * fatal to the connection. */
static int icom_network_session_stage_threads(struct icom_network_session *s)
{
    s->stop = 0;

    int control_started = pthread_create(&s->control.thread, NULL,
                                         icom_network_session_control_thread, s);

    if (control_started != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: cannot start control thread\n", __func__);
        return -RIG_EIO;
    }

    s->control.thread_running = 1;

    int civ_started = pthread_create(&s->civ.thread, NULL,
                                     icom_network_session_civ_thread, s);

    if (civ_started != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: cannot start CI-V thread\n", __func__);
        return -RIG_EIO;
    }

    s->civ.thread_running = 1;

    return RIG_OK;
}

/*
 * The radio buffers CI-V frames left undelivered by the previous client
 * and flushes them as soon as the new stream opens; a stale NAK among
 * them would be mistaken for the reply to this session's first command.
 * Nothing received before this session's first command can be a response
 * to it, so discard the response queue until it stays quiet. Unsolicited
 * async frames (spectrum, transceive) are routed by the receive thread's
 * callback and never enter this queue, so they are unaffected.
 */
static void icom_network_session_stage_drain_civ(struct icom_network_session *s)
{
    uint8_t rbuf[ICOM_NETWORK_SESSION_PACKET_MAX];
    int64_t drain_deadline = icom_network_now_ms() +
                             ICOM_NETWORK_SESSION_CIV_DRAIN_TOTAL_MS;
    int drained = 0;

    while (icom_network_now_ms() < drain_deadline)
    {
        int n = icom_network_civ_recv(s, rbuf, sizeof(rbuf),
                                      ICOM_NETWORK_SESSION_CIV_DRAIN_QUIET_MS);

        if (n <= 0)
        {
            break;
        }

        drained++;
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: drained stale CI-V frame %d (%d bytes, cmd 0x%02x)\n",
                  __func__, drained, n, n >= 5 ? rbuf[4] : 0);
    }
}

int icom_network_session_connect(struct icom_network_session *s)
{
    int ret;

    ret = icom_network_session_stage_control(s);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_stage_login(s);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_stage_capabilities(s);

    if (ret != RIG_OK) { return ret; }

    /* Both data sockets are created before connection_info, because the
     * request has to tell the radio which local ports to send to. */
    ret = icom_network_session_stage_open_data_socket(s, &s->civ,
            &s->civ_local_port);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_stage_open_data_socket(s, &s->audio,
            &s->audio_local_port);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_stage_connection_info(s);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_stage_reconnect_data_socket(s, &s->civ,
            s->civ_server_port);

    if (ret != RIG_OK) { return ret; }

    /* Audio is optional: an RX-only session, or one the radio gave no audio
     * port, still works for CI-V, so a failure here is not fatal. */
    if (s->audio_server_port != 0)
    {
        if (icom_network_session_stage_reconnect_data_socket(s, &s->audio,
                s->audio_server_port) != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: audio socket not connected\n", __func__);
        }
    }

    ret = icom_network_session_stage_civ_stream(s);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_stage_threads(s);

    if (ret != RIG_OK) { return ret; }

    s->connected = 1;
    s->lost = 0;
    s->loss_reason = RIG_COMM_REASON_NONE;
    /* Start the liveness clock now; the radio has just answered the whole
     * handshake, so this is the most recent proof of life there is. */
    s->last_heard_ms = icom_network_now_ms();

    icom_network_session_stage_drain_civ(s);

    if (s->config.auto_reconnect && !s->reconnect_running)
    {
        s->stop_reconnect = 0;

        if (pthread_create(&s->reconnect_thread, NULL,
                           icom_network_session_reconnect_thread, s) == 0)
        {
            s->reconnect_running = 1;
        }
        else
        {
            /* Not fatal: the session works, it just will not self-heal. */
            rig_debug(RIG_DEBUG_WARN, "%s: cannot start reconnect thread\n",
                      __func__);
        }
    }

    return RIG_OK;
}

int icom_network_civ_send(struct icom_network_session *s,
                          const unsigned char *frame, int length)
{
    uint8_t packet[ICOM_NETWORK_SESSION_PACKET_MAX];
    uint16_t sequence, send_sequence;
    int packet_length;

    if (!s->connected || length <= 0
            || (size_t)length > ICOM_NETWORK_SESSION_PACKET_MAX - 0x15)
    {
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&s->civ.lock);
    sequence = s->civ.send_sequence++;
    send_sequence = s->civ_send_sequence++;
    packet_length = icom_network_packet_build_civ(packet, sizeof(packet), frame,
                    (size_t)length,
                    0xc1, send_sequence, sequence, s->civ.local_id,
                    s->civ.remote_id);

    if (packet_length > 0)
    {
        if (icom_network_txbuf_add(&s->civ.txbuf, sequence, packet,
                                   packet_length, icom_network_now_ms()) != 0)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: CI-V sequence %u not retained for retransmit\n",
                      __func__, (unsigned)sequence);
        }

        (void)send(s->civ.fd, packet, packet_length, 0);
    }

    pthread_mutex_unlock(&s->civ.lock);

    return packet_length > 0 ? length : -RIG_EIO;
}

int icom_network_civ_recv(struct icom_network_session *s,
                          unsigned char *buf, size_t buffer_length, int timeout_ms)
{
    int ret = -RIG_ETIMEOUT;
    struct timespec ts;

    icom_network_deadline_from_now(&ts, timeout_ms);

    pthread_mutex_lock(&s->civ_rx_lock);

    while (s->civ_rx_count == 0)
    {
        if (pthread_cond_timedwait(&s->civ_rx_cond, &s->civ_rx_lock, &ts) != 0)
        {
            break;
        }
    }

    if (s->civ_rx_count > 0)
    {
        size_t n = s->civ_rx_q[s->civ_rx_tail].length;

        if (n > buffer_length) { n = buffer_length; }

        memcpy(buf, s->civ_rx_q[s->civ_rx_tail].data, n);
        s->civ_rx_tail = (s->civ_rx_tail + 1) % ICOM_NETWORK_SESSION_CIV_QUEUE_LENGTH;
        s->civ_rx_count--;
        ret = (int)n;
    }

    pthread_mutex_unlock(&s->civ_rx_lock);

    return ret;
}

int icom_network_audio_start(struct icom_network_session *s)
{
    uint8_t rbuf[ICOM_NETWORK_SESSION_PACKET_MAX];
    int response_length, ret;

    if (!s->connected || s->audio.fd < 0 || s->audio_server_port == 0)
    {
        return -RIG_EINVAL;
    }

    if (s->audio_active) { return RIG_OK; }

    /* audio stream open: just the probe + ready handshake. The
     * audio stream (unlike CI-V) uses no openclose packet; the radio starts
     * streaming after the handshake. */
    ret = icom_network_session_exchange(&s->audio, ICOM_NETWORK_CTL_PROBE,
                                        ICOM_NETWORK_CTL_PRESENT, rbuf,
                                        sizeof(rbuf), &response_length);

    if (ret != RIG_OK) { return ret; }

    ret = icom_network_session_exchange(&s->audio, ICOM_NETWORK_CTL_READY,
                                        ICOM_NETWORK_CTL_READY, rbuf,
                                        sizeof(rbuf), &response_length);

    if (ret != RIG_OK) { return ret; }

    s->audio_active = 1;

    if (pthread_create(&s->audio.thread, NULL, icom_network_session_audio_thread,
                       s) != 0)
    {
        s->audio_active = 0;
        return -RIG_EIO;
    }

    s->audio.thread_running = 1;
    return RIG_OK;
}

void icom_network_audio_stop(struct icom_network_session *s)
{
    uint8_t packet[0x16];
    uint16_t sequence;
    int packet_length;

    if (!s->audio_active) { return; }

    s->audio_active = 0;

    if (s->audio.thread_running)
    {
        pthread_join(s->audio.thread, NULL);
        s->audio.thread_running = 0;
    }

    /* tell the server to stop streaming */
    pthread_mutex_lock(&s->audio.lock);
    sequence = s->audio.send_sequence++;
    pthread_mutex_unlock(&s->audio.lock);
    packet_length = icom_network_packet_build_openclose(packet, sizeof(packet),
                    0x00, 0, sequence, s->audio.local_id, s->audio.remote_id);

    /* Neither failure is fatal -- we are stopping anyway -- but the radio then
     * holds the audio stream open for its own timeout and rejects a quick
     * reconnect, so both are worth naming separately. */
    if (packet_length < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: could not build the audio stream-close "
                  "packet\n", __func__);
    }
    else if (icom_network_session_send_tracked(&s->audio, packet, packet_length,
             0) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: audio stream close not sent\n", __func__);
    }
}

int icom_network_audio_recv(struct icom_network_session *s, unsigned char *buf,
                            size_t buffer_length, int timeout_ms)
{
    int ret = -RIG_ETIMEOUT;
    struct timespec ts;

    icom_network_deadline_from_now(&ts, timeout_ms);

    pthread_mutex_lock(&s->audio_rx_lock);

    while (s->audio_rx_count == 0)
    {
        if (pthread_cond_timedwait(&s->audio_rx_cond, &s->audio_rx_lock, &ts)
                != 0)
        {
            break;
        }
    }

    if (s->audio_rx_count > 0)
    {
        size_t n = s->audio_rx_q[s->audio_rx_tail].length;

        if (n > buffer_length) { n = buffer_length; }

        memcpy(buf, s->audio_rx_q[s->audio_rx_tail].data, n);
        s->audio_rx_tail = (s->audio_rx_tail + 1) %
                           ICOM_NETWORK_SESSION_AUDIO_QUEUE_LENGTH;
        s->audio_rx_count--;
        ret = (int)n;
    }

    pthread_mutex_unlock(&s->audio_rx_lock);

    return ret;
}

int icom_network_audio_send(struct icom_network_session *s,
                            const unsigned char *buf, size_t length)
{
    uint8_t packet[ICOM_NETWORK_SESSION_PACKET_MAX];
    uint16_t sequence, send_sequence;
    int packet_length;

    if (!s->audio_active || length == 0
            || length > ICOM_NETWORK_SESSION_PACKET_MAX - 0x18)
    {
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&s->audio.lock);
    sequence = s->audio.send_sequence++;
    send_sequence = s->audio_send_sequence++;
    packet_length = icom_network_packet_build_audio(packet, sizeof(packet), buf,
                    length, 0x0080,
                    send_sequence, sequence, s->audio.local_id,
                    s->audio.remote_id);

    {
        static int dbg = 0;

        if (dbg < 3 && packet_length > 0)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: TXPKT packet_length=%d | length@0=%02x%02x%02x%02x type@4=%02x%02x"
                      " sequence@6=%02x%02x local_id@8=%02x%02x%02x%02x"
                      " remote_id@c=%02x%02x%02x%02x | identifier@10=%02x%02x"
                      " send_sequence@12=%02x%02x unused@14=%02x%02x payload_length@16=%02x%02x"
                      " | pcm@18=%02x%02x %02x%02x %02x%02x\n",
                      __func__, packet_length,
                      packet[0], packet[1], packet[2], packet[3], packet[4], packet[5],
                      packet[6], packet[7], packet[8], packet[9], packet[0x0a], packet[0x0b],
                      packet[0x0c], packet[0x0d], packet[0x0e], packet[0x0f],
                      packet[0x10], packet[0x11], packet[0x12], packet[0x13],
                      packet[0x14], packet[0x15], packet[0x16], packet[0x17],
                      packet[0x18], packet[0x19], packet[0x1a], packet[0x1b],
                      packet[0x1c], packet[0x1d]);
            dbg++;
        }
    }

    if (packet_length > 0)
    {
        if (icom_network_txbuf_add(&s->audio.txbuf, sequence, packet,
                                   packet_length, icom_network_now_ms()) != 0)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: audio sequence %u not retained for retransmit\n",
                      __func__, (unsigned)sequence);
        }

        (void)send(s->audio.fd, packet, packet_length, 0);
    }

    pthread_mutex_unlock(&s->audio.lock);

    return packet_length > 0 ? (int)length : -RIG_EIO;
}

/* Send an openclose "stop" (magic 0x00) on a data socket, as a tracked packet
 * so a retransmit request can be serviced. */
static void icom_network_session_send_stream_close(struct
        icom_network_session_socket *sock)
{
    uint8_t packet[0x16];
    uint16_t sequence;
    int packet_length;
    pthread_mutex_lock(&sock->lock);
    sequence = sock->send_sequence++;
    pthread_mutex_unlock(&sock->lock);
    packet_length = icom_network_packet_build_openclose(packet, sizeof(packet),
                    0x00, 0, sequence, sock->local_id, sock->remote_id);

    if (packet_length < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: could not build the stream-close packet\n",
                  __func__);
    }
    else if (icom_network_session_send_tracked(sock, packet, packet_length,
             1) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: stream close not sent\n", __func__);
    }
}

static void icom_network_session_teardown(struct icom_network_session *s)
{
    struct timespec ts;

    if (!s->connected) { return; }

    /* Tear down in the order the radio expects, and crucially send every
     * close/token-remove while the per-socket threads are still running, so
     * they can service the radio's retransmit requests for these tracked
     * packets. Closing after the threads stop would leave those requests
     * unanswered, and the radio then holds the CI-V (2 s) and audio (30 s)
     * streams open and rejects a quick reconnect. */

    /* close the CI-V stream (the audio stream uses no openclose; it is closed
     * by the control DISCONNECT below) */
    icom_network_session_send_stream_close(&s->civ);

    /* release the session slot with a token-remove on the control socket */
    if (s->token != 0)
    {
        uint8_t packet[0x40];
        uint16_t sequence;
        int packet_length;
        pthread_mutex_lock(&s->control.lock);
        sequence = s->control.send_sequence++;
        pthread_mutex_unlock(&s->control.lock);
        packet_length = icom_network_packet_build_token(packet, sizeof(packet),
                        ICOM_NETWORK_TOKEN_REMOVE,
                        s->auth_inner_sequence++, s->token_request,
                        s->token, NULL, sequence, s->control.local_id,
                        s->control.remote_id);

        /* Either way the radio keeps the session slot reserved until its own
         * timeout, so say which step failed. */
        if (packet_length < 0)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: could not build the token-remove "
                      "packet\n", __func__);
        }
        else if (icom_network_session_send_tracked(&s->control, packet,
                 packet_length, 1) != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: token remove not sent\n", __func__);
        }
    }

    /* Give the radio a moment to request retransmits of those tracked packets;
     * the still-running threads answer them. Skipped when the session is known
     * lost: the packets were still worth attempting in case the detection was
     * wrong, but waiting for answers that cannot come only delays rig_close(). */
    if (!s->lost)
    {
        ts.tv_sec = 0;
        ts.tv_nsec = ICOM_NETWORK_SESSION_DISCONNECT_GRACE_MS * 1000000L;
        nanosleep(&ts, NULL);
    }

    /* control DISCONNECT on every socket */
    if (s->audio.fd >= 0)
    {
        (void)icom_network_session_send_control(&s->audio, ICOM_NETWORK_CTL_DISCONNECT);
    }

    (void)icom_network_session_send_control(&s->civ, ICOM_NETWORK_CTL_DISCONNECT);
    (void)icom_network_session_send_control(&s->control, ICOM_NETWORK_CTL_DISCONNECT);

    /* now stop the threads */
    s->stop = 1;
    s->audio_active = 0;

    if (s->audio.thread_running)
    {
        pthread_join(s->audio.thread, NULL);
        s->audio.thread_running = 0;
    }

    if (s->control.thread_running)
    {
        pthread_join(s->control.thread, NULL);
        s->control.thread_running = 0;
    }

    if (s->civ.thread_running)
    {
        pthread_join(s->civ.thread, NULL);
        s->civ.thread_running = 0;
    }

    s->connected = 0;
}

/* Tear the transport down and stand it back up. The sockets must be closed
 * before connect() reopens them, or the old descriptors leak. */
static int icom_network_session_reestablish(struct icom_network_session *s)
{
    icom_network_session_teardown(s);
    icom_network_session_socket_close(&s->control);
    icom_network_session_socket_close(&s->civ);
    icom_network_session_socket_close(&s->audio);

    return icom_network_session_connect(s);
}

/* Opt-in background reconnect. Waits for the session to die, then retries the
 * full handshake with a growing delay until it succeeds or the session is torn
 * down for good. Open streams are not re-attached: the new session has fresh
 * sequence numbers and possibly different ports, so the application reopens
 * them. */
static void *icom_network_session_reconnect_thread(void *arg)
{
    struct icom_network_session *s = arg;
    int delay_ms = ICOM_NETWORK_SESSION_RECONNECT_MIN_MS;

    while (!s->stop_reconnect)
    {
        int64_t wake;

        if (!s->lost)
        {
            icom_network_session_sleep_ms(200);
            continue;
        }

        rig_debug(RIG_DEBUG_WARN, "%s: reconnecting in %d ms\n", __func__,
                  delay_ms);

        /* sleep in slices so a teardown does not wait out the whole backoff */
        wake = icom_network_now_ms() + delay_ms;

        while (!s->stop_reconnect && icom_network_now_ms() < wake)
        {
            icom_network_session_sleep_ms(100);
        }

        if (s->stop_reconnect) { break; }

        if (icom_network_session_reestablish(s) == RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: session re-established\n", __func__);
            delay_ms = ICOM_NETWORK_SESSION_RECONNECT_MIN_MS;

            if (s->lost_cb) { s->lost_cb(s->lost_ctx, RIG_COMM_REASON_NONE); }
        }
        else
        {
            delay_ms *= 2;

            if (delay_ms > ICOM_NETWORK_SESSION_RECONNECT_MAX_MS)
            {
                delay_ms = ICOM_NETWORK_SESSION_RECONNECT_MAX_MS;
            }
        }
    }

    return NULL;
}

void icom_network_session_disconnect(struct icom_network_session *s)
{
    /* Stop reconnecting first: a teardown is deliberate, and the thread would
     * otherwise race to rebuild what we are dismantling. */
    if (s->reconnect_running)
    {
        s->stop_reconnect = 1;
        pthread_join(s->reconnect_thread, NULL);
        s->reconnect_running = 0;
    }

    icom_network_session_teardown(s);
}

void icom_network_session_free(struct icom_network_session *s)
{
    if (s == NULL) { return; }

    if (s->connected) { icom_network_session_disconnect(s); }

    icom_network_session_socket_close(&s->control);
    icom_network_session_socket_close(&s->civ);
    icom_network_session_socket_close(&s->audio);
    pthread_mutex_destroy(&s->control.lock);
    pthread_mutex_destroy(&s->civ.lock);
    pthread_mutex_destroy(&s->audio.lock);
    pthread_mutex_destroy(&s->civ_rx_lock);
    pthread_cond_destroy(&s->civ_rx_cond);
    pthread_mutex_destroy(&s->audio_rx_lock);
    pthread_cond_destroy(&s->audio_rx_cond);
    free(s);
}
