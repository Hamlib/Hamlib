/*
 *  Hamlib Icom network backend
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

/* Generic Icom LAN (network-protocol) backend: controls a radio over the Icom */
/* UDP protocol, reusing the CI-V backend with the network session as transport. */

#include "hamlib/config.h"

#include <string.h>
#include <stdlib.h>

#include <pthread.h>
#include <time.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "icom.h"
#include "icom_defs.h"
#include "network_session.h"
#include "network_proto.h"
#include "network_conf.h"
#include "icom_network.h"
#include "network_utils.h"
#include "stream.h"
#include "stream_ringbuf.h"
#include "stream_codec.h"
#include "stream_anchor.h"
#include "stream_proto.h"
#include "stream_time.h"

/* The radio frames its own audio in 20 ms chunks for every codec; TX uses the
 * same duration by default (net_tx_frame_ms overrides). */
#define ICOM_NETWORK_TX_FRAME_MS 20

/* Largest audio payload per packet (the radio splits larger frames, e.g. a
 * 1920-byte LPCM16 frame travels as 1364 + 556). */
#define ICOM_NETWORK_AUDIO_MAX_PAYLOAD 1364

/* One I/Q sample on the wire: a signed 16-bit I and Q pair. */
#define ICOM_NETWORK_IQ_FRAME_BYTES 4

/* The session queues audio payloads up to ICOM_NETWORK_SESSION_AUDIO_FRAME_MAX,
 * which is larger than the buffer above; icom_network_audio_recv() clamps to
 * the buffer it is given and drops the remainder, silently. That the radios do
 * not send payloads that large is an observation, not a guarantee: measured
 * across every codec and both stream types on an IC-7610 and an IC-9700, no
 * payload has ever exceeded this maximum. Raise this constant, do not lower
 * it, and if a radio ever appears that packetises differently the symptom will
 * be audible dropouts with no counter moving. */

static const struct icom_network_model *icom_network_find_model(
    rig_model_t model);
static int icom_network_publish_session_caps(RIG *rig,
        const struct icom_network_session_config *scfg);

/*
 * Route inbound CI-V frames from the session's receive thread: handle
 * unsolicited async frames (spectrum scope, transceive) here so the waterfall
 * flows during and between transactions; leave command responses (return 0)
 * for the transaction layer to read via the seam.
 */
