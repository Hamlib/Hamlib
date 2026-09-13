/*
 *  Hamlib SmartSDR mock radio
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

/* In-process mock of a SmartSDR radio. See smartsdr_mock.h. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "smartsdr_mock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <hamlib/rig.h>
/* Socket headers come from stream_proto.h, which picks the right set for the
 * host; do not include them directly. */
#include "../src/stream_proto.h"
/* TCP_NODELAY: from <netinet/tcp.h> on POSIX, from winsock on Windows, which
 * stream_proto.h has already pulled in. */
#ifdef HAVE_SYS_SOCKET_H
#include <netinet/tcp.h>
#endif
#include <sys/time.h>


/* A radio the backend can open: slice A in use, tuned and reporting the
 * properties rig_open reads before it will hand the rig over. */
const char *const smartsdr_mock_default_slice =
    "slice 0 in_use=1 sample_rate=24000 RF_frequency=14.100000 "
    "client_handle=0x" SMARTSDR_MOCK_HANDLE " index_letter=A rit_on=0 "
    "rit_freq=0 xit_on=0 xit_freq=0 rxant=ANT1 mode=USB filter_lo=100 "
    "filter_hi=2800 step=10 agc_mode=med pan=0x40000000 txant=ANT1 lock=0 "
    "tx=1 active=1 audio_level=50 audio_pan=50 audio_mute=0 anf=0 nr=0 "
    "nr_level=0 nb=0 nb_level=0 wnb=0 apf=0 apf_level=0 squelch=1 "
    "squelch_level=20 diversity=0 rfgain=20 "
    "ant_list=ANT1,ANT2,RX_A,XVTA tx_ant_list=ANT1,ANT2,XVTA "
    "mode_list=LSB,USB,AM,CW,DIGL,DIGU,SAM,FM,NFM,RTTY";

/* What the radio says about itself, for the two commands rig_open asks. */
#define MOCK_INFO_BODY \
    "model=\"FLEX-6400M\" chassis_serial=\"1234-5678-9012-3456\" " \
    "callsign=\"MOCK\" nickname=\"mock\""
#define MOCK_VERSION_BODY \
    "SmartSDR-MB=3.4.0.9#PSoC-MBTRX=2.1.7#FPGA-MB=2.0.44"


static void mock_send(struct smartsdr_mock *m, const char *text)
{
    size_t len;

    if (m->client_fd < 0 || text == NULL)
    {
        return;
    }

    len = strlen(text);

    if (send(m->client_fd, text, len, 0) < 0)
    {
        /* The client went away between the select and here; the next read
         * sees the same thing and closes the connection. */
        return;
    }
}


/* Send one status line, prefixed the way the radio prefixes them. */
static void mock_send_status(struct smartsdr_mock *m, const char *body)
{
    char line[SMARTSDR_MOCK_LINE_LEN + 64];

    snprintf(line, sizeof(line), "S%s|%s\n", SMARTSDR_MOCK_HANDLE, body);
    mock_send(m, line);
}


/* The reply body a test asked for, or NULL for the mock's own answer. */

/* ------------------------------------------------------------------ */
/* The VITA-49 side                                                     */
/* ------------------------------------------------------------------ */

static int64_t mock_now_us(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}


/* The radio owns the VITA port, so the mock takes it while starting. Doing it
 * later would lose the race against the backend, which binds the same port for
 * its own receive socket and only falls back when something else holds it. */
static void mock_udp_open(struct smartsdr_mock *m)
{
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0)
    {
        return;
    }

    /* Deliberately no address reuse. The point of holding this port is that
     * the backend cannot have it and falls back to another, as it does against
     * a real radio; any reuse option hands it the port instead and it then
     * registers the radio's own endpoint as its own. */

    /* The wildcard address, not loopback: the backend binds the wildcard, and
     * a specific address does not collide with it. Binding narrowly would let
     * both hold the port, with datagrams going to the more specific socket. */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(4991);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        socket_close(fd);
        return;
    }

    m->udp_fd = fd;
}


