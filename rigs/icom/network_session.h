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
/* Owns sockets + threads; bridges complete CI-V frames to the icom transaction */
/* layer and routes unsolicited (spectrum) frames to an async callback. */

#ifndef _ICOM_NETWORK_SESSION_H
#define _ICOM_NETWORK_SESSION_H 1

#include <stddef.h>
#include <stdint.h>

/* Opaque session handle. */
struct icom_network_session;

/* Connection configuration, populated from the backend config tokens. */
struct icom_network_session_config
{
    char     host[128];     /* radio IP / hostname */
    uint16_t control_port;  /* default ICOM_NETWORK_PORT_CONTROL (50001) */
    /* Sized to match the icom_priv_data fields they are copied from, so the
     * copy cannot truncate; the usable length is ICOM_NETWORK_PASSCODE_MAX and
     * is enforced when the config token is set. */
    char     username[24];
    char     password[24];
    char     client_name[17];  /* client name reported to the radio */
    char     radio_name[33];   /* this model's own name; the fallback selector
                                  and the cross-check for a mismatch warning */
    /* Milliseconds of silence from the radio before the session is treated as
     * lost. 0 disables the check, leaving loss to surface as command timeouts. */
    int      liveness_timeout_ms;
    int      auto_reconnect;   /* re-establish a lost session in the background */
    int      radio_index;      /* index into the advertised radio list,
                                  -1 = select by name */
    char     radio_select_name[33]; /* explicit name to select, empty = use
                                       radio_name */
    uint8_t  rx_codec;      /* enum icom_network_codec */
    uint8_t  tx_codec;
    uint32_t sample_rate;   /* negotiated audio/IQ sample rate (Hz) */
    uint32_t tx_buffer_ms;  /* TX jitter-buffer length sent to the radio (ms) */
    int      tx_enable;     /* request a TX audio session (0 = RX only) */
};

/*
 * Per-frame router for inbound CI-V frames. Called by the CI-V receive thread
 * for every complete FE FE .. FD frame from the radio. The backend inspects the
 * frame and, if it is an unsolicited async frame (spectrum scope, transceive
 * broadcast), handles it (e.g. icom_process_async_frame) and returns 1. For an
 * ordinary command response it returns 0 and the session queues the frame for
 * icom_network_civ_recv(). The frame is owned by the session for the call.
 */
typedef int (*icom_network_async_cb)(void *ctx, const unsigned char *frame,
                                     size_t length);

/* Allocate a session from a config. Does not open any socket yet. */
struct icom_network_session *
icom_network_session_alloc(const struct icom_network_session_config *config);

/* Register the unsolicited-frame callback (call before connect). */
void icom_network_session_set_async_cb(struct icom_network_session *s,
                                       icom_network_async_cb cb, void *ctx);

/* Called from a session thread when the session is lost, with a RIG_COMM_REASON_*
 * value, so the backend can report it without polling. */
typedef void (*icom_network_lost_cb)(void *ctx, unsigned reason);

/* Register the session-lost callback (call before connect). */
void icom_network_session_set_lost_cb(struct icom_network_session *s,
                                      icom_network_lost_cb cb, void *ctx);

/*
 * Resolved connection config (codecs / sample rate as requested at connect).
 * The Icom protocol does not renegotiate these, so the stream pumps read the
 * effective audio format back from here rather than re-deriving it.
 */
const struct icom_network_session_config *
icom_network_session_config(const struct icom_network_session *s);

/*
 * Open the control socket and run the full handshake (probe ->
 * login -> token create -> capabilities -> connection_info -> status) and bring the
 * CI-V socket up to OPEN. Returns RIG_OK once CI-V traffic can flow, or a
 * negative Hamlib error code.
 */
int icom_network_session_connect(struct icom_network_session *s);

/* Why a session stopped working, as a RIG_COMM_REASON_* value. */
unsigned icom_network_session_loss_reason(const struct icom_network_session *s);

/* Whether the session is still usable. False once the radio has disconnected us
 * or gone silent past the liveness timeout. */
int icom_network_session_is_valid(const struct icom_network_session *s);

/* Number of receive-sequence resyncs on each data socket since connect. A
 * resync means the link lost more than the replay window can recover, so a
 * rising count points at the network rather than at the radio. */
void icom_network_session_resync_counts(const struct icom_network_session *s,
                                        unsigned *civ, unsigned *audio);

/* Whether the selected radio advertises TX audio at the negotiated rate.
 * Valid only after a successful connect; a TX stream must not be opened when
 * this is 0. */
int icom_network_session_tx_audio_available(const struct icom_network_session *s);

/* Graceful disconnect (token remove + control disconnect), stop threads. */
void icom_network_session_disconnect(struct icom_network_session *s);

/* Free the session (disconnects first if still connected). */
void icom_network_session_free(struct icom_network_session *s);

/*
 * CI-V seam, called from rigs/icom/frame.c. `frame` is a complete FE FE .. FD
 * CI-V frame; the session wraps it in a CI-V data packet (reply 0xc1, payload_length,
 * send_sequence), assigns the socket sequence, and sends it. Returns `length` on
 * success or a negative Hamlib error code.
 */
int icom_network_civ_send(struct icom_network_session *s,
                          const unsigned char *frame, int length);

/*
 * Block up to timeout_ms for the next CI-V frame from the radio. On success
 * copies a complete FE FE .. FD frame into buf and returns its length;
 * -RIG_ETIMEOUT if none arrived in time. The transaction layer
 * (icom_one_transaction) sorts response vs interleaved async frames as it does
 * for serial; frames arriving between transactions go to the async callback.
 */
int icom_network_civ_recv(struct icom_network_session *s,
                          unsigned char *buf, size_t buffer_length, int timeout_ms);

/*
 * Audio stream (3rd UDP socket). The audio socket is set up during connect;
 * the flow is started on demand. start() runs the audio probe/open
 * handshake and a receive thread; stop() halts it. recv() returns the next
 * encoded audio payload (codec bytes) from the radio; send() transmits one.
 * The backend's stream pump decodes/encodes PCM and moves it to/from the ring
 * buffer.
 */
int icom_network_audio_start(struct icom_network_session *s);
void icom_network_audio_stop(struct icom_network_session *s);
int icom_network_audio_recv(struct icom_network_session *s, unsigned char *buf,
                            size_t buffer_length, int timeout_ms);
int icom_network_audio_send(struct icom_network_session *s,
                            const unsigned char *buf, size_t length);

#endif /* _ICOM_NETWORK_SESSION_H */