static int icom_network_async_frame(void *ctx, const unsigned char *frame,
                                    size_t length)
{
    RIG *rig = (RIG *)ctx;

    if (icom_is_async_frame(rig, length, frame))
    {
        icom_process_async_frame(rig, length, frame);
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* rig open / close                                                    */
/* ------------------------------------------------------------------ */

/* Publish a change in the session's health where applications and the multicast
 * snapshot can see it. The session layer reports both directions through this
 * one callback: a reason names what went wrong, and RIG_COMM_REASON_NONE means
 * the reconnect thread has the session back. Treating every call as a loss
 * would leave a recovered rig looking disconnected for the rest of its life.
 * Runs on a session thread, so it only touches rig_state fields that are plain
 * scalars. */
static void icom_network_session_state_changed(void *ctx, unsigned reason)
{
    RIG *rig = ctx;

    STATE(rig)->comm_status = reason == RIG_COMM_REASON_NONE
                              ? RIG_COMM_STATUS_OK
                              : RIG_COMM_STATUS_DISCONNECTED;
    STATE(rig)->comm_reason = reason;
}

static int icom_network_open(RIG *rig)
{
    struct icom_priv_data *priv = (struct icom_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    struct icom_network_session_config config;
    struct icom_network_session *sess;
    char *colon;
    int ret;

    /* icom_rig_open() can re-enter this open on its internal retry; free any
     * session from a prior attempt so we never orphan one whose threads keep
     * the radio marked busy and refusing new connections. */
    if (priv->netsession)
    {
        icom_network_session_free(priv->netsession);
        priv->netsession = NULL;
    }

    memset(&config, 0, sizeof(config));
    /* the radio host comes from the -r/rig_pathname argument, host[:port] */
    strncpy(config.host, rp->pathname, sizeof(config.host) - 1);
    colon = strchr(config.host, ':');

    if (colon)
    {
        *colon = '\0';
        config.control_port = (uint16_t)atoi(colon + 1);
    }

    if (priv->net_control_port)
    {
        config.control_port = (uint16_t)priv->net_control_port;
    }

    strncpy(config.username, priv->net_username, sizeof(config.username) - 1);
    strncpy(config.password, priv->net_password, sizeof(config.password) - 1);
    strncpy(config.client_name, "hamlib", sizeof(config.client_name) - 1);

    const struct icom_network_model *m =
        icom_network_find_model(STATE(rig)->rig_model);

    if (m == NULL)
    {
        return -RIG_EINVAL;
    }

    strncpy(config.radio_name, m->radio_name, sizeof(config.radio_name) - 1);
    config.radio_index = priv->net_radio_index;
    /* net_liveness_timeout defaults to 0 in a zeroed priv, which would mean
     * "disabled"; an untouched token should get the built-in default instead. */
    config.liveness_timeout_ms = priv->net_liveness_timeout;
    config.auto_reconnect = priv->net_auto_reconnect;
    strncpy(config.radio_select_name, priv->net_radio_name,
            sizeof(config.radio_select_name) - 1);
    priv->net_model = m;
    /* Audio formats come from the config tokens; I/Q mode forces a stereo
     * LPCM16 RX stream (I=left, Q=right) unless an RX codec was set explicitly. */
    config.rx_codec = priv->net_rx_codec ? (uint8_t)priv->net_rx_codec
                      : (priv->net_iq_mode ? ICOM_NETWORK_CODEC_LPCM16S
                         : ICOM_NETWORK_CODEC_LPCM16);
    config.tx_codec = priv->net_tx_codec ? (uint8_t)priv->net_tx_codec
                      : ICOM_NETWORK_CODEC_LPCM16;
    config.sample_rate = priv->net_sample_rate ? (uint32_t)priv->net_sample_rate
                         : 48000;
    config.tx_buffer_ms = priv->net_tx_latency ? (uint32_t)priv->net_tx_latency :
                          ICOM_NETWORK_DEFAULT_LATENCY_MS;
    /* RX-only models (e.g. IC-R8600) never reserve a TX audio path; on TX
     * capable models the radio must reserve it at connect for a TX stream to
     * work later, so it defaults on (no audio transmits without PTT + a TX
     * stream) and the net_tx_enable token can force it off. */
    config.tx_enable = m->rx_only ? 0 : priv->net_tx_enable;

    sess = icom_network_session_alloc(&config);

    if (sess == NULL)
    {
        return -RIG_ENOMEM;
    }

    icom_network_session_set_async_cb(sess, icom_network_async_frame, rig);
    icom_network_session_set_lost_cb(sess, icom_network_session_state_changed,
                                     rig);

    ret = icom_network_session_connect(sess);

    if (ret != RIG_OK)
    {
        icom_network_session_free(sess);
        return ret;
    }

    priv->netsession = sess;
    /* The negotiated geometry is known only now, and no stream can open
     * before this returns, so this is the moment to publish what the session
     * can carry. Without it the frontend would resolve streams against the
     * model declaration, which describes every configuration rather than this
     * one -- and a stream opened on that basis reads the wire at the wrong
     * width. Refuse the open rather than serve it wrongly. */
    ret = icom_network_publish_session_caps(rig, &config);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: could not publish session caps: %s\n",
                  __func__, rigerror(ret));
        priv->netsession = NULL;
        icom_network_session_free(sess);
        return ret;
    }

    /* The session layer strips the radio's CI-V command echo, so the icom
     * transaction layer must not expect an echo of its own frames. */
    priv->serial_USB_echo_off = 1;

    /* run the standard icom open sequence; CI-V now flows over the session */
    ret = icom_rig_open(rig);

    if (ret != RIG_OK)
    {
        /* free so the disconnect (token-remove + DISCONNECT) releases the
         * radio's streams/slot instead of leaving them reserved */
        icom_network_session_free(priv->netsession);
        priv->netsession = NULL;
    }

    return ret;
}

static int icom_network_close(RIG *rig)
{
    struct icom_priv_data *priv = (struct icom_priv_data *)STATE(rig)->priv;

    icom_rig_close(rig);

    if (priv->netsession)
    {
        icom_network_session_free(priv->netsession);
        priv->netsession = NULL;
    }

    return RIG_OK;
}

/* ------------------------------------------------------------------ */
/* audio codecs and timing                                             */
/* ------------------------------------------------------------------ */

struct icom_network_stream_state
{
    RIG *rig;
    struct rig_stream *stream;
    struct icom_network_session *sess;
    struct rig_audio_codec_state *codec;
    pthread_t thread;
    volatile int running;   /* thread stop flag; read by the stream thread */
    unsigned tx_send_errors;  /* failed wire sends; first one is logged */
    int thread_started;
    int wire_channels;       /* negotiated wire codec channel count */
    int passthrough;         /* the wire payload already is this stream's own
                                bytes, so it crosses the ring untouched */
    size_t wire_frame_bytes; /* one frame on the wire, all channels */
    uint8_t silence_fill;    /* the byte that means silence in this stream's
                                format: 0 signed, 0x80 for unsigned bytes */
    uint32_t wire_rate;      /* negotiated wire sample rate (Hz) */
    uint8_t *pcm_buffer;        /* the stream's own format (one frame / one
                                   payload); unused on a passthrough stream */
    uint8_t *encode_buffer;        /* wire-encoded data */
    size_t pcm_buffer_length;
    size_t encode_buffer_length;
    size_t frame_bytes;      /* TX: bytes read per frame, in the stream's
                                own format */
};

/* Map a negotiated Icom network codec id to a Hamlib device codec. */
static rig_audio_codec_t icom_network_codec_map(uint8_t net_codec)
{
    switch (net_codec)
    {
    case ICOM_NETWORK_CODEC_PCMU:
    case ICOM_NETWORK_CODEC_PCMUS:
        return RIG_AUDIO_CODEC_MULAW;

    case ICOM_NETWORK_CODEC_ADPCM:
        return RIG_AUDIO_CODEC_ADPCM_IMA;

    default:                       /* LPCM family: 16-bit PCM passthrough */
        return RIG_AUDIO_CODEC_NONE;
    }
}

/* Channel count carried on the wire by a negotiated Icom network codec. */
static int icom_network_codec_channels(uint8_t net_codec)
{
    switch (net_codec)
    {
    case ICOM_NETWORK_CODEC_LPCM8S:
    case ICOM_NETWORK_CODEC_LPCM16S:
    case ICOM_NETWORK_CODEC_PCMUS:
        return 2;

    default:
        return 1;
    }
}

/* Linear PCM on the wire, in any of its widths. These are the codecs whose
 * payload already IS the stream's own bytes -- 8-bit unsigned or 16-bit
 * signed -- so nothing has to be done to them in either direction. The
 * companded and block codecs (uLaw, ADPCM) genuinely encode, and have to be
 * run through the codec layer. */
static int icom_network_codec_is_lpcm(uint8_t net_codec)
{
    switch (net_codec)
    {
    case ICOM_NETWORK_CODEC_LPCM:
    case ICOM_NETWORK_CODEC_LPCM8:
    case ICOM_NETWORK_CODEC_LPCM16:
    case ICOM_NETWORK_CODEC_LPCM8S:
    case ICOM_NETWORK_CODEC_LPCM16S:
        return 1;

    default:
        return 0;
    }
}

/* The 8-bit LPCM codecs carry UNSIGNED samples biased at 128, while the 16-bit
 * ones are signed -- so they are the wire's PCM_U8, not a narrower version of
 * the same thing. The device-link codec layer has no linear-8 codec (its
 * "none" passthrough is the S16 pivot itself), so these two go through the
 * frontend's U8 converter instead. */
static int icom_network_codec_is_linear8(uint8_t net_codec)
{
    return net_codec == ICOM_NETWORK_CODEC_LPCM8
           || net_codec == ICOM_NETWORK_CODEC_LPCM8S;
}

/* The one sample format a session serves without touching the bytes.
 *
 * For the 8-bit LPCM codecs that is PCM_U8: their wire samples already are
 * unsigned bytes, which is the point of choosing them -- half the network
 * traffic, and nothing to do at either end. Every other codec decodes to the
 * 16-bit pivot. Reaching any other format is the frontend's conversion, so
 * exactly one format belongs here: naming a second would claim the backend
 * serves it for free when it does not. uLaw is one byte on the wire too and
 * saves as much traffic, but it is companded rather than linear and Hamlib
 * has no uLaw sample format, so it only ever arrives through the pivot. */
static rig_stream_format_t icom_network_session_formats(uint8_t net_codec)
{
    return icom_network_codec_is_linear8(net_codec)
           ? RIG_STREAM_FORMAT_PCM_U8
           : RIG_STREAM_FORMAT_PCM_S16;
}

/* Bytes per sample carried on the wire by a negotiated Icom network codec. */
static int icom_network_codec_sample_bytes(uint8_t net_codec)
{
    switch (net_codec)
    {
    case ICOM_NETWORK_CODEC_PCMU:
    case ICOM_NETWORK_CODEC_PCMUS:
    case ICOM_NETWORK_CODEC_LPCM8:
    case ICOM_NETWORK_CODEC_LPCM8S:
        return 1;

    default:                       /* LPCM16 family */
        return 2;
    }
}

/* ------------------------------------------------------------------ */
/* audio stream threads                                                */
/* ------------------------------------------------------------------ */

/* Turn one received payload into the bytes this stream carries, and say how
 * many there are. Returns 0 when the payload yields nothing to write. */
static size_t icom_network_rx_decode(struct icom_network_stream_state *st,
                                     size_t received, const uint8_t **out)
{
    size_t pcm_bytes = 0;

    /* The stereo LPCM16 payload {L,R,...} already is IQ_CS16 {I,Q,...}. Hand
     * over whole complex frames only: a payload ending mid-frame would swap I
     * and Q for everything after it, which no consumer can detect. */
    if (stream_type_is_iq(st->stream->type))
    {
        *out = st->encode_buffer;
        return received - (received % ICOM_NETWORK_IQ_FRAME_BYTES);
    }

    /* Linear PCM on the wire is already this stream's own bytes, whatever its
     * width, so it goes to the ring as it arrived -- no codec, no copy. Hand
     * over whole frames only: a payload ending mid-frame would put the
     * channels out of phase for everything after it. */
    if (st->passthrough)
    {
        *out = st->encode_buffer;
        return received - (received % st->wire_frame_bytes);
    }

    /* The companded and block codecs genuinely decode, to the 16-bit pivot. */
    *out = st->pcm_buffer;

    if (rig_audio_convert_to_pcm(st->codec, st->encode_buffer, received,
                                 RIG_STREAM_FORMAT_PCM_S16, st->pcm_buffer,
                                 st->pcm_buffer_length, &pcm_bytes) != RIG_OK)
    {
        return 0;
    }

    return pcm_bytes;
}

/* A host-clock anchor about once a second, so a consumer can put the samples
 * on a wall clock. */
static void icom_network_rx_push_anchor(struct rig_stream *stream)
{
    struct rig_stream_time_anchor a;

    memset(&a, 0, sizeof(a));
    a.sample_index = rig_stream_get_samples_written(stream);
    stream_time_now(&a.seconds, &a.picoseconds);
    a.source = RIG_STREAM_TIME_SRC_HOST;
    a.accuracy = RIG_STREAM_TIME_ACC_MS;
    rig_stream_push_time_anchor(stream, &a);
}

static void *icom_network_rx_thread(void *arg)
{
    struct icom_network_stream_state *st = arg;
    struct rig_stream *stream = st->stream;
    int64_t last_anchor = 0;

    while (st->running)
    {
        const uint8_t *out = NULL;
        size_t length;
        int64_t now;
        int n;

        if (!icom_network_session_is_valid(st->sess))
        {
            /* Wake any blocked reader with -RIG_EIO: the samples are not
             * merely late, the radio is gone. */
            pthread_mutex_lock(&stream->ringbuf.lock);
            stream->ringbuf.failed = 1;
            pthread_cond_broadcast(&stream->ringbuf.data_available);
            pthread_mutex_unlock(&stream->ringbuf.lock);
            break;
        }

        n = icom_network_audio_recv(st->sess, st->encode_buffer,
                                    st->encode_buffer_length, 100);

        /* A timeout is ordinary: the anchor below still has to be pushed, so
         * this only skips the decode. */
        if (n > 0)
        {
            length = icom_network_rx_decode(st, (size_t)n, &out);

            if (length > 0)
            {
                (void)stream_backend_write(stream, out, length);
            }
        }

        now = icom_network_now_ms();

        if (now - last_anchor >= 1000)
        {
            icom_network_rx_push_anchor(stream);
            last_anchor = now;
        }
    }

    return NULL;
}

/* Fill one wire frame from the ring, padding a genuine underrun with silence.
 * Returns the bytes gathered, which is 0 when the producer had nothing at all. */
static size_t icom_network_tx_fill_frame(struct icom_network_stream_state *st,
                                         size_t frame_bytes)
{
    struct rig_stream *stream = st->stream;
    size_t got = 0;

    while (got < frame_bytes && st->running)
    {
        size_t n = stream_ringbuf_read(&stream->ringbuf, st->pcm_buffer + got,
                                       frame_bytes - got, 100);

        if (n == 0) { break; }   /* underrun: no data within the timeout */

        got += n;
    }

    /* A partial frame because the stream is closing is not an underrun: the
     * producer did not stall, the loop was told to stop. Padding and warning
     * about it would report a fault on every clean close. */
    if (got == 0 || got >= frame_bytes || !st->running) { return got; }

    rig_debug(RIG_DEBUG_WARN,
              "%s: TX underrun, padded %zu of %zu bytes with silence\n",
              __func__, frame_bytes - got, frame_bytes);
    /* Silence is the midpoint of the sample range, which is zero for the
     * signed pivot but 128 for unsigned bytes -- zero there is full-scale
     * negative, i.e. a click at every underrun. */
    memset(st->pcm_buffer + got, st->silence_fill, frame_bytes - got);

    return frame_bytes;
}

/* Encode one frame into what goes on the wire. Returns the wire length and
 * points *out at it; 0 means the frame produced nothing to send. */
static size_t icom_network_tx_encode(struct icom_network_stream_state *st,
                                     size_t frame_bytes, const uint8_t **out)
{
    size_t enc_bytes = 0;

    /* A linear-PCM stream already holds the wire layout, so there is no
     * encoding step at all: the frame goes out as it came off the ring. */
    if (st->passthrough)
    {
        *out = st->pcm_buffer;
        return frame_bytes;
    }

    *out = st->encode_buffer;

    if (rig_audio_convert_from_pcm(st->codec, RIG_STREAM_FORMAT_PCM_S16,
                                   st->pcm_buffer, frame_bytes,
                                   st->encode_buffer, st->encode_buffer_length,
                                   &enc_bytes) != RIG_OK)
    {
        return 0;
    }

    return enc_bytes;
}

/* Send one wire frame as max-payload packets (a 1920-byte LPCM16 frame travels
 * as 1364 + 556). */
static void icom_network_tx_send_fragments(struct icom_network_stream_state *st,
        const uint8_t *wire, size_t length)
{
    size_t off = 0;

    while (off < length)
    {
        size_t chunk = length - off;

        if (chunk > ICOM_NETWORK_AUDIO_MAX_PAYLOAD)
        {
            chunk = ICOM_NETWORK_AUDIO_MAX_PAYLOAD;
        }

        if (icom_network_audio_send(st->sess, wire + off, chunk) < 0)
        {
            /* Logged once: this runs per wire frame, so a persistent fault
             * would otherwise flood the log. The total is reported when the
             * stream closes. */
            if (st->tx_send_errors == 0)
            {
                rig_debug(RIG_DEBUG_WARN, "%s: TX audio send failed\n", __func__);
            }

            st->tx_send_errors++;
        }

        off += chunk;
    }
}

/* TX thread: pull one frame from the ring, encode it, and put it on the wire.
 * The application producer runs in real time, so a full frame becomes ready
 * about once per frame duration and that paces the radio's receive framing. */
static void *icom_network_tx_thread(void *arg)
{
    struct icom_network_stream_state *st = arg;
    size_t frame_bytes = st->frame_bytes;

    while (st->running)
    {
        const uint8_t *wire = NULL;
        size_t got, enc_bytes;

        if (!icom_network_session_is_valid(st->sess))
        {
            break;   /* nothing to transmit to */
        }

        got = icom_network_tx_fill_frame(st, frame_bytes);

        if (!st->running) { break; }

        if (got == 0) { continue; }  /* nothing produced; the ping keeps it up */

        enc_bytes = icom_network_tx_encode(st, frame_bytes, &wire);

        if (enc_bytes > 0)
        {
            icom_network_tx_send_fragments(st, wire, enc_bytes);
        }
    }

    return NULL;
}

/* Release everything a stream state owns. Every failure path in stream_open
 * and the normal teardown in stream_close go through here, so the buffers
 * cannot be forgotten on one path and freed on another. */
/* ------------------------------------------------------------------ */
/* stream open / close                                                 */
/* ------------------------------------------------------------------ */

static void icom_network_stream_state_free(struct icom_network_stream_state *st)
{
    if (st == NULL)
    {
        return;
    }

    rig_audio_codec_close(st->codec);
    free(st->pcm_buffer);
    free(st->encode_buffer);
    free(st);
}

/* Can this session serve the stream at all? Checked before anything is
 * allocated, so a refusal costs nothing. */
static int icom_network_stream_check_openable(struct rig_stream *stream,
        struct icom_network_session *sess)
{
    if (sess == NULL)
    {
        return -RIG_EIO;
    }

    if (!icom_network_session_is_valid(sess))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: session is not connected\n", __func__);
        return -RIG_EIO;
    }

    /* The model's stream capabilities are static, but whether a TX audio path
     * exists is only known once the radio has answered the capabilities
     * exchange, so the check belongs here rather than in the caps table. */
    int tx_available = icom_network_session_tx_audio_available(sess);

    if (!stream_type_is_rx(stream->type) && !tx_available)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: radio advertises no TX audio at the negotiated rate\n",
                  __func__);
        return -RIG_ENAVAIL;
    }

    return RIG_OK;
}