/* Where to send: the client names its own port in "client udpport". */
static void mock_udp_learn(struct smartsdr_mock *m, uint16_t port)
{
    if (port == 0)
    {
        return;
    }

    pthread_mutex_lock(&m->lock);
    m->udp_port = port;
    pthread_mutex_unlock(&m->lock);
}


/* Fill in the seven header words every packet here shares. */
static int mock_vita_header(uint8_t *buf, uint32_t stream_id, uint16_t pcc,
                            uint8_t counter, int payload_bytes)
{
    uint32_t w0;
    uint32_t words = (uint32_t)((28 + payload_bytes) / 4);

    /* EXT_DATA with stream id and class id, no trailer, no timestamps. */
    w0 = (0x3u << 28) | (1u << 27) | ((uint32_t)(counter & 0x0F) << 16) | words;
    buf[0] = (uint8_t)(w0 >> 24); buf[1] = (uint8_t)(w0 >> 16);
    buf[2] = (uint8_t)(w0 >> 8);  buf[3] = (uint8_t)w0;
    buf[4] = (uint8_t)(stream_id >> 24); buf[5] = (uint8_t)(stream_id >> 16);
    buf[6] = (uint8_t)(stream_id >> 8);  buf[7] = (uint8_t)stream_id;
    /* class id: FlexRadio OUI in the upper half, packet class in the lower */
    buf[8] = 0x00; buf[9] = 0x00; buf[10] = 0x1C; buf[11] = 0x2D;
    buf[12] = 0x00; buf[13] = 0x00;
    buf[14] = (uint8_t)(pcc >> 8); buf[15] = (uint8_t)pcc;
    memset(buf + 16, 0, 12);
    return 28;
}


static void mock_udp_send(struct smartsdr_mock *m, const uint8_t *buf, int len)
{
    struct sockaddr_in to;
    int fd;
    uint16_t port;

    pthread_mutex_lock(&m->lock);
    fd = m->udp_fd;
    port = m->udp_port;
    pthread_mutex_unlock(&m->lock);

    if (fd < 0 || port == 0)
    {
        return;
    }

    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    to.sin_port = htons(port);
    sendto(fd, (const char *)buf, (size_t)len, 0,
           (struct sockaddr *)&to, sizeof(to));
}



void smartsdr_mock_swallow(struct smartsdr_mock *m, const char *prefix)
{
    pthread_mutex_lock(&m->lock);
    strncpy(m->swallow, prefix ? prefix : "", sizeof(m->swallow) - 1);
    m->swallow[sizeof(m->swallow) - 1] = '\0';
    pthread_mutex_unlock(&m->lock);
}


void smartsdr_mock_fail(struct smartsdr_mock *m, const char *prefix,
                        const char *code)
{
    pthread_mutex_lock(&m->lock);
    strncpy(m->fail_prefix, prefix ? prefix : "", sizeof(m->fail_prefix) - 1);
    m->fail_prefix[sizeof(m->fail_prefix) - 1] = '\0';
    strncpy(m->fail_code, code ? code : "50000001", sizeof(m->fail_code) - 1);
    m->fail_code[sizeof(m->fail_code) - 1] = '\0';
    pthread_mutex_unlock(&m->lock);
}


void smartsdr_mock_delay(struct smartsdr_mock *m, const char *prefix, int ms)
{
    pthread_mutex_lock(&m->lock);
    strncpy(m->delay_prefix, prefix ? prefix : "",
            sizeof(m->delay_prefix) - 1);
    m->delay_prefix[sizeof(m->delay_prefix) - 1] = '\0';
    m->delay_ms = ms;
    pthread_mutex_unlock(&m->lock);
}


int smartsdr_mock_wait_udp(struct smartsdr_mock *m, int timeout_ms)
{
    int waited;

    for (waited = 0; waited < timeout_ms; waited += 10)
    {
        int ready;

        pthread_mutex_lock(&m->lock);
        ready = m->udp_port != 0;
        pthread_mutex_unlock(&m->lock);

        if (ready)
        {
            return 1;
        }

        usleep(10 * 1000);
    }

    return 0;
}


