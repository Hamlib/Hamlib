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

/* In-process mock of a SmartSDR radio: a TCP listener on an ephemeral port
 * plus a thread that speaks the C/R/S protocol, so the session layer can be
 * exercised without hardware and without a separate simulator process. A test
 * can queue the reply to a command, push unsolicited status of its own, and
 * make the radio stop answering or refuse connections. */

#ifndef _SMARTSDR_MOCK_H
#define _SMARTSDR_MOCK_H 1

#include <stdint.h>
#include <pthread.h>

/* Client handle the mock hands out, which prefixes every status line it
 * sends. */
#define SMARTSDR_MOCK_HANDLE "1234ABCD"

#define SMARTSDR_MOCK_MAX_COMMANDS 256
#define SMARTSDR_MOCK_MAX_REPLIES   16
#define SMARTSDR_MOCK_MAX_STATUS    16
#define SMARTSDR_MOCK_LINE_LEN    2048

/* The reply body for a command whose prefix matches. An empty body is a
 * meaningful reply in its own right, so a matching entry always answers. */
struct smartsdr_mock_reply
{
    char prefix[64];
    char body[512];
};

struct smartsdr_mock
{
    int listen_fd;
    int client_fd;              /* -1 when no client is connected */
    uint16_t port;
    pthread_t thread;
    pthread_mutex_t lock;
    volatile int stop;
    volatile int started;

    /* Faults a test can inject. */
    volatile int go_silent;     /* hold the connection open, answer nothing */
    volatile int refuse;        /* accept a connection and close it at once */

    /* What the mock has seen. */
    volatile int connections;   /* clients that have connected so far */
    volatile int ping_count;    /* "ping" commands answered */
    int command_count;          /* commands seen, including any dropped */
    char commands[SMARTSDR_MOCK_MAX_COMMANDS][160];

    struct smartsdr_mock_reply replies[SMARTSDR_MOCK_MAX_REPLIES];
    int reply_count;

    /* Status lines queued by a test, sent as soon as a client is connected. */
    char status[SMARTSDR_MOCK_MAX_STATUS][SMARTSDR_MOCK_LINE_LEN];
    int status_count;

    /* The slice line the mock sends when the client subscribes to slices.
     * A test can replace it to describe a different radio. */
    char slice_status[SMARTSDR_MOCK_LINE_LEN];

    /* The radio sends every VITA-49 datagram to the one endpoint the client
     * registers with "client udpport", so the mock learns it from there. */
    int udp_fd;                 /* -1 until a client registers a port */
    uint16_t udp_port;          /* 0 until then */

    /* Faults a test can inject: a command whose reply never comes, and one
     * whose reply is late. Both are things a radio does under load and
     * neither can be provoked from a simulator that always answers. */
    char swallow[64];           /* prefix to leave unanswered; empty for none */
    char delay_prefix[64];      /* prefix whose reply is held back */
    int delay_ms;
    char fail_prefix[64];       /* prefix to refuse; empty for none */
    char fail_code[16];         /* status the refusal carries */

    /* What the client has transmitted, and when each datagram arrived, so a
     * test can judge whether the backend paces its transmit to real time. */
    int tx_capture;             /* set by a test to start recording */
    int tx_count;
    int tx_bytes;
    int64_t tx_first_us;
    int64_t tx_last_us;
};


/* The slice line the mock reports by default: slice A in use on 14.100 USB. */
extern const char *const smartsdr_mock_default_slice;

/* Start listening on an ephemeral loopback port and serve clients until
 * stopped. The port is in m->port once this returns. */
void smartsdr_mock_start(struct smartsdr_mock *m);

/* Stop serving and close every socket. */
void smartsdr_mock_stop(struct smartsdr_mock *m);

/* "127.0.0.1:<port>", for rig_pathname. */
void smartsdr_mock_pathname(const struct smartsdr_mock *m, char *out,
                            size_t out_len);

/* Answer any command starting with prefix with this R-line body. A later
 * call for the same prefix replaces the earlier one. */
void smartsdr_mock_reply(struct smartsdr_mock *m, const char *prefix,
                         const char *body);

/* Queue an unsolicited status line. The mock prefixes it with "S<handle>|"
 * and sends it as soon as a client is connected. */
void smartsdr_mock_push_status(struct smartsdr_mock *m, const char *line);

/* Whether any command the mock received contains needle. */
int smartsdr_mock_saw(struct smartsdr_mock *m, const char *needle);

/* How many commands the mock has received containing needle. */
int smartsdr_mock_count(struct smartsdr_mock *m, const char *needle);

/* True when both commands were seen and the first came before the second. */
int smartsdr_mock_saw_before(struct smartsdr_mock *m, const char *first,
                             const char *second);

/* Wait up to timeout_ms for the count of commands containing needle to reach
 * want. Returns the count reached, which is less than want on timeout. */
int smartsdr_mock_wait_for(struct smartsdr_mock *m, const char *needle,
                           int want, int timeout_ms);

/* Forget every command seen so far, so a later assertion covers only what
 * happened after this point. */
void smartsdr_mock_forget(struct smartsdr_mock *m);

/* Wait until the client has registered a UDP port, so datagrams sent after
 * this reach it. Returns 0 if none arrives within timeout_ms. */
int smartsdr_mock_wait_udp(struct smartsdr_mock *m, int timeout_ms);

/* Leave commands starting with prefix unanswered, as a radio that has stopped
 * responding to one thing but not the connection would. */
void smartsdr_mock_swallow(struct smartsdr_mock *m, const char *prefix);

/* Refuse any command starting with prefix, answering the given status instead
 * of success. Pass NULL to stop refusing. Lets a test drive the paths a real
 * radio reaches by rejecting a command. */
void smartsdr_mock_fail(struct smartsdr_mock *m, const char *prefix,
                        const char *code);

/* Hold back the reply to commands starting with prefix. */
void smartsdr_mock_delay(struct smartsdr_mock *m, const char *prefix, int ms);

/* Send one VITA-49 data packet of num_floats float32 samples under the given
 * class code, with the supplied 4-bit packet counter. A test controls the
 * counter so it can skip deliberately and prove a gap is seen.
 *
 * The encoding follows the radio: audio and everything else is big-endian at
 * +/-0.01, while DAX I/Q class codes (0x02E3-0x02E6) are little-endian counts
 * of +/-16384, which is +/-0.5 after the backend normalises by full scale. */
void smartsdr_mock_send_samples(struct smartsdr_mock *m, uint32_t stream_id,
                                uint16_t pcc, uint8_t counter,
                                int num_floats);

/* Send one meter packet: pairs of meter id and raw reading. */
void smartsdr_mock_send_meters(struct smartsdr_mock *m, uint32_t stream_id,
                               const uint16_t *ids, const int16_t *raw,
                               int count);

/* Send part of a panadapter FFT frame. Splitting one frame over several calls,
 * each with its own start_bin, is what the radio does and what exercises
 * reassembly. */
void smartsdr_mock_send_fft(struct smartsdr_mock *m, uint32_t stream_id,
                            uint8_t counter, unsigned start_bin,
                            unsigned num_bins, unsigned total_bins,
                            uint32_t frame, unsigned fill);

/* Start recording what the client transmits, and report the mean interval
 * between datagrams in microseconds (0 when fewer than two arrived). */
void smartsdr_mock_capture_tx(struct smartsdr_mock *m);
int smartsdr_mock_tx_count(struct smartsdr_mock *m);
int64_t smartsdr_mock_tx_mean_interval_us(struct smartsdr_mock *m);

#endif /* _SMARTSDR_MOCK_H */