/* Resolve the wire geometry from the negotiated codec, open the codec and size
 * the two working buffers. On failure st owns whatever was allocated and the
 * caller frees it. */
static int icom_network_stream_setup(RIG *rig, struct rig_stream *stream,
                                     struct icom_network_stream_state *st)
{
    const struct icom_network_session_config *scfg =
        icom_network_session_config(st->sess);
    int is_rx = stream_type_is_rx(stream->type);
    uint8_t net_codec = is_rx ? scfg->rx_codec : scfg->tx_codec;
    int wire_sb = icom_network_codec_sample_bytes(net_codec);
    int codec_channels = icom_network_codec_channels(net_codec);

    /* backend_config is the configuration the frontend resolved this stream
     * to, and the negotiated codec is what the wire can carry. They agree
     * while the advertised caps describe the session, so a disagreement means
     * the caps are wrong. Refuse it: running at a geometry the codec cannot
     * represent produces audio at the wrong speed while every counter stays at
     * zero. I/Q is exempt because it rides a stereo codec as a single complex
     * channel. */
    rig_stream_format_t session_format = icom_network_session_formats(net_codec);

    if (!stream_type_is_iq(stream->type)
            && (stream->backend_config.sample_rate != (int)scfg->sample_rate
                || stream->backend_config.channels != codec_channels
                || stream->backend_config.format != session_format))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: stream resolved to %d Hz %d ch format 0x%x, but the "
                  "codec carries %u Hz %d ch format 0x%x\n", __func__,
                  stream->backend_config.sample_rate,
                  stream->backend_config.channels,
                  (unsigned)stream->backend_config.format,
                  (unsigned)scfg->sample_rate, codec_channels,
                  (unsigned)session_format);
        return -RIG_ECONF;
    }

    st->wire_rate = (uint32_t)scfg->sample_rate;
    st->wire_channels = codec_channels;
    /* A session declares exactly one native format, so the frontend always
     * resolves these streams to it and anything else the application wants is
     * the frontend's conversion on the far side of the ring. Where that format
     * is what the wire already carries -- the whole linear-PCM family -- the
     * payload needs nothing done to it in either direction. */
    st->passthrough = icom_network_codec_is_lpcm(net_codec);
    st->wire_frame_bytes = (size_t)wire_sb * (size_t)st->wire_channels;
    st->silence_fill =
        stream->backend_config.format == RIG_STREAM_FORMAT_PCM_U8 ? 0x80 : 0x00;

    /* The codec runs at the wire channel count. */
    st->codec = rig_audio_codec_open(icom_network_codec_map(net_codec),
                                     st->wire_channels);

    /* One unit of work each way, in this stream's own format: RX one packet
     * payload, TX one wire frame. Both hold native data only; reaching the
     * format, channel count and rate the application asked for belongs to the
     * frontend, on the far side of the ring. */
    if (is_rx)
    {
        st->encode_buffer_length = ICOM_NETWORK_AUDIO_MAX_PAYLOAD;
        /* A one-byte codec doubles in width when decoded to the S16 pivot, so
         * size for the widest case rather than the wire. */
        st->pcm_buffer_length = ICOM_NETWORK_AUDIO_MAX_PAYLOAD
                                / (size_t)wire_sb * 2;
        st->frame_bytes = 0;
    }
    else
    {
        int frame_ms = ((struct icom_priv_data *)STATE(rig)->priv)
                       ->net_tx_frame_ms;
        size_t per_ch;

        if (frame_ms <= 0)
        {
            frame_ms = ICOM_NETWORK_TX_FRAME_MS;
        }

        per_ch = (size_t)st->wire_rate * (size_t)frame_ms / 1000;

        if (per_ch == 0)
        {
            per_ch = 1;
        }

        /* The ring is read in the stream's own format: the wire's width when
         * nothing is converted, the 16-bit pivot otherwise. */
        st->frame_bytes = per_ch * (size_t)st->wire_channels
                          * (size_t)(st->passthrough ? wire_sb : 2);
        st->encode_buffer_length = per_ch * (size_t)st->wire_channels
                                   * (size_t)wire_sb;
        st->pcm_buffer_length = st->frame_bytes;
    }

    /* A passthrough stream works out of one buffer: receive fills the wire
     * buffer and hands it straight over, transmit fills the ring buffer and
     * sends it as it stands. Only a codec stream needs both, one for each side
     * of the decode. */
    if (st->passthrough)
    {
        if (is_rx) { st->pcm_buffer_length = 0; }
        else { st->encode_buffer_length = 0; }
    }

    st->pcm_buffer = st->pcm_buffer_length
                     ? malloc(st->pcm_buffer_length) : NULL;
    st->encode_buffer = st->encode_buffer_length
                        ? malloc(st->encode_buffer_length) : NULL;

    if (st->codec == NULL
            || (st->pcm_buffer_length && st->pcm_buffer == NULL)
            || (st->encode_buffer_length && st->encode_buffer == NULL))
    {
        return st->codec == NULL ? -RIG_EINVAL : -RIG_ENOMEM;
    }

    return RIG_OK;
}