void smartsdr_mock_send_samples(struct smartsdr_mock *m, uint32_t stream_id,
                                uint16_t pcc, uint8_t counter, int num_floats)
{
    uint8_t buf[8192];
    int payload = num_floats * 4;
    int n;
    int i;
    int is_iq;

    if (payload <= 0 || payload > (int)sizeof(buf) - 28)
    {
        return;
    }

    n = mock_vita_header(buf, stream_id, pcc, counter, payload);

    /* DAX I/Q is the odd one out on both counts, so the mock has to be too:
     * it is little-endian where every other payload is network order, and it
     * carries left-justified fixed point, so full scale is 32768 rather than
     * 1.0. Sending it any other way lets a backend that byte-swaps I/Q, or
     * forgets to normalise it, pass. */
    is_iq = (pcc >= 0x02E3 && pcc <= 0x02E6);

    for (i = 0; i < num_floats; i++)
    {
        union { float f; uint8_t b[4]; } v;
        uint8_t *p = buf + n + i * 4;

        if (is_iq)
        {
            /* +/-16384 counts, which is +/-0.5 once normalised. */
            v.f = (i & 1) ? 16384.0f : -16384.0f;
            p[0] = v.b[0]; p[1] = v.b[1]; p[2] = v.b[2]; p[3] = v.b[3];
        }
        else
        {
            v.f = (i & 1) ? 0.01f : -0.01f;
            p[0] = v.b[3]; p[1] = v.b[2]; p[2] = v.b[1]; p[3] = v.b[0];
        }
    }

    mock_udp_send(m, buf, n + payload);
}


void smartsdr_mock_send_meters(struct smartsdr_mock *m, uint32_t stream_id,
                               const uint16_t *ids, const int16_t *raw,
                               int count)
{
    uint8_t buf[1024];
    int payload = count * 4;
    int n;
    int i;

    if (count <= 0 || payload > (int)sizeof(buf) - 28)
    {
        return;
    }

    n = mock_vita_header(buf, stream_id, 0x8002, 0, payload);

    for (i = 0; i < count; i++)
    {
        uint8_t *p = buf + n + i * 4;

        p[0] = (uint8_t)(ids[i] >> 8); p[1] = (uint8_t)ids[i];
        p[2] = (uint8_t)((uint16_t)raw[i] >> 8); p[3] = (uint8_t)raw[i];
    }

    mock_udp_send(m, buf, n + payload);
}


void smartsdr_mock_send_fft(struct smartsdr_mock *m, uint32_t stream_id,
                            uint8_t counter, unsigned start_bin,
                            unsigned num_bins, unsigned total_bins,
                            uint32_t frame, unsigned fill)
{
    uint8_t buf[4096];
    int payload = 12 + (int)num_bins * 2;
    int n;
    unsigned i;

    if (num_bins == 0 || payload > (int)sizeof(buf) - 28)
    {
        return;
    }

    n = mock_vita_header(buf, stream_id, 0x8003, counter, payload);

    buf[n + 0] = (uint8_t)(start_bin >> 8);  buf[n + 1] = (uint8_t)start_bin;
    buf[n + 2] = (uint8_t)(num_bins >> 8);   buf[n + 3] = (uint8_t)num_bins;
    buf[n + 4] = 0; buf[n + 5] = 1;          /* bin size */
    buf[n + 6] = (uint8_t)(total_bins >> 8); buf[n + 7] = (uint8_t)total_bins;
    buf[n + 8]  = (uint8_t)(frame >> 24); buf[n + 9]  = (uint8_t)(frame >> 16);
    buf[n + 10] = (uint8_t)(frame >> 8);  buf[n + 11] = (uint8_t)frame;

    for (i = 0; i < num_bins; i++)
    {
        buf[n + 12 + i * 2] = (uint8_t)(fill >> 8);
        buf[n + 13 + i * 2] = (uint8_t)fill;
    }

    mock_udp_send(m, buf, n + payload);
}


