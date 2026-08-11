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

/* Streaming simulator for the dummy backend. */
/* Generates tone/silence or loops back TX→RX for testing. */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>

#include <hamlib/rig.h>
#include "hamlib/rig_state.h"
#include "stream.h"
#include "stream_convert.h"
#include "stream_proto.h"
#include "stream_time.h"
#include "dummy.h"
#include "misc.h"

/* Generator: push a host-clock anchor every N frames (~250 ms at 10 ms
 * frames); inject the synthetic test gap after this many frames. */
#define DUMMY_ANCHOR_EVERY_FRAMES 25
#define DUMMY_SYNTH_GAP_AT_FRAME  5

/* TX scheduler: a timed burst further past due than this counts as late. */
#define DUMMY_TX_LATE_TOLERANCE_MS 50

/* Default tone parameters, overridable via conf tokens */
#define DUMMY_DEFAULT_TONE_FREQ  1000.0f
#define DUMMY_DEFAULT_TONE_AMP   0.5f
#define DUMMY_DEFAULT_IQ_OFFSET  1000.0f

/* Frame size: 480 samples = 10ms at 48kHz */
#define DUMMY_FRAME_SAMPLES 480


/* ------------------------------------------------------------------ */
/* Tone generators                                                     */
/* ------------------------------------------------------------------ */

/* Generate mono sine wave samples into F32LE buffer.
 * phase_acc tracks position across calls for continuous waveform. */
static void generate_sine_f32(float *dst, size_t sample_count,
                              int sample_rate, float frequency,
                              float amplitude, uint32_t *phase_acc)
{
    for (size_t i = 0; i < sample_count; i++)
    {
        float phase = 2.0f * (float)M_PI * (*phase_acc) * frequency
                      / (float)sample_rate;
        dst[i] = amplitude * sinf(phase);
        (*phase_acc)++;
    }
}

/* Generate complex exponential (I/Q spinning phasor) into a CF32LE buffer.
 * For channels > 1 the output is channel-interleaved per sample instant:
 * I0 Q0 I1 Q1 ... I(N-1) Q(N-1), then the next instant. Each channel spins
 * at frequency × (channel + 1) so the interleave order is verifiable. */
static void generate_cexp_cf32(float *dst, size_t sample_count, int channels,
                               int sample_rate, float frequency,
                               float amplitude, uint32_t *phase_acc)
{
    for (size_t i = 0; i < sample_count; i++)
    {
        for (int c = 0; c < channels; c++)
        {
            float freq = frequency * (float)(c + 1);
            float phase = 2.0f * (float)M_PI * (*phase_acc) * freq
                          / (float)sample_rate;
            size_t base = 2 * (i * (size_t)channels + (size_t)c);
            dst[base]     = amplitude * cosf(phase);  /* I */
            dst[base + 1] = amplitude * sinf(phase);  /* Q */
        }

        (*phase_acc)++;
    }
}


/* ------------------------------------------------------------------ */
/* Generator thread                                                    */
/* ------------------------------------------------------------------ */

/* Push a host-clock capture-time anchor. sample_index is in the BACKEND
 * (native) sample domain — the producer's own count of samples generated
 * plus gaps — because rig_stream_push_time_anchor() rescales it into the
 * consumer domain when rate conversion is active. (Do not use
 * rig_stream_get_samples_written() here: it reports the consumer-domain
 * position and would be rescaled a second time.) */
static void push_host_anchor(struct rig_stream *stream, uint64_t sample_index,
                             uint32_t flags)
{
    struct rig_stream_time_anchor anchor;
    memset(&anchor, 0, sizeof(anchor));
    anchor.sample_index = sample_index;
    stream_time_now(&anchor.seconds, &anchor.picoseconds);
    anchor.source = RIG_STREAM_TIME_SRC_HOST;
    anchor.accuracy = RIG_STREAM_TIME_ACC_MS;
    anchor.flags = flags;
    rig_stream_push_time_anchor(stream, &anchor);
}