static int icom_network_stream_open(RIG *rig, struct rig_stream *stream)
{
    struct icom_priv_data *priv = (struct icom_priv_data *)STATE(rig)->priv;
    struct icom_network_session *sess = priv->netsession;
    struct icom_network_stream_state *st;
    void *(*thread_fn)(void *);
    int ret;

    ret = icom_network_stream_check_openable(stream, sess);

    if (ret != RIG_OK)
    {
        return ret;
    }

    st = calloc(1, sizeof(*st));

    if (st == NULL)
    {
        return -RIG_ENOMEM;
    }

    st->rig = rig;
    st->stream = stream;
    st->sess = sess;

    ret = icom_network_stream_setup(rig, stream, st);

    if (ret != RIG_OK)
    {
        icom_network_stream_state_free(st);
        return ret;
    }

    /* bring the audio flow up (idempotent across RX/TX streams) */
    ret = icom_network_audio_start(sess);

    if (ret != RIG_OK)
    {
        icom_network_stream_state_free(st);
        return ret;
    }

    thread_fn = stream_type_is_rx(stream->type) ? icom_network_rx_thread
                : icom_network_tx_thread;
    st->running = 1;

    if (pthread_create(&st->thread, NULL, thread_fn, st) != 0)
    {
        st->running = 0;
        icom_network_stream_state_free(st);
        return -RIG_EIO;
    }

    st->thread_started = 1;
    stream->backend_priv = st;

    return RIG_OK;
}