void smartsdr_mock_capture_tx(struct smartsdr_mock *m)
{
    pthread_mutex_lock(&m->lock);
    m->tx_capture = 1;
    m->tx_count = 0;
    m->tx_bytes = 0;
    m->tx_first_us = 0;
    m->tx_last_us = 0;
    pthread_mutex_unlock(&m->lock);
}


int smartsdr_mock_tx_count(struct smartsdr_mock *m)
{
    int n;

    pthread_mutex_lock(&m->lock);
    n = m->tx_count;
    pthread_mutex_unlock(&m->lock);
    return n;
}


int64_t smartsdr_mock_tx_mean_interval_us(struct smartsdr_mock *m)
{
    int64_t span;
    int n;

    pthread_mutex_lock(&m->lock);
    n = m->tx_count;
    span = m->tx_last_us - m->tx_first_us;
    pthread_mutex_unlock(&m->lock);

    return n > 1 ? span / (n - 1) : 0;
}


static const char *mock_canned_body(struct smartsdr_mock *m, const char *cmd)
{
    const char *body = NULL;
    int i;

    pthread_mutex_lock(&m->lock);

    for (i = 0; i < m->reply_count; i++)
    {
        size_t n = strlen(m->replies[i].prefix);

        if (strncmp(cmd, m->replies[i].prefix, n) == 0)
        {
            body = m->replies[i].body;
            break;
        }
    }

    pthread_mutex_unlock(&m->lock);
    return body;
}


static void mock_record_command(struct smartsdr_mock *m, const char *cmd)
{
    pthread_mutex_lock(&m->lock);

    if (m->command_count < SMARTSDR_MOCK_MAX_COMMANDS)
    {
        strncpy(m->commands[m->command_count], cmd,
                sizeof(m->commands[0]) - 1);
        m->commands[m->command_count][sizeof(m->commands[0]) - 1] = '\0';
    }

    m->command_count++;
    pthread_mutex_unlock(&m->lock);
}