static void *dummy_stream_generator(void *arg)
{
    struct dummy_stream_state *ds = (struct dummy_stream_state *)arg;
    struct rig_stream *stream = ds->stream;
    /* The backend produces at its NATIVE side (always F32/CF32 for the
     * dummy); any client-requested conversion happens in the frontend
     * behind stream_backend_write(). */
    const struct rig_stream_config *cfg = &stream->backend_config;
    int sample_rate = cfg->sample_rate;
    int channels = cfg->channels;
    int is_iq = stream_type_is_iq(cfg->type);

    /* Frame size in samples per channel */
    size_t frame_samples = DUMMY_FRAME_SAMPLES;

    /* F32LE work buffer — the native production format.
     * I/Q: 2 floats (I+Q) per channel per sample instant. */
    size_t f32_elements = is_iq
                          ? frame_samples * 2 * (size_t)channels
                          : frame_samples * channels;
    size_t f32_bytes = f32_elements * sizeof(float);
    float *f32_buf = malloc(f32_bytes);

    if (!f32_buf)
    {
        return NULL;
    }

    /* Sleep interval: frame_samples / sample_rate seconds */
    struct timespec sleep_ts;
    long sleep_ns = (long)((double)frame_samples / (double)sample_rate * 1e9);
    sleep_ts.tv_sec = sleep_ns / 1000000000L;
    sleep_ts.tv_nsec = sleep_ns % 1000000000L;

    uint64_t frame_no = 0;
    /* Native-domain producer position: samples generated plus gap samples.
     * This is the sample_index base for time anchors. */
    uint64_t native_pos = 0;

    while (ds->running)
    {
        /* When paused, sleep without generating data */
        if (stream->paused)
        {
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        /* Capture-time anchors: at start and on a fixed cadence */
        if (frame_no % DUMMY_ANCHOR_EVERY_FRAMES == 0)
        {
            push_host_anchor(stream, native_pos, 0);
        }

        /* Synthetic radio-side gap (single-shot, for tests) */
        if (ds->synth_gap > 0 && frame_no == DUMMY_SYNTH_GAP_AT_FRAME)
        {
            rig_stream_mark_gap(stream, (uint64_t)ds->synth_gap);
            native_pos += (uint64_t)ds->synth_gap;
            push_host_anchor(stream, native_pos,
                             RIG_STREAM_TIME_FLAG_DISCONTINUITY);
            ds->synth_gap = 0;
        }

        frame_no++;
        native_pos += frame_samples;

        /* Fabricated codec frames: deterministic variable-length records
         * (LCG lengths, counter-filled payload continuing across frames)
         * with a fixed per-frame duration, mirroring a fixed-cadence
         * radio codec (e.g. FlexRadio's 10 ms Opus). Applies to any
         * generator mode — a fake codec has no meaningful tone. */
        if (stream->is_codec)
        {
            ds->lcg = ds->lcg * 1103515245u + 12345u;
            size_t flen = 16 + (size_t)((ds->lcg >> 16) % 241);  /* 16..256 */
            uint8_t *fb = (uint8_t *)f32_buf;

            for (size_t fi = 0; fi < flen; fi++)
            {
                fb[fi] = (uint8_t)(ds->phase_acc++ & 0xFF);
            }

            stream_backend_write_frame(stream, fb, flen,
                                       (uint32_t)frame_samples);
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        /* Counter mode: incrementing bytes in the native frame, for
         * byte-exact transport tests (which open the native format —
         * a conversion stage would rewrite the pattern). */
        if (ds->mode == DUMMY_STREAM_COUNTER)
        {
            uint8_t *p = (uint8_t *)f32_buf;
            size_t i;

            for (i = 0; i < f32_bytes; i++)
            {
                p[i] = (uint8_t)(ds->phase_acc++ & 0xFF);
            }

            stream_backend_write(stream, f32_buf, f32_bytes);
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        /* Generate samples in F32LE */
        if (ds->mode == DUMMY_STREAM_SILENCE)
        {
            memset(f32_buf, 0, f32_elements * sizeof(float));
        }
        else if (is_iq)
        {
            generate_cexp_cf32(f32_buf, frame_samples, channels, sample_rate,
                               ds->iq_offset, ds->tone_amp, &ds->phase_acc);
        }
        else
        {
            /* Audio: generate mono tone */
            generate_sine_f32(f32_buf, frame_samples, sample_rate,
                              ds->tone_freq, ds->tone_amp, &ds->phase_acc);

            /* Stereo: duplicate to right channel at half amplitude */
            if (channels == 2)
            {
                /* Expand mono to interleaved stereo in-place (work backwards) */
                for (int j = (int)frame_samples - 1; j >= 0; j--)
                {
                    f32_buf[2 * j]     = f32_buf[j];       /* L = full */
                    f32_buf[2 * j + 1] = f32_buf[j] * 0.5f; /* R = half */
                }
            }
        }

        /* Hand the native float frame to the frontend; it applies any
         * client-requested conversion on the way into the ring. */
        stream_backend_write(stream, f32_buf, f32_bytes);

        /* Sleep to simulate real-time data rate */
        nanosleep(&sleep_ts, NULL);
    }

    free(f32_buf);
    return NULL;
}


/* ------------------------------------------------------------------ */
/* Loopback thread: reads from paired TX ring buffer, writes to RX.    */
/* ------------------------------------------------------------------ */

static struct rig_stream *find_loopback_peer_locked(struct dummy_priv_data
        *priv,
        rig_stream_type_t rx_type);

/* Re-create ds->loop_conv when the resolved TX peer's native parameters
 * (or the RX side's) change; reuse it otherwise so the resampler state
 * stays seam-free across frames. Returns 0 with a usable pipeline. */
static int ensure_loop_conv(struct dummy_stream_state *ds,
                            rig_stream_format_t fmt, int is_iq,
                            int src_rate, int src_ch,
                            int dst_rate, int dst_ch)
{
    if (ds->loop_conv
            && ds->lc_src_rate == src_rate && ds->lc_src_ch == src_ch
            && ds->lc_dst_rate == dst_rate && ds->lc_dst_ch == dst_ch)
    {
        return 0;
    }

    stream_conv_free(ds->loop_conv);
    ds->loop_conv = NULL;

    if (stream_conv_init(&ds->loop_conv, fmt, src_rate, src_ch,
                         fmt, dst_rate, dst_ch, is_iq,
                         stream_resample_quality(ds->stream
                                                 ? ds->stream->rig
                                                 : NULL)) != 0)
    {
        return -1;
    }

    ds->lc_src_rate = src_rate;
    ds->lc_src_ch = src_ch;
    ds->lc_dst_rate = dst_rate;
    ds->lc_dst_ch = dst_ch;
    return 0;
}

static size_t loop_sink(void *ctx, const void *buf, size_t len)
{
    return stream_backend_write((struct rig_stream *)ctx, buf, len);
}

static void *dummy_stream_loopback_thread(void *arg)
{
    struct dummy_stream_state *ds = (struct dummy_stream_state *)arg;
    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)STATE(ds->rig)->priv;
    struct rig_stream *rx_stream = ds->stream;
    /* Both ends of the loopback run at their NATIVE sides (float formats
     * for the dummy); client-side conversions happen in the frontend. */
    const struct rig_stream_config *rx_cfg = &rx_stream->backend_config;
    int sample_rate = rx_cfg->sample_rate;
    int is_iq = stream_type_is_iq(rx_cfg->type);
    int rx_channels = rx_cfg->channels;

    /* RX native frame size in bytes (channels x per-channel sample size;
     * for I/Q the sample size is one complex I+Q pair). */
    int rx_sample_size = rig_stream_format_sample_size(rx_cfg->format);
    size_t rx_frame_bytes = (size_t)DUMMY_FRAME_SAMPLES * rx_channels
                            * rx_sample_size;

    /* Sleep interval */
    struct timespec sleep_ts;
    long sleep_ns = (long)((double)DUMMY_FRAME_SAMPLES
                           / (double)sample_rate * 1e9);
    sleep_ts.tv_sec = sleep_ns / 1000000000L;
    sleep_ts.tv_nsec = sleep_ns % 1000000000L;

    /* One TX-native frame per iteration, plus room for the RX silence
     * frame when no peer is present. */
    size_t buf_size = DUMMY_FRAME_SAMPLES * 16 * 2 * sizeof(float);

    if (buf_size < rx_frame_bytes)
    {
        buf_size = rx_frame_bytes;
    }

    void *tx_buf = malloc(buf_size);

    if (!tx_buf)
    {
        return NULL;
    }

    while (ds->running)
    {
        /* When paused, sleep without generating data */
        if (rx_stream->paused)
        {
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        /* Resolve the TX peer and read its ring under the state lock so the
         * peer cannot be unregistered and freed while it is dereferenced: a
         * peer's close blocks in unregister_stream_state() until this read
         * releases the lock. Everything afterwards works on copied-out scalars
         * and tx_buf, so it is safe to run unlocked. */
        pthread_mutex_lock(&priv->stream_states_lock);

        struct rig_stream *tx_stream = find_loopback_peer_locked(priv,
                                       rx_stream->type);

        if (!tx_stream)
        {
            pthread_mutex_unlock(&priv->stream_states_lock);

            /* Raw streams idle on silence; a codec stream has no valid
             * "silence" frame, so it just waits for a peer. */
            if (!rx_stream->is_codec)
            {
                memset(tx_buf, 0, rx_frame_bytes);
                stream_backend_write(rx_stream, tx_buf, rx_frame_bytes);
            }

            nanosleep(&sleep_ts, NULL);
            continue;
        }

        /* The TX ring holds the TX stream's native side. */
        const struct rig_stream_config *tx_cfg = &tx_stream->backend_config;
        int tx_channels = tx_cfg->channels;
        int tx_sample_size = rig_stream_format_sample_size(tx_cfg->format);

        /* Codec loopback: records copied verbatim (bytes, duration and
         * start index preserved) — no conversion exists for codec frames,
         * so a format or rate mismatch is refused (frames drained and
         * dropped so the TX side keeps flowing). */
        if (rx_stream->is_codec || tx_stream->is_codec)
        {
            size_t flen = 0;
            uint32_t fdur = 0;
            uint64_t fidx = 0;
            int ok = tx_stream->is_codec
                     && stream_backend_read_frame(tx_stream, tx_buf, buf_size,
                                                  &flen, &fdur, &fidx,
                                                  10) == RIG_OK;

            pthread_mutex_unlock(&priv->stream_states_lock);

            if (ok && rx_stream->is_codec
                    && rx_cfg->format == tx_cfg->format
                    && rx_cfg->sample_rate == tx_cfg->sample_rate)
            {
                stream_backend_write_frame_indexed(rx_stream, tx_buf, flen,
                                                   fdur, fidx);
            }
            else if (!ok)
            {
                nanosleep(&sleep_ts, NULL);
            }

            continue;
        }

        /* TX frame size: scale to TX sample rate (DUMMY_FRAME_SAMPLES is
         * 10ms at 48kHz, so at other rates we adjust proportionally). */
        int tx_rate = tx_cfg->sample_rate;
        size_t tx_frame_samples = (size_t)DUMMY_FRAME_SAMPLES * tx_rate / 48000;

        if (tx_frame_samples == 0)
        {
            tx_frame_samples = 1;
        }

        /* Coherent I/Q channels are interleaved per sample just like audio
         * channels; the per-channel "sample" is one complex I+Q pair. */
        size_t tx_frame_bytes =
            tx_frame_samples * (size_t)tx_channels * tx_sample_size;

        if (tx_frame_bytes > buf_size)
        {
            tx_frame_bytes = buf_size - buf_size % ((size_t)tx_channels
                             * tx_sample_size);
        }

        /* Read from TX ring buffer */
        size_t got = stream_ringbuf_read(&tx_stream->ringbuf,
                                         tx_buf, tx_frame_bytes, 10);

        pthread_mutex_unlock(&priv->stream_states_lock);

        if (got == 0)
        {
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        if (tx_channels == rx_channels && tx_rate == sample_rate)
        {
            /* Native sides match (both float): straight through. */
            stream_backend_write(rx_stream, tx_buf, got);
        }
        else
        {
            /* Adapt channels/rate through the stateful pipeline so
             * resampled frames stay seam-free, then hand the result to
             * the RX stream's normal produce path. */
            if (ensure_loop_conv(ds, rx_cfg->format, is_iq,
                                 tx_rate, tx_channels,
                                 sample_rate, rx_channels) != 0)
            {
                nanosleep(&sleep_ts, NULL);
                continue;
            }

            stream_conv_process(ds->loop_conv, tx_buf, got,
                                loop_sink, rx_stream);
        }
    }

    free(tx_buf);
    return NULL;
}


/* ------------------------------------------------------------------ */
/* Stream state table helpers                                          */
/* ------------------------------------------------------------------ */

/* Find a free slot in the priv stream_states table and store ds.
 * Returns the slot index, or -1 if full. */
static int register_stream_state(struct dummy_priv_data *priv,
                                 rig_stream_type_t type,
                                 struct dummy_stream_state *ds)
{
    pthread_mutex_lock(&priv->stream_states_lock);

    for (int i = 0; i < DUMMY_MAX_STREAMS_PER_TYPE; i++)
    {
        if (priv->stream_states[type][i] == NULL)
        {
            priv->stream_states[type][i] = ds;
            pthread_mutex_unlock(&priv->stream_states_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&priv->stream_states_lock);
    return -1;
}

static void unregister_stream_state(struct dummy_priv_data *priv,
                                    rig_stream_type_t type,
                                    struct dummy_stream_state *ds)
{
    pthread_mutex_lock(&priv->stream_states_lock);

    for (int i = 0; i < DUMMY_MAX_STREAMS_PER_TYPE; i++)
    {
        if (priv->stream_states[type][i] == ds)
        {
            priv->stream_states[type][i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&priv->stream_states_lock);
}

/* Find the TX peer for an RX loopback stream. The caller must hold
 * priv->stream_states_lock so the returned stream cannot be unregistered and
 * freed while it is in use. Audio RX pairs with Audio TX slot 0; I/Q RX pairs
 * with I/Q TX slot 0. */
static struct rig_stream *find_loopback_peer_locked(struct dummy_priv_data
        *priv,
        rig_stream_type_t rx_type)
{
    rig_stream_type_t tx_type;

    if (rx_type == RIG_STREAM_TYPE_AUDIO_RX)
    {
        tx_type = RIG_STREAM_TYPE_AUDIO_TX;
    }
    else if (rx_type == RIG_STREAM_TYPE_IQ_RX)
    {
        tx_type = RIG_STREAM_TYPE_IQ_TX;
    }
    else
    {
        return NULL;
    }

    /* Find first registered TX stream of matching type */
    for (int i = 0; i < DUMMY_MAX_STREAMS_PER_TYPE; i++)
    {
        if (priv->stream_states[tx_type][i] != NULL)
        {
            return priv->stream_states[tx_type][i]->stream;
        }
    }

    return NULL;
}


/* ------------------------------------------------------------------ */
/* TX scheduler thread                                                 */
/* ------------------------------------------------------------------ */

/* Consumes the TX ring buffer at the stream's real-time rate, honoring
 * queued burst targets: a timed SOB gates play-out until its UTC instant
 * (counting tx_late when past due), and with BURST_PTT declared, SOB keys
 * PTT and EOB unkeys it. Simulates timed/gated transmit. */
static void *dummy_stream_tx_scheduler(void *arg)
{
    struct dummy_stream_state *ds = (struct dummy_stream_state *)arg;
    struct rig_stream *stream = ds->stream;
    /* The TX ring holds the stream's native side; consume at that rate. */
    const struct rig_stream_config *cfg = &stream->backend_config;
    int is_iq = stream_type_is_iq(cfg->type);
    int sample_size = rig_stream_format_sample_size(cfg->format);
    int channels = (!is_iq && cfg->channels > 0) ? cfg->channels : 1;

    if (sample_size <= 0)
    {
        /* Codec TX: consume whole codec frames at their declared pace and
         * discard them (a real backend would send them to the radio).
         * Burst targets are not simulated for codec streams. */
        unsigned char *cbuf = malloc((size_t)stream->max_payload);

        if (!cbuf)
        {
            return NULL;
        }

        while (ds->running)
        {
            if (stream->paused)
            {
                struct timespec idle = { 0, 10000000L };  /* 10 ms */
                nanosleep(&idle, NULL);
                continue;
            }

            size_t flen = 0;
            uint32_t fdur = 0;

            if (stream_backend_read_frame(stream, cbuf,
                                          (size_t)stream->max_payload,
                                          &flen, &fdur, NULL, 100) != RIG_OK)
            {
                continue;
            }

            ds->tx_consumed += fdur;

            /* Pace at the frame's decoded duration (fallback 10 ms). */
            long ns = fdur > 0
                      ? (long)((double)fdur / (double)cfg->sample_rate * 1e9)
                      : 10000000L;
            struct timespec pace = { ns / 1000000000L, ns % 1000000000L };
            nanosleep(&pace, NULL);
        }

        free(cbuf);
        return NULL;
    }

    size_t frame_samples = DUMMY_FRAME_SAMPLES;
    size_t frame_bytes = frame_samples * (size_t)sample_size * channels;
    unsigned char *buf = malloc(frame_bytes);

    if (!buf)
    {
        return NULL;
    }

    struct timespec sleep_ts;

    long sleep_ns = (long)((double)frame_samples
                           / (double)cfg->sample_rate * 1e9);
    sleep_ts.tv_sec = sleep_ns / 1000000000L;
    sleep_ts.tv_nsec = sleep_ns % 1000000000L;

    int had_data = 0;   /* for underrun-episode edge detection */

    while (ds->running)
    {
        if (stream->paused)
        {
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        /* Process burst targets reachable within the next frame */
        struct rig_stream_tx_target tgt;

        while (rig_stream_pop_tx_target(stream,
                                        ds->tx_consumed + frame_samples,
                                        &tgt))
        {
            if (tgt.flags & RIG_STREAM_TIME_FLAG_TX_TIMED)
            {
                int64_t now_s;
                uint64_t now_ps;
                stream_time_now(&now_s, &now_ps);

                int64_t wait_ms = stream_time_diff_ms(tgt.seconds,
                                                      tgt.picoseconds,
                                                      now_s, now_ps);

                if (wait_ms < -DUMMY_TX_LATE_TOLERANCE_MS)
                {
                    struct rig_stream_write_status ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.event = RIG_STREAM_WRITE_EVENT_LATE;
                    ev.sample_index = ds->tx_consumed;
                    ev.lateness = (int64_t)(-wait_ms)
                                  * (int64_t)cfg->sample_rate / 1000;
                    ev.time_valid = 1;
                    ev.seconds = tgt.seconds;
                    ev.picoseconds = tgt.picoseconds;
                    ev.time_source = RIG_STREAM_TIME_SRC_HOST;
                    ev.time_accuracy = RIG_STREAM_TIME_ACC_MS;
                    stream_record_write_status(stream, &ev, 0);
                }

                /* Timed gate: hold play-out until the target instant */
                while (wait_ms > 0 && ds->running)
                {
                    struct timespec gate = { 0, 10000000L };  /* 10 ms */
                    nanosleep(&gate, NULL);
                    stream_time_now(&now_s, &now_ps);
                    wait_ms = stream_time_diff_ms(tgt.seconds,
                                                  tgt.picoseconds,
                                                  now_s, now_ps);
                }
            }

            if (ds->burst_ptt && (tgt.flags & RIG_STREAM_TIME_FLAG_SOB))
            {
                rig_set_ptt(ds->rig, RIG_VFO_CURR, RIG_PTT_ON);
            }

            if (ds->burst_ptt && (tgt.flags & RIG_STREAM_TIME_FLAG_EOB))
            {
                rig_set_ptt(ds->rig, RIG_VFO_CURR, RIG_PTT_OFF);
            }
        }

        size_t got = stream_ringbuf_read(&stream->ringbuf, buf, frame_bytes, 100);

        if (got > 0)
        {
            had_data = 1;
            ds->tx_consumed += got / ((size_t)sample_size * channels);
            nanosleep(&sleep_ts, NULL);   /* real-time pacing */
        }
        else if (had_data)
        {
            /* Ring starved mid-transmission: one underrun event per episode. */
            had_data = 0;
            struct rig_stream_write_status ev;
            memset(&ev, 0, sizeof(ev));
            ev.event = RIG_STREAM_WRITE_EVENT_UNDERRUN;
            ev.sample_index = ds->tx_consumed;
            stream_record_write_status(stream, &ev, 0);
        }
    }

    free(buf);
    return NULL;
}


/* ------------------------------------------------------------------ */
/* Backend hooks                                                       */
/* ------------------------------------------------------------------ */

int dummy_stream_open(RIG *rig, struct rig_stream *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered: type=%s format=%s\n",
              __func__,
              stream_type_name(stream->type),
              stream_format_name(stream->config.format));

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)STATE(rig)->priv;

    struct dummy_stream_state *ds = calloc(1, sizeof(*ds));

    if (!ds)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: calloc failed\n", __func__);
        return -RIG_ENOMEM;
    }

    ds->stream = stream;
    ds->rig = rig;
    ds->mode = priv->stream_mode;
    ds->tone_freq = priv->stream_tone_freq;
    ds->tone_amp = priv->stream_tone_amp;
    ds->iq_offset = priv->stream_iq_offset;
    ds->synth_gap = priv->stream_synth_gap;
    ds->burst_ptt = (stream->caps_flags & RIG_STREAM_CAP_BURST_PTT) != 0;
    ds->phase_acc = 0;

    /* Register in the priv state table */
    if (register_stream_state(priv, stream->type, ds) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no free stream slots for type=%s\n",
                  __func__, stream_type_name(stream->type));
        free(ds);
        return -RIG_EINVAL;  /* No free slots */
    }

    /* RX streams get a generator or loopback thread; TX streams get the
     * scheduler that consumes the ring buffer and honors burst targets
     * (except in loopback mode, where the paired RX thread consumes). */
    void *(*thread_fn)(void *) = NULL;

    if (stream_type_is_rx(stream->type))
    {
        if (ds->mode == DUMMY_STREAM_LOOPBACK)
        {
            /* The loopback thread resolves its TX peer under the state lock
             * each iteration, so no peer is cached here. */
            thread_fn = dummy_stream_loopback_thread;
        }
        else
        {
            thread_fn = dummy_stream_generator;
        }
    }
    else if (ds->mode != DUMMY_STREAM_LOOPBACK)
    {
        thread_fn = dummy_stream_tx_scheduler;
    }

    if (thread_fn)
    {
        ds->running = 1;
        int err = pthread_create(&ds->thread, NULL, thread_fn, ds);

        if (err != 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: pthread_create failed\n", __func__);
            unregister_stream_state(priv, stream->type, ds);
            free(ds);
            return -RIG_EIO;
        }

        ds->thread_started = 1;
    }

    /* Store ds pointer for retrieval in close */
    stream->backend_priv = ds;

    return RIG_OK;
}

int dummy_stream_close(RIG *rig, struct rig_stream *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s() entered: type=%s\n",
              __func__, stream_type_name(stream->type));

    struct dummy_priv_data *priv =
        (struct dummy_priv_data *)STATE(rig)->priv;

    struct dummy_stream_state *ds =
        (struct dummy_stream_state *)stream->backend_priv;

    if (!ds)
    {
        return RIG_OK;
    }

    /* Stop generator/loopback/TX-scheduler thread */
    ds->running = 0;

    if (ds->thread_started)
    {
        pthread_join(ds->thread, NULL);
    }

    /* The loopback pipeline is owned by the (now joined) thread. */
    stream_conv_free(ds->loop_conv);

    unregister_stream_state(priv, stream->type, ds);
    stream->backend_priv = NULL;
    free(ds);

    return RIG_OK;
}