static int icom_network_stream_close(RIG *rig, struct rig_stream *stream)
{
    struct icom_network_stream_state *st =
        (struct icom_network_stream_state *)stream->backend_priv;

    (void)rig;

    if (st == NULL)
    {
        return RIG_OK;
    }

    st->running = 0;

    if (st->thread_started)
    {
        pthread_join(st->thread, NULL);
    }

    if (st->tx_send_errors > 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: %u TX audio send failure(s) on this stream\n",
                  __func__, st->tx_send_errors);
    }

    stream->backend_priv = NULL;
    icom_network_stream_state_free(st);

    /* The session's audio flow is stopped at disconnect, so concurrent RX/TX
     * streams keep working; closing one stream does not tear down the other. */
    return RIG_OK;
}

#define ICOM_NETWORK_MAX_MODELS 16
/* ------------------------------------------------------------------ */
/* model registry and capabilities                                     */
/* ------------------------------------------------------------------ */

static const struct icom_network_model
    *icom_network_models[ICOM_NETWORK_MAX_MODELS];
/* The stream-caps buffer each model owns, parallel to the array above.
 * rig_caps hands it out as const, but this backend built it and narrows the
 * rates it advertises once a session has settled on one. */
static struct rig_stream_caps
    *icom_network_model_caps[ICOM_NETWORK_MAX_MODELS];