/* Answer one "C<seq>|<command>" line. */
static void mock_handle_command(struct smartsdr_mock *m, const char *line)
{
    const char *cmd = strchr(line, '|');
    const char *canned;
    char reply[1024];
    int seq = 0;

    if (cmd == NULL)
    {
        return;
    }

    sscanf(line, "C%d|", &seq);
    cmd++;

    mock_record_command(m, cmd);

    if (strncmp(cmd, "ping", 4) == 0)
    {
        m->ping_count++;
    }

    /* A silent radio holds the connection open and says nothing, which is what
     * a radio that lost power on the far side of a switch looks like. */
    if (m->go_silent)
    {
        return;
    }

    /* Subscribing to slices is answered with the slice status the radio would
     * push, before the R-line, as the radio sends it. */
    if (strncmp(cmd, "sub slice", 9) == 0)
    {
        pthread_mutex_lock(&m->lock);

        if (m->slice_status[0] != '\0')
        {
            char copy[SMARTSDR_MOCK_LINE_LEN];

            strncpy(copy, m->slice_status, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';
            pthread_mutex_unlock(&m->lock);
            mock_send_status(m, copy);
        }
        else
        {
            pthread_mutex_unlock(&m->lock);
        }
    }

    if (strncmp(cmd, "client udpport ", 15) == 0)
    {
        mock_udp_learn(m, (uint16_t)atoi(cmd + 15));
    }

    {
        char swallow[64];
        char delay_prefix[64];
        int delay_ms;

        pthread_mutex_lock(&m->lock);
        strncpy(swallow, m->swallow, sizeof(swallow) - 1);
        swallow[sizeof(swallow) - 1] = '\0';
        strncpy(delay_prefix, m->delay_prefix, sizeof(delay_prefix) - 1);
        delay_prefix[sizeof(delay_prefix) - 1] = '\0';
        delay_ms = m->delay_ms;
        pthread_mutex_unlock(&m->lock);

        if (swallow[0] != '\0'
                && strncmp(cmd, swallow, strlen(swallow)) == 0)
        {
            return;     /* answered by silence */
        }

        if (delay_prefix[0] != '\0' && delay_ms > 0
                && strncmp(cmd, delay_prefix, strlen(delay_prefix)) == 0)
        {
            usleep((useconds_t)delay_ms * 1000);
        }
    }

    {
        char fail_prefix[64], fail_code[16];

        pthread_mutex_lock(&m->lock);
        strncpy(fail_prefix, m->fail_prefix, sizeof(fail_prefix) - 1);
        fail_prefix[sizeof(fail_prefix) - 1] = '\0';
        strncpy(fail_code, m->fail_code, sizeof(fail_code) - 1);
        fail_code[sizeof(fail_code) - 1] = '\0';
        pthread_mutex_unlock(&m->lock);

        if (fail_prefix[0] != '\0'
                && strncmp(cmd, fail_prefix, strlen(fail_prefix)) == 0)
        {
            snprintf(reply, sizeof(reply), "R%d|%s|\n", seq, fail_code);
            mock_send(m, reply);
            return;
        }
    }

    canned = mock_canned_body(m, cmd);

    if (canned != NULL)
    {
        snprintf(reply, sizeof(reply), "R%d|0|%s\n", seq, canned);
    }
    else if (strncmp(cmd, "info", 4) == 0)
    {
        snprintf(reply, sizeof(reply), "R%d|0|%s\n", seq, MOCK_INFO_BODY);
    }
    else if (strncmp(cmd, "version", 7) == 0)
    {
        snprintf(reply, sizeof(reply), "R%d|0|%s\n", seq, MOCK_VERSION_BODY);
    }
    else
    {
        snprintf(reply, sizeof(reply), "R%d|0|\n", seq);
    }

    mock_send(m, reply);
}


/* Send whatever status a test has queued. */
static void mock_flush_status(struct smartsdr_mock *m)
{
    char pending[SMARTSDR_MOCK_MAX_STATUS][SMARTSDR_MOCK_LINE_LEN];
    int count;
    int i;

    if (m->client_fd < 0 || m->go_silent)
    {
        return;
    }

    pthread_mutex_lock(&m->lock);
    count = m->status_count;

    for (i = 0; i < count; i++)
    {
        memcpy(pending[i], m->status[i], sizeof(pending[i]));
    }

    m->status_count = 0;
    pthread_mutex_unlock(&m->lock);

    for (i = 0; i < count; i++)
    {
        mock_send_status(m, pending[i]);
    }
}


static void mock_accept(struct smartsdr_mock *m)
{
    int fd = accept(m->listen_fd, NULL, NULL);

    if (fd < 0)
    {
        return;
    }

    m->connections++;

    /* A radio that is up but not accepting sessions: the connection is taken
     * and dropped, which is what the client sees while the radio is busy. */
    if (m->refuse)
    {
        socket_close(fd);
        return;
    }

    {
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
    }

    m->client_fd = fd;

    /* The banner the radio sends unsolicited the moment a client connects. */
    mock_send(m, "V1.4.0.0\n");
    mock_send(m, "H" SMARTSDR_MOCK_HANDLE "\n");
    mock_send(m, "M10000001|Client connected from IP 127.0.0.1\n");
}


static void mock_drop_client(struct smartsdr_mock *m)
{
    if (m->client_fd >= 0)
    {
        socket_close(m->client_fd);
        m->client_fd = -1;
    }
}


static void *mock_run(void *arg)
{
    struct smartsdr_mock *m = (struct smartsdr_mock *)arg;
    char asm_buf[8192];
    size_t asm_len = 0;

    while (!m->stop)
    {
        fd_set r;
        struct timeval tv;
        int maxfd = m->listen_fd;
        int udp_fd;

        FD_ZERO(&r);
        FD_SET(m->listen_fd, &r);

        if (m->client_fd >= 0)
        {
            FD_SET(m->client_fd, &r);

            if (m->client_fd > maxfd)
            {
                maxfd = m->client_fd;
            }
        }

        pthread_mutex_lock(&m->lock);
        udp_fd = m->udp_fd;
        pthread_mutex_unlock(&m->lock);

        if (udp_fd >= 0)
        {
            FD_SET(udp_fd, &r);

            if (udp_fd > maxfd)
            {
                maxfd = udp_fd;
            }
        }

        mock_flush_status(m);

        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        if (select(maxfd + 1, &r, NULL, NULL, &tv) <= 0)
        {
            continue;
        }

        if (udp_fd >= 0 && FD_ISSET(udp_fd, &r))
        {
            uint8_t datagram[8192];
            ssize_t n = recv(udp_fd, (char *)datagram, sizeof(datagram), 0);

            if (n > 0)
            {
                pthread_mutex_lock(&m->lock);

                if (m->tx_capture)
                {
                    int64_t now = mock_now_us();

                    if (m->tx_count == 0)
                    {
                        m->tx_first_us = now;
                    }

                    m->tx_last_us = now;
                    m->tx_count++;
                    m->tx_bytes += (int)n;
                }

                pthread_mutex_unlock(&m->lock);
            }
        }

        if (FD_ISSET(m->listen_fd, &r))
        {
            /* One client at a time: a reconnect replaces the old session, as
             * it does on the radio. */
            mock_drop_client(m);
            asm_len = 0;
            mock_accept(m);
        }

        if (m->client_fd >= 0 && FD_ISSET(m->client_fd, &r))
        {
            char chunk[4096];
            ssize_t n = recv(m->client_fd, chunk, sizeof(chunk), 0);

            if (n <= 0)
            {
                mock_drop_client(m);
                asm_len = 0;
                continue;
            }

            if (asm_len + (size_t)n + 1U > sizeof(asm_buf))
            {
                asm_len = 0;
            }

            memcpy(asm_buf + asm_len, chunk, (size_t)n);
            asm_len += (size_t)n;
            asm_buf[asm_len] = '\0';

            for (;;)
            {
                char *nl = memchr(asm_buf, '\n', asm_len);
                char line[1024];
                size_t linelen;

                if (nl == NULL)
                {
                    break;
                }

                linelen = (size_t)(nl - asm_buf);

                if (linelen >= sizeof(line))
                {
                    linelen = sizeof(line) - 1;
                }

                memcpy(line, asm_buf, linelen);
                line[linelen] = '\0';

                {
                    size_t rest = asm_len - (size_t)(nl - asm_buf) - 1U;

                    memmove(asm_buf, nl + 1, rest);
                    asm_len = rest;
                    asm_buf[asm_len] = '\0';
                }

                if (line[0] == 'C')
                {
                    mock_handle_command(m, line);
                }
            }
        }
    }

    mock_drop_client(m);
    return NULL;
}


void smartsdr_mock_start(struct smartsdr_mock *m)
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
    struct sockaddr_in sa;
    socklen_t addr_length = sizeof(sa);
    int one = 1;

    memset(m, 0, sizeof(*m));
    m->client_fd = -1;
    m->udp_fd = -1;
    mock_udp_open(m);
    pthread_mutex_init(&m->lock, NULL);

    strncpy(m->slice_status, smartsdr_mock_default_slice,
            sizeof(m->slice_status) - 1);

    m->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(m->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one,
               sizeof(one));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;

    if (bind(m->listen_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0
            || listen(m->listen_fd, 4) < 0
            || getsockname(m->listen_fd, (struct sockaddr *)&sa,
                           &addr_length) < 0)
    {
        socket_close(m->listen_fd);
        m->listen_fd = -1;
        return;
    }

    m->port = ntohs(sa.sin_port);

    if (pthread_create(&m->thread, NULL, mock_run, m) != 0)
    {
        socket_close(m->listen_fd);
        m->listen_fd = -1;
        return;
    }

    m->started = 1;
}


void smartsdr_mock_stop(struct smartsdr_mock *m)
{
    if (!m->started)
    {
        return;
    }

    m->stop = 1;
    pthread_join(m->thread, NULL);

    if (m->udp_fd >= 0)
    {
        socket_close(m->udp_fd);
        m->udp_fd = -1;
        m->udp_port = 0;
    }

    if (m->listen_fd >= 0)
    {
        socket_close(m->listen_fd);
        m->listen_fd = -1;
    }

    pthread_mutex_destroy(&m->lock);
    m->started = 0;
}


void smartsdr_mock_pathname(const struct smartsdr_mock *m, char *out,
                            size_t out_len)
{
    snprintf(out, out_len, "127.0.0.1:%u", (unsigned)m->port);
}


void smartsdr_mock_reply(struct smartsdr_mock *m, const char *prefix,
                         const char *body)
{
    int i;

    pthread_mutex_lock(&m->lock);

    for (i = 0; i < m->reply_count; i++)
    {
        if (strcmp(m->replies[i].prefix, prefix) == 0)
        {
            break;
        }
    }

    if (i == m->reply_count)
    {
        if (m->reply_count >= SMARTSDR_MOCK_MAX_REPLIES)
        {
            pthread_mutex_unlock(&m->lock);
            return;
        }

        m->reply_count++;
    }

    strncpy(m->replies[i].prefix, prefix, sizeof(m->replies[i].prefix) - 1);
    m->replies[i].prefix[sizeof(m->replies[i].prefix) - 1] = '\0';
    strncpy(m->replies[i].body, body, sizeof(m->replies[i].body) - 1);
    m->replies[i].body[sizeof(m->replies[i].body) - 1] = '\0';

    pthread_mutex_unlock(&m->lock);
}


void smartsdr_mock_push_status(struct smartsdr_mock *m, const char *line)
{
    pthread_mutex_lock(&m->lock);

    if (m->status_count < SMARTSDR_MOCK_MAX_STATUS)
    {
        strncpy(m->status[m->status_count], line,
                sizeof(m->status[0]) - 1);
        m->status[m->status_count][sizeof(m->status[0]) - 1] = '\0';
        m->status_count++;
    }

    pthread_mutex_unlock(&m->lock);
}


/* Index of the first command containing needle, or -1. Order matters for a few
 * sequences the radio is sensitive to -- setting a mode resets that slice's
 * filters, so a width has to be sent after it, not before. */
static int smartsdr_mock_index_locked(struct smartsdr_mock *m,
                                      const char *needle)
{
    int i;
    int n = m->command_count < SMARTSDR_MOCK_MAX_COMMANDS
            ? m->command_count : SMARTSDR_MOCK_MAX_COMMANDS;

    for (i = 0; i < n; i++)
    {
        if (strstr(m->commands[i], needle) != NULL)
        {
            return i;
        }
    }

    return -1;
}


int smartsdr_mock_saw_before(struct smartsdr_mock *m, const char *first,
                             const char *second)
{
    int a;
    int b;

    pthread_mutex_lock(&m->lock);
    a = smartsdr_mock_index_locked(m, first);
    b = smartsdr_mock_index_locked(m, second);
    pthread_mutex_unlock(&m->lock);

    return (a >= 0 && b >= 0 && a < b);
}


int smartsdr_mock_count(struct smartsdr_mock *m, const char *needle)
{
    int found = 0;
    int i;
    int n;

    pthread_mutex_lock(&m->lock);
    n = m->command_count < SMARTSDR_MOCK_MAX_COMMANDS
        ? m->command_count : SMARTSDR_MOCK_MAX_COMMANDS;

    for (i = 0; i < n; i++)
    {
        if (strstr(m->commands[i], needle) != NULL)
        {
            found++;
        }
    }

    pthread_mutex_unlock(&m->lock);
    return found;
}


int smartsdr_mock_saw(struct smartsdr_mock *m, const char *needle)
{
    return smartsdr_mock_count(m, needle) > 0;
}


int smartsdr_mock_wait_for(struct smartsdr_mock *m, const char *needle,
                           int want, int timeout_ms)
{
    int waited = 0;
    int got = smartsdr_mock_count(m, needle);

    while (got < want && waited < timeout_ms)
    {
        usleep(50 * 1000);
        waited += 50;
        got = smartsdr_mock_count(m, needle);
    }

    return got;
}


void smartsdr_mock_forget(struct smartsdr_mock *m)
{
    pthread_mutex_lock(&m->lock);
    m->command_count = 0;
    memset(m->commands, 0, sizeof(m->commands));
    pthread_mutex_unlock(&m->lock);
}