static int icom_network_model_count;

static const struct icom_network_model *icom_network_find_model(
    rig_model_t model)
{
    int i;

    for (i = 0; i < icom_network_model_count; i++)
    {
        if (icom_network_models[i]->rig_model == model)
        {
            return icom_network_models[i];
        }
    }

    return NULL;
}


/* A session settles on one audio rate and one codec per direction at connect,
 * and changing either means reconnecting, so until then they are the only
 * geometry this backend can produce or accept. Advertise exactly that and let
 * the frontend convert for any application wanting something else. Leaving
 * the hardware's whole range up would let the frontend resolve to a native
 * configuration no stream could be served at -- and a channel count is worse
 * than a rate, because handing stereo to a mono codec is not refused
 * anywhere: the wire bytes are simply read as twice as many mono samples.
 * The I/Q entry carries its own fixed geometry and is left alone.
 *
 * The buffer belongs to the model, not the rig, so two rigs of one model in a
 * single process share it -- the second connection's settings win. */
static int icom_network_publish_session_caps(RIG *rig,
        const struct icom_network_session_config *scfg)
{
    const struct rig_stream_caps *model = NULL;
    struct rig_stream_caps session[HAMLIB_MAX_STREAM_CAPS];
    /* Slots per wire frame, which is the audio channel count but only the
     * carrier for I/Q -- see the I/Q branch below. */
    int rx_codec_slots = icom_network_codec_channels(scfg->rx_codec);
    int tx_codec_slots = icom_network_codec_channels(scfg->tx_codec);
    int n = 0;
    int i;

    for (i = 0; i < icom_network_model_count; i++)
    {
        if (icom_network_models[i]->rig_model == STATE(rig)->rig_model)
        {
            model = icom_network_model_caps[i];
            break;
        }
    }

    if (model == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no stream caps registered for model %u\n",
                  __func__, (unsigned)STATE(rig)->rig_model);
        return -RIG_EINTERNAL;
    }

    memset(session, 0, sizeof(session));

    for (i = 0; model[i].formats != 0 && n < HAMLIB_MAX_STREAM_CAPS; i++)
    {
        struct rig_stream_caps e = model[i];
        int channels;

        if (e.type == RIG_STREAM_TYPE_AUDIO_RX)
        {
            channels = rx_codec_slots;
            e.formats = icom_network_session_formats(scfg->rx_codec);
        }
        else if (e.type == RIG_STREAM_TYPE_AUDIO_TX)
        {
            channels = tx_codec_slots;
            e.formats = icom_network_session_formats(scfg->tx_codec);
        }
        else
        {
            /* I/Q is the RX audio path carrying {I,Q} pairs as IQ_CS16, so it
             * needs the two slots of a stereo codec AND those slots must be
             * signed 16-bit samples. Only LPCM16S is: the other two-slot
             * codecs carry companded or unsigned bytes, and advertising I/Q
             * over one of those hands an application something it will read as
             * complex samples and hear as noise. A mono session has nowhere to
             * put Q at all. Those two slots are the components of ONE complex
             * channel, not two channels. */
            if (scfg->rx_codec != ICOM_NETWORK_CODEC_LPCM16S)
            {
                continue;
            }

            channels = 1;
        }

        memset(e.sample_rates, 0, sizeof(e.sample_rates));
        e.sample_rates[0] = (int)scfg->sample_rate;
        memset(e.channels, 0, sizeof(e.channels));
        e.channels[0] = channels;
        session[n++] = e;
    }

    return stream_set_session_caps(rig, session, n);
}

/* Initialize the icom private data, then set the network audio defaults that
 * must be non-zero (TX path enabled; the open clears it for RX-only models). */
static int icom_network_init(RIG *rig)
{
    int ret = icom_init(rig);

    if (ret == RIG_OK)
    {
        struct icom_priv_data *priv = (struct icom_priv_data *)STATE(rig)->priv;
        priv->net_tx_enable = 1;
        priv->net_tx_frame_ms = ICOM_NETWORK_TX_FRAME_MS;
        priv->net_radio_index = -1;
        priv->net_liveness_timeout = ICOM_NETWORK_DEFAULT_LIVENESS_MS;
    }

    return ret;
}

void icom_network_build_caps(struct rig_caps *out,
                             struct rig_stream_caps *stream_caps,
                             const struct icom_network_model *m)
{
    *out = *m->base_caps;
    out->rig_model = m->rig_model;
    out->macro_name = m->macro_name;
    out->model_name = m->model_name;
    out->status = m->status;
    out->version = m->version;
    out->port_type = RIG_PORT_CUSTOM;
    out->rig_init = icom_network_init;
    out->rig_open = icom_network_open;
    out->rig_close = icom_network_close;
    out->cfgparams = icom_network_config_params;
    out->set_conf = icom_network_set_conf;
    out->get_conf = icom_network_get_conf;
    out->stream_open = icom_network_stream_open;
    out->stream_close = icom_network_stream_close;

    {
        /* rig_caps.stream_caps is a pointer; fill the caller's per-model buffer
         * sequentially (0-terminated by the memset) and point at it. */
        struct rig_stream_caps rx, tx, iq;
        int n = 0;
        memset(stream_caps, 0,
               HAMLIB_MAX_STREAM_CAPS * sizeof(*stream_caps));
        memset(&rx, 0, sizeof(rx));
        rx.type = RIG_STREAM_TYPE_AUDIO_RX;
        /* Most wire codecs this radio offers -- LPCM16, uLaw, ADPCM -- are
         * decoded to a 16-bit pivot, so the codec chosen through net_rx_codec
         * decides what crosses the network but not what this backend produces.
         * The two 8-bit LPCM codecs are the exception: their wire samples are
         * unsigned bytes, which is PCM_U8 exactly, so a session that has
         * negotiated one can hand an application the network bytes with
         * nothing done to them. Both are therefore reachable on this model;
         * which one a given connection can serve is pinned once it has
         * negotiated. Anything else an application asks for is the frontend's
         * conversion to make. */
        rx.formats = RIG_STREAM_FORMAT_PCM_S16 | RIG_STREAM_FORMAT_PCM_U8;
        {
            /* ICOM_NETWORK_SUPPORTED_RATES is in the radio's own preference
             * order, highest first, and net_sample_rate is a combo whose value
             * is an index into it, so that order is part of the public
             * configuration and cannot be changed here. A caps rate list is an
             * ascending set, so read the same single source of truth back to
             * front. dump_caps checks the result, so a future edit that breaks
             * this assumption surfaces as a warning rather than silently. */
            static const int supported[] = ICOM_NETWORK_SUPPORTED_RATES;
            int count = ICOM_NETWORK_SUPPORTED_RATE_COUNT;
            int r;

            if (count > HAMLIB_MAX_STREAM_RATES)
            {
                count = HAMLIB_MAX_STREAM_RATES;
            }

            for (r = 0; r < count; r++)
            {
                rx.sample_rates[r] = supported[count - 1 - r];
            }
        }
        /* Both counts are reachable: the mono and stereo codecs are the same
         * hardware, chosen by net_rx_codec. Which one this session can
         * actually carry is pinned once it has negotiated. */
        rx.channels[0] = 1;
        rx.channels[1] = 2;
        rx.max_streams = 1;
        stream_caps[n++] = rx;

        if (!m->rx_only)
        {
            tx = rx;
            tx.type = RIG_STREAM_TYPE_AUDIO_TX;
            stream_caps[n++] = tx;
        }

        if (m->iq_capable)
        {
            memset(&iq, 0, sizeof(iq));
            iq.type = RIG_STREAM_TYPE_IQ_RX;
            /* The stereo LPCM16 payload is already {I,Q,...}; CS16 is what
             * comes off the wire and so all this backend natively has. */
            iq.formats = RIG_STREAM_FORMAT_IQ_CS16;
            iq.sample_rates[0] = 48000;
            /* The stereo payload is one complex stream, not two channels. */
            iq.channels[0] = 1;
            iq.max_streams = 1;
            stream_caps[n++] = iq;
        }

        out->stream_caps = stream_caps;
    }

    if (icom_network_model_count < ICOM_NETWORK_MAX_MODELS)
    {
        icom_network_model_caps[icom_network_model_count] = stream_caps;
        icom_network_models[icom_network_model_count++] = m;
    }
}
