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

/* Hamlib streaming subsystem — stream lifecycle, read/write, and metadata. */
/* Implements the rig_stream_* frontend API declared in rig.h (the ring buffer */
/* itself lives in stream_ringbuf). */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "stream.h"
#include "stream_ringbuf.h"
#include "hamlib/rig_state.h"
#include "stream_convert.h"
#include "stream_proto.h"
#include "stream_time.h"
#include "cache.h"
#include "misc.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>


/* ------------------------------------------------------------------ */
/* Stream state lifecycle                                              */
/* ------------------------------------------------------------------ */

int rig_stream_state_init(struct rig_stream_state **state)
{
    struct rig_stream_state *ss = calloc(1, sizeof(*ss));

    if (!ss)
    {
        return -1;
    }

    pthread_mutex_init(&ss->stream_mutex, NULL);
    pthread_cond_init(&ss->stream_idle, NULL);
    *state = ss;
    return 0;
}


/* Quiescent teardown of a single stream that the caller has already removed
 * from the registry. Marks it closing, wakes and waits out any blocked
 * rig_stream_read()/rig_stream_wait_write_status() caller, runs the backend
 * close hook (which joins the backend I/O thread), then destroys the ring and
 * condvars and frees the stream. The backend hook runs with no lock held. */
static void stream_teardown(struct rig_stream_state *ss,
                            struct rig_stream *stream)
{
    if (!stream)
    {
        return;
    }

    pthread_mutex_lock(&stream->ringbuf.lock);
    stream->active = 0;
    stream->ringbuf.closing = 1;
    stream->write_event_closing = 1;
    pthread_cond_broadcast(&stream->ringbuf.data_available);
    pthread_cond_broadcast(&stream->write_event_available);

    while (stream->blocked_waiters > 0)
    {
        pthread_cond_wait(&stream->quiesced, &stream->ringbuf.lock);
    }

    pthread_mutex_unlock(&stream->ringbuf.lock);

    /* Wait out any in-flight public API call still holding a reference. The
     * stream is already removed from the registry, so stream_guard_enter() can
     * no longer take a new reference; once in_use drains, no thread can touch
     * the ring or the struct and teardown is safe. */
    pthread_mutex_lock(&ss->stream_mutex);

    while (stream->in_use > 0)
    {
        pthread_cond_wait(&ss->stream_idle, &ss->stream_mutex);
    }

    pthread_mutex_unlock(&ss->stream_mutex);

    if (stream->rig && stream->rig->caps->stream_close)
    {
        stream->rig->caps->stream_close(stream->rig, stream);
    }

    stream_ringbuf_destroy(&stream->ringbuf);
    stream_write_event_destroy(stream);
    free(stream);
}


void rig_stream_state_cleanup(struct rig_stream_state *state)
{
    if (!state)
    {
        return;
    }

    /* Close any open streams */
    for (int t = 0; t < RIG_STREAM_TYPE_COUNT; t++)
    {
        for (int i = 0; i < HAMLIB_MAX_STREAMS; i++)
        {
            struct rig_stream *s = state->streams[t][i];

            state->streams[t][i] = NULL;

            if (s)
            {
                stream_teardown(state, s);
            }
        }
    }

    pthread_cond_destroy(&state->stream_idle);
    pthread_mutex_destroy(&state->stream_mutex);
    free(state);
}


/* ------------------------------------------------------------------ */
/* Helper: default buffer size for a stream type                       */
/* ------------------------------------------------------------------ */

static size_t default_buffer_size(rig_stream_type_t type)
{
    switch (type)
    {
    case RIG_STREAM_TYPE_AUDIO_RX:
    case RIG_STREAM_TYPE_AUDIO_TX:
        return RIG_STREAM_AUDIO_BUF_DEFAULT;

    case RIG_STREAM_TYPE_IQ_RX:
    case RIG_STREAM_TYPE_IQ_TX:
        return RIG_STREAM_IQ_BUF_DEFAULT;

    default:
        return RIG_STREAM_AUDIO_BUF_DEFAULT;
    }
}


/* Bytes per frame (all channels) for buffer_duration_ms sizing. Compressed
 * formats have no fixed sample size, so they fall back to a generous 4-byte
 * estimate to keep the duration-based ring comfortably sized. */
static int stream_format_bytes_per_frame(rig_stream_format_t fmt, int channels)
{
    if (channels <= 0)
    {
        return 0;
    }

    int sample_size = rig_stream_format_sample_size(fmt);

    return sample_size > 0 ? sample_size * channels : 4 * channels;
}


static size_t buffer_size_from_config(const struct rig_stream_config *config)
{
    size_t buf_size = config->buffer_bytes;
    unsigned int duration_ms = config->buffer_duration_ms;

    /* With neither buffer field set, audio streams default to a duration at
     * the configured rate and format. Capacity is headroom against consumer
     * scheduling jitter, not added latency: the steady-state fill level is
     * set by the producer/consumer pacing. I/Q keeps its byte default. */
    if (buf_size == 0 && duration_ms == 0
            && (config->type == RIG_STREAM_TYPE_AUDIO_RX
                || config->type == RIG_STREAM_TYPE_AUDIO_TX))
    {
        duration_ms = RIG_STREAM_AUDIO_BUF_DEFAULT_MS;
    }

    if (buf_size == 0 && duration_ms > 0)
    {
        int bpf = stream_format_bytes_per_frame(config->format,
                                                config->channels);
        uint64_t bytes = (uint64_t)config->sample_rate * (uint64_t)bpf
                         * (uint64_t)duration_ms / 1000ULL;

        if (bytes == 0)
        {
            bytes = 4096;
        }

        if (bytes > (uint64_t)SIZE_MAX)
        {
            bytes = SIZE_MAX;
        }

        buf_size = (size_t)bytes;
    }

    if (buf_size == 0)
    {
        buf_size = default_buffer_size(config->type);
    }

    return buf_size;
}


/* ------------------------------------------------------------------ */
/* Helper: get stream_state from RIG, with NULL check                  */
/* ------------------------------------------------------------------ */

static struct rig_stream_state *get_stream_state(RIG *rig)
{
    if (!rig)
    {
        return NULL;
    }

    return (struct rig_stream_state *)STATE(rig)->stream_state;
}


/* Reference a stream for the duration of a public API call so a concurrent
 * rig_stream_close() cannot free it mid-call. Returns RIG_OK with in_use
 * incremented, or an error if the stream has been closed. Pair with
 * stream_guard_leave() on every exit path.
 *
 * rig_stream_close() clears active and drains in_use under stream_mutex, so a
 * call that gets the lock first is counted (close waits for it), and one that
 * arrives after close has cleared active is rejected. Passing a handle whose
 * close has already returned is use-after-free, as with any handle-based API. */
static int stream_guard_enter(RIG *rig, rig_stream_t *stream)
{
    struct rig_stream_state *ss = get_stream_state(rig);

    /* No per-rig streaming registry (e.g. a white-box test operating on a
     * stack stream, or a rig not opened for streaming): nothing can close and
     * free this stream underneath us, so the call proceeds unguarded.
     * stream_guard_leave() mirrors this via the same NULL check. */
    if (!ss)
    {
        return RIG_OK;
    }

    pthread_mutex_lock(&ss->stream_mutex);

    if (!stream->active)
    {
        pthread_mutex_unlock(&ss->stream_mutex);
        return -RIG_EINVAL;
    }

    stream->in_use++;
    pthread_mutex_unlock(&ss->stream_mutex);
    return RIG_OK;
}


/* Release the reference taken by stream_guard_enter(); wakes a close that is
 * draining in_use. */
static void stream_guard_leave(RIG *rig, rig_stream_t *stream)
{
    struct rig_stream_state *ss = get_stream_state(rig);

    if (!ss)
    {
        return;
    }

    pthread_mutex_lock(&ss->stream_mutex);

    if (--stream->in_use == 0)
    {
        pthread_cond_broadcast(&ss->stream_idle);
    }

    pthread_mutex_unlock(&ss->stream_mutex);
}


/* ------------------------------------------------------------------ */
/* rig_stream_caps_count / rig_stream_caps_at                          */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_caps_count(RIG *rig)
{
    if (!rig || !rig->caps || !rig->caps->stream_caps)
    {
        return 0;
    }

    const struct rig_stream_caps *src = rig->caps->stream_caps;
    int count = 0;

    for (int i = 0; i < HAMLIB_MAX_STREAM_CAPS; i++)
    {
        if (src[i].formats == 0)
        {
            break;  /* 0-terminated */
        }

        count++;
    }

    return count;
}

const struct rig_stream_caps *HAMLIB_API rig_stream_caps_at(RIG *rig,
        int index)
{
    if (!rig || !rig->caps || !rig->caps->stream_caps
            || index < 0 || index >= HAMLIB_MAX_STREAM_CAPS)
    {
        return NULL;
    }

    const struct rig_stream_caps *src = rig->caps->stream_caps;

    if (src[index].formats == 0)
    {
        return NULL;  /* at or past the 0-terminator */
    }

    return &src[index];
}


/* ------------------------------------------------------------------ */
/* rig_stream_config_alloc / rig_stream_config_free                    */
/* ------------------------------------------------------------------ */

struct rig_stream_config *HAMLIB_API rig_stream_config_alloc(void)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    struct rig_stream_config *config =
        calloc(1, sizeof(struct rig_stream_config));

    if (config)
    {
        config->struct_size = sizeof(struct rig_stream_config);
    }

    return config;
}

void HAMLIB_API rig_stream_config_free(struct rig_stream_config *config)
{
    free(config);
}


/* ------------------------------------------------------------------ */
/* rig_stream_open                                                     */
/* ------------------------------------------------------------------ */

/* Return the backend's stream caps entry for a stream type, or NULL if the
 * type is not offered. */
static const struct rig_stream_caps *find_stream_caps(RIG *rig,
        rig_stream_type_t type)
{
    const struct rig_stream_caps *caps_arr = rig->caps->stream_caps;

    for (int i = 0; caps_arr && i < HAMLIB_MAX_STREAM_CAPS; i++)
    {
        if (caps_arr[i].formats == 0)
        {
            break;
        }

        if (caps_arr[i].type == type)
        {
            return &caps_arr[i];
        }
    }

    return NULL;
}


/* Check a requested config's format, sample rate and channel count against a
 * backend caps entry. Returns RIG_OK or -RIG_EINVAL. */
static int validate_config_against_caps(const struct rig_stream_config *config,
                                        const struct rig_stream_caps *caps)
{
    /* A stream needs a concrete rate and channel count; zero or negative
     * values would otherwise reach downstream divisions (rate-derived timing,
     * per-channel sizing) as a divide-by-zero. */
    if (config->sample_rate <= 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: sample rate must be positive (got %d)\n",
                  __func__, config->sample_rate);
        return -RIG_EINVAL;
    }

    if (config->channels <= 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: channel count must be positive (got %d)\n",
                  __func__, config->channels);
        return -RIG_EINVAL;
    }

    /* The requested format must be exactly one supported format bit. A
     * multi-bit value would otherwise pass the overlap test and then resolve
     * to sample_size 0, silently degrading the stream to compressed handling. */
    if (config->format == 0
            || (config->format & (config->format - 1)) != 0
            || !(config->format & caps->formats))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: format 0x%x not a single supported format "
                  "(caps formats=0x%x)\n",
                  __func__, config->format, caps->formats);
        return -RIG_EINVAL;
    }

    /* Verify sample rate is in the supported list */
    if (config->sample_rate > 0)
    {
        int rate_ok = 0;

        for (int i = 0; i < HAMLIB_MAX_STREAM_RATES; i++)
        {
            if (caps->sample_rates[i] == 0)
            {
                break;
            }

            if (caps->sample_rates[i] == config->sample_rate)
            {
                rate_ok = 1;
                break;
            }
        }

        if (!rate_ok)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: sample rate %u not in supported list\n",
                      __func__, config->sample_rate);
            return -RIG_EINVAL;
        }
    }

    /* Verify the channel count is within the supported range */
    if (caps->channels_min > 0
            && (config->channels < caps->channels_min
                || config->channels > caps->channels_max))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: channels %d outside supported range %d-%d\n",
                  __func__, config->channels,
                  caps->channels_min, caps->channels_max);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/* Resolve the staleness-watchdog thresholds for a new stream: per-stream config
 * wins, then the rig-level conf default, then the built-in default. coarse must
 * not exceed invalidate — clamped with a warning. */
static void resolve_stale_thresholds(RIG *rig,
                                     const struct rig_stream_config *config,
                                     struct rig_stream *s)
{
    struct rig_state *rs = STATE(rig);

    s->stale_coarse_ms = config->time_stale_coarse_ms
                         ? config->time_stale_coarse_ms
                         : (rs->stream_time_stale_coarse_ms
                            ? rs->stream_time_stale_coarse_ms
                            : RIG_STREAM_STALE_COARSE_DEFAULT_MS);
    s->stale_invalidate_ms = config->time_stale_invalidate_ms
                             ? config->time_stale_invalidate_ms
                             : (rs->stream_time_stale_invalidate_ms
                                ? rs->stream_time_stale_invalidate_ms
                                : RIG_STREAM_STALE_INVALIDATE_DEFAULT_MS);

    if (s->stale_coarse_ms > s->stale_invalidate_ms)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: time_stale_coarse (%u ms) > invalidate (%u ms); "
                  "clamping coarse\n", __func__,
                  s->stale_coarse_ms, s->stale_invalidate_ms);
        s->stale_coarse_ms = s->stale_invalidate_ms;
    }
}


int HAMLIB_API rig_stream_open(RIG *rig,
                               const struct rig_stream_config *config,
                               rig_stream_t **stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !config || !stream)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: null rig, config, or stream pointer\n",
                  __func__);
        return -RIG_EINVAL;
    }

    if (config->struct_size == 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: config not obtained from rig_stream_config_alloc()\n",
                  __func__);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: type=%d format=0x%x rate=%u\n",
              __func__, config->type, config->format, config->sample_rate);

    if (config->type >= RIG_STREAM_TYPE_COUNT)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid stream type %d\n",
                  __func__, config->type);
        return -RIG_EINVAL;
    }

    if (!rig->caps->stream_open)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: backend does not implement stream_open\n",
                  __func__);
        return -RIG_ENIMPL;
    }

    /* Validate format against capabilities */
    const struct rig_stream_caps *found_caps = find_stream_caps(rig,
        config->type);

    if (!found_caps)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no caps found for stream type %d\n",
                  __func__, config->type);
        return -RIG_EINVAL;
    }

    int cfg_ret = validate_config_against_caps(config, found_caps);

    if (cfg_ret != RIG_OK)
    {
        return cfg_ret;
    }

    struct rig_stream_state *ss = get_stream_state(rig);

    if (!ss)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no stream state on rig\n", __func__);
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&ss->stream_mutex);

    /* Enforce max concurrent streams of this type (per backend caps). */
    {
        int active = 0;
        int limit = found_caps->max_streams;

        if (limit <= 0)
        {
            limit = 1;
        }

        for (int i = 0; i < HAMLIB_MAX_STREAMS; i++)
        {
            if (ss->streams[config->type][i] != NULL)
            {
                active++;
            }
        }

        if (active >= limit)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: max_streams=%d reached for type %d\n",
                      __func__, limit, config->type);
            pthread_mutex_unlock(&ss->stream_mutex);
            return -RIG_EINVAL;
        }
    }

    /* Find a free slot */
    int slot = -1;

    for (int i = 0; i < HAMLIB_MAX_STREAMS; i++)
    {
        if (ss->streams[config->type][i] == NULL)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no free stream slots for type %d\n",
                  __func__, config->type);
        pthread_mutex_unlock(&ss->stream_mutex);
        return -RIG_EINVAL;
    }

    /* Allocate and initialize stream handle */
    struct rig_stream *s = calloc(1, sizeof(*s));

    if (!s)
    {
        pthread_mutex_unlock(&ss->stream_mutex);
        return -RIG_ENOMEM;
    }

    s->type = config->type;
    /* Sized copy over a zeroed target: an older app's config may be smaller
     * than our struct (its unset trailing fields default to 0), and a newer
     * app's may be larger (we ignore fields we don't know). */
    {
        size_t copy = config->struct_size < sizeof(s->config)
                      ? config->struct_size : sizeof(s->config);
        memset(&s->config, 0, sizeof(s->config));
        memcpy(&s->config, config, copy);
    }
    s->id = ss->stream_next_id[config->type]++;

    /* Bytes per frame for producer-index accounting; 0 for compressed
     * formats (index/time features unavailable there). */
    {
        int sample_size = rig_stream_format_sample_size(config->format);
        int channels = config->channels > 0 ? config->channels : 1;
        s->frame_bytes = sample_size > 0 ? sample_size * channels : 0;
    }

    /* Effective sender payload budget from the configured (clamped) MTU. */
    s->max_payload = stream_max_payload_from_mtu(config->mtu, s->frame_bytes);

    s->caps_flags = found_caps->caps_flags;
    s->tx_horizon_ms = found_caps->tx_schedule_horizon_ms;

    resolve_stale_thresholds(rig, config, s);

    /* Initialize ring buffer */
    size_t buf_size = buffer_size_from_config(config);

    if (stream_ringbuf_init(&s->ringbuf, buf_size) != 0)
    {
        free(s);
        pthread_mutex_unlock(&ss->stream_mutex);
        return -RIG_ENOMEM;
    }

    /* Write-status and close-rendezvous condvars share ringbuf.lock. */
    stream_write_event_init(s);

    s->rig = rig;
    s->active = 1;

    /* Reserve the slot and release the registry lock before the backend hook:
     * stream_open may block (network handshake) or open a companion stream, so
     * it must not run holding the rig-wide stream_mutex. The reserved slot is
     * unreachable to readers until *stream is published on success. */
    ss->streams[config->type][slot] = s;
    pthread_mutex_unlock(&ss->stream_mutex);

    int ret = rig->caps->stream_open(rig, s);

    if (ret != RIG_OK)
    {
        pthread_mutex_lock(&ss->stream_mutex);
        ss->streams[config->type][slot] = NULL;
        pthread_mutex_unlock(&ss->stream_mutex);

        s->active = 0;
        stream_ringbuf_destroy(&s->ringbuf);
        stream_write_event_destroy(s);
        free(s);
        return ret;
    }

    *stream = s;
    return RIG_OK;
}


/* ------------------------------------------------------------------ */
/* rig_stream_close                                                    */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_close(RIG *rig, rig_stream_t *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream)
    {
        return -RIG_EINVAL;
    }

    struct rig_stream_state *ss = get_stream_state(rig);

    if (!ss)
    {
        return -RIG_EINVAL;
    }

    /* Remove from the active table under the registry lock, then tear down
     * with no lock held: the backend close hook may block joining its I/O
     * thread, and a blocked reader/waiter must be able to re-acquire the ring
     * lock to observe the closing flag and exit. */
    pthread_mutex_lock(&ss->stream_mutex);

    int found = 0;

    for (int i = 0; i < HAMLIB_MAX_STREAMS; i++)
    {
        if (ss->streams[stream->type][i] == stream)
        {
            ss->streams[stream->type][i] = NULL;
            /* Clear active under the same lock stream_guard_enter() checks it
             * under, so a call starting concurrently is cleanly rejected rather
             * than referencing a stream that is about to be freed. */
            stream->active = 0;
            found = 1;
            break;
        }
    }

    pthread_mutex_unlock(&ss->stream_mutex);

    if (!found)
    {
        return -RIG_EINVAL;
    }

    stream_teardown(ss, stream);
    return RIG_OK;
}


/* ------------------------------------------------------------------ */
/* rig_stream_read                                                     */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_read(RIG *rig,
                               rig_stream_t *stream,
                               void *buffer,
                               size_t buffer_size,
                               size_t *bytes_read,
                               int timeout_ms,
                               struct rig_stream_read_info *info)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (!rig || !stream || !buffer || !bytes_read)
    {
        return -RIG_EINVAL;
    }

    if (info)
    {
        memset(info, 0, sizeof(*info));
    }

    /* Hold a reference for the whole call so a concurrent rig_stream_close()
     * waits for this read to finish (past the accounting that re-locks the
     * ring) before it destroys the ring and frees the stream. The guard also
     * confirms the stream is still live and registered. */
    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    struct rig_stream_ringbuf *rb = &stream->ringbuf;

    uint64_t first_index;
    int ret;

    /* Paused: don't deliver data, leave ring buffer intact */
    if (stream->paused)
    {
        *bytes_read = 0;
        ret = -RIG_ETIMEOUT;
        goto out;
    }

    /* Use backend hook if available */
    if (rig->caps->stream_read)
    {
        ret = rig->caps->stream_read(rig, stream, buffer, buffer_size,
                                     bytes_read, timeout_ms, info);

        /* Muted: consume as usual but hand back silence. */
        if (ret == RIG_OK && stream->muted && *bytes_read > 0)
        {
            memset(buffer, 0, *bytes_read);
        }

        goto out;
    }

    pthread_mutex_lock(&rb->lock);

    if (rb->closing)
    {
        pthread_mutex_unlock(&rb->lock);
        *bytes_read = 0;
        ret = -RIG_ENAVAIL;
        goto out;
    }

    stream->blocked_waiters++;

    if (stream_ringbuf_wait_data_locked(rb, timeout_ms) < 0)
    {
        int closing = rb->closing;

        if (--stream->blocked_waiters == 0 && closing)
        {
            pthread_cond_signal(&stream->quiesced);
        }

        pthread_mutex_unlock(&rb->lock);
        *bytes_read = 0;
        ret = closing ? -RIG_ENAVAIL : -RIG_ETIMEOUT;
        goto out;
    }

    first_index = stream_first_readable_index_locked(stream);
    *bytes_read = stream_ringbuf_consume_locked(rb, buffer, buffer_size);

    /* Account for the consume in the same critical section as the consume
     * itself, so a concurrent stream_skip_samples() cannot land between them
     * and mis-attribute the gap (gap vs overrun). */
    {
        uint64_t frames = stream->frame_bytes > 0
                          ? *bytes_read / (uint64_t)stream->frame_bytes : 0;
        stream_consume_account_locked(stream, first_index, frames, info);
    }

    pthread_mutex_unlock(&rb->lock);

    /* Muted: consume data from ring buffer but return silence */
    if (stream->muted)
    {
        memset(buffer, 0, *bytes_read);
    }

    if (info)
    {
        stream_fill_read_time(stream, info);
    }

    pthread_mutex_lock(&rb->lock);

    if (--stream->blocked_waiters == 0 && rb->closing)
    {
        pthread_cond_signal(&stream->quiesced);
    }

    pthread_mutex_unlock(&rb->lock);

    ret = RIG_OK;

out:
    stream_guard_leave(rig, stream);
    return ret;
}


/* ------------------------------------------------------------------ */
/* rig_stream_write                                                    */
/* ------------------------------------------------------------------ */

/* Bind a burst target (scheduled time and/or SOB/EOB) to the producer index of
 * the next sample to be written. Returns RIG_OK, or an error if a timed target
 * is requested on a backend that lacks timed TX or lands beyond the horizon. */
static int stream_bind_tx_target(rig_stream_t *stream,
                                 const struct rig_stream_write_info *info)
{
    if (info->time_valid)
    {
        if (!(stream->caps_flags & (RIG_STREAM_CAP_TIMED_TX_COARSE
                                    | RIG_STREAM_CAP_TIMED_TX_SAMPLE)))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: backend does not support timed TX\n", __func__);
            return -RIG_ENAVAIL;
        }

        if (stream->tx_horizon_ms > 0)
        {
            int64_t now_s;
            uint64_t now_ps;
            stream_time_now(&now_s, &now_ps);

            int64_t lead_ms = stream_time_diff_ms(info->seconds,
                                                  info->picoseconds,
                                                  now_s, now_ps);

            if (lead_ms > stream->tx_horizon_ms)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: target %lld ms ahead exceeds horizon %d ms\n",
                          __func__, (long long)lead_ms,
                          stream->tx_horizon_ms);
                return -RIG_EINVAL;
            }
        }
    }

    struct rig_stream_tx_target target;

    memset(&target, 0, sizeof(target));
    target.sample_index = rig_stream_get_samples_written(stream);
    target.flags = info->flags;

    if (info->time_valid)
    {
        target.flags |= RIG_STREAM_TIME_FLAG_TX_TIMED;
        target.seconds = info->seconds;
        target.picoseconds = info->picoseconds;
    }

    stream_push_tx_target(stream, &target);
    return RIG_OK;
}


int HAMLIB_API rig_stream_write(RIG *rig,
                                rig_stream_t *stream,
                                const void *buffer,
                                size_t buffer_size,
                                size_t *bytes_written,
                                int timeout_ms,
                                const struct rig_stream_write_info *info)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (!rig || !stream || !buffer || !bytes_written)
    {
        return -RIG_EINVAL;
    }

    /* Hold a reference so a concurrent close waits for this write to finish
     * before it destroys the ring and frees the stream. */
    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    int ret = RIG_OK;

    /* Muted: accept the write for the caller's pacing but discard it so
     * nothing is transmitted (neither the backend nor the ring sees it). */
    if (stream->muted)
    {
        *bytes_written = buffer_size;
        goto out;
    }

    /* Use backend hook if available */
    if (rig->caps->stream_write)
    {
        ret = rig->caps->stream_write(rig, stream, buffer, buffer_size,
                                      bytes_written, timeout_ms, info);
        goto out;
    }

    /* A burst target (scheduled time and/or SOB/EOB) binds to the producer
     * index of the first sample of this write. */
    if (info && (info->time_valid || info->flags))
    {
        ret = stream_bind_tx_target(stream, info);

        if (ret != RIG_OK)
        {
            goto out;
        }
    }

    int ovr_before = stream->ringbuf.overrun_count;
    *bytes_written = stream_ringbuf_write(&stream->ringbuf, buffer, buffer_size);

    /* A write that overwrote unread data is a local TX overrun: report it as a
     * write-status event (the ring already counted it in overrun_count). */
    if (stream->ringbuf.overrun_count != ovr_before)
    {
        struct rig_stream_write_status ev;
        memset(&ev, 0, sizeof(ev));
        ev.event = RIG_STREAM_WRITE_EVENT_OVERRUN;
        ev.sample_index = rig_stream_get_samples_written(stream);
        stream_record_write_status(stream, &ev, 0);
    }

out:
    stream_guard_leave(rig, stream);
    return ret;
}


/* ------------------------------------------------------------------ */
/* rig_stream_drain                                                    */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_drain(RIG *rig,
                                rig_stream_t *stream,
                                int timeout_ms)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream)
    {
        return -RIG_EINVAL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    int ret = RIG_OK;

    /* Use backend hook if available */
    if (rig->caps->stream_drain)
    {
        ret = rig->caps->stream_drain(rig, stream, timeout_ms);
        goto out;
    }

    /* Poll until ring buffer is empty or timeout expires */
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (stream_ringbuf_available(&stream->ringbuf) > 0)
    {
        usleep(1000);  /* 1ms poll interval */

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000
                          + (now.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= timeout_ms)
        {
            ret = -RIG_ETIMEOUT;
            goto out;
        }
    }

out:
    stream_guard_leave(rig, stream);
    return ret;
}


/* ------------------------------------------------------------------ */
/* rig_stream_get_hardware_time                                        */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_get_hardware_time(RIG *rig, rig_stream_t *stream,
        struct rig_stream_time_anchor *now)
{
    if (!rig || !stream || !now)
    {
        return -RIG_EINVAL;
    }

    /* Zero first so the reserved tail is cleared whether the backend or the
     * host fallback fills the anchor. */
    memset(now, 0, sizeof(*now));

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    int ret;

    if (rig->caps->stream_hardware_time)
    {
        ret = rig->caps->stream_hardware_time(rig, stream, now);
        goto out;
    }

    /* Host fallback: CLOCK_REALTIME, not tied to the radio sample clock */
    now->sample_index = rig_stream_get_samples_written(stream);
    stream_time_now(&now->seconds, &now->picoseconds);
    now->source = RIG_STREAM_TIME_SRC_HOST;
    now->accuracy = RIG_STREAM_TIME_ACC_MS;
    ret = RIG_OK;

out:
    stream_guard_leave(rig, stream);
    return ret;
}


/* ------------------------------------------------------------------ */
/* Loss accounting: producer index, gap marking, cause attribution     */
/* ------------------------------------------------------------------ */

/* Caller must hold stream->ringbuf.lock. */
uint64_t stream_first_readable_index_locked(struct rig_stream *stream)
{
    if (stream->frame_bytes <= 0)
    {
        return 0;
    }

    uint64_t consumed_bytes = stream->ringbuf.write_total
                              - stream->ringbuf.count;
    return consumed_bytes / (uint64_t)stream->frame_bytes
           + stream->skipped_samples;
}


void stream_skip_samples(struct rig_stream *stream, uint64_t dropped_samples,
                         uint8_t drop_flag)
{
    pthread_mutex_lock(&stream->ringbuf.lock);

    stream->pending_drop_flags |= drop_flag;

    switch (drop_flag)
    {
    case RIG_STREAM_DROP_GAP:
        stream->gap_count++;

        if (dropped_samples == 0)
        {
            stream->gaps_unknown++;
            stream->pending_drop_flags |= RIG_STREAM_DROP_UNSIZED;
        }
        else
        {
            stream->dropped_samples_gap += dropped_samples;
        }

        break;

    case RIG_STREAM_DROP_OVERRUN:
        stream->remote_overruns++;
        stream->dropped_samples_overrun += dropped_samples;
        break;

    case RIG_STREAM_DROP_LINK:
        stream->link_loss++;
        stream->dropped_samples_link += dropped_samples;
        break;

    default:
        break;
    }

    if (dropped_samples > 0)
    {
        stream->skipped_samples += dropped_samples;
        stream->skip_samples_unread += dropped_samples;
    }

    pthread_mutex_unlock(&stream->ringbuf.lock);
}


int HAMLIB_API rig_stream_get_stats(RIG *rig, rig_stream_t *stream,
                                    struct rig_stream_stats *stats)
{
    if (!rig || !stream || !stats)
    {
        return -RIG_EINVAL;
    }

    /* Zero the reserved tail (and any field not set below). */
    memset(stats, 0, sizeof(*stats));

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&stream->ringbuf.lock);

    /* overruns/underruns are LOCAL-ring only; remote losses are separate. */
    stats->overruns = (uint32_t)stream->ringbuf.overrun_count;
    stats->underruns = (uint32_t)stream->ringbuf.underrun_count;
    stats->gaps = (uint32_t)stream->gap_count;
    stats->gaps_unknown = stream->gaps_unknown;
    stats->link_loss = stream->link_loss;
    stats->tx_late = stream->tx_late;
    stats->remote_overruns = stream->remote_overruns;
    stats->remote_underruns = stream->remote_underruns;
    stats->write_events_dropped = stream->write_events_dropped;
    stats->dropped_samples_gap = stream->dropped_samples_gap;
    stats->dropped_samples_overrun = stream->dropped_samples_overrun;
    stats->dropped_samples_link = stream->dropped_samples_link;

    pthread_mutex_unlock(&stream->ringbuf.lock);

    stream_guard_leave(rig, stream);
    return RIG_OK;
}


void stream_write_event_init(struct rig_stream *stream)
{
    /* Match the ringbuf condvar clock so the timed wait uses one time base. */
#if defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0

    if (stream->ringbuf.use_monotonic)
    {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);

        if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0)
        {
            pthread_cond_init(&stream->write_event_available, &attr);
            pthread_cond_init(&stream->quiesced, &attr);
            pthread_condattr_destroy(&attr);
            return;
        }

        pthread_condattr_destroy(&attr);
    }

#endif
    pthread_cond_init(&stream->write_event_available, NULL);
    pthread_cond_init(&stream->quiesced, NULL);
}


void stream_write_event_destroy(struct rig_stream *stream)
{
    pthread_cond_destroy(&stream->write_event_available);
    pthread_cond_destroy(&stream->quiesced);
}


void stream_record_write_status(struct rig_stream *stream,
                                const struct rig_stream_write_status *ev,
                                int remote)
{
    if (!stream || !ev)
    {
        return;
    }

    /* Stored copy carries an authoritative REMOTE flag from the caller. */
    struct rig_stream_write_status e = *ev;

    if (remote)
    {
        e.flags |= RIG_STREAM_WRITE_STATUS_REMOTE;
    }
    else
    {
        e.flags &= ~RIG_STREAM_WRITE_STATUS_REMOTE;
    }

    pthread_mutex_lock(&stream->ringbuf.lock);

    /* Bump the matching aggregate counter. Local ring over/underruns are
     * already counted by the ring itself; only LATE and remote-reported
     * losses are tallied here. */
    switch (e.event)
    {
    case RIG_STREAM_WRITE_EVENT_LATE:
        stream->tx_late++;
        break;

    case RIG_STREAM_WRITE_EVENT_OVERRUN:
        if (remote)
        {
            stream->remote_overruns++;
        }

        break;

    case RIG_STREAM_WRITE_EVENT_UNDERRUN:
        if (remote)
        {
            stream->remote_underruns++;
        }

        break;

    default:
        break;
    }

    /* Push onto the FIFO; when full, overwriting the head slot drops the
     * oldest entry (head points at the oldest when count == depth). */
    if (stream->write_event_count >= RIG_STREAM_WRITE_EVENT_DEPTH)
    {
        stream->write_events_dropped++;
    }
    else
    {
        stream->write_event_count++;
    }

    stream->write_events[stream->write_event_head] = e;
    stream->write_event_head =
        (stream->write_event_head + 1) % RIG_STREAM_WRITE_EVENT_DEPTH;

    pthread_cond_signal(&stream->write_event_available);
    pthread_mutex_unlock(&stream->ringbuf.lock);
}


int HAMLIB_API rig_stream_wait_write_status(RIG *rig, rig_stream_t *stream,
        struct rig_stream_write_status *status, int timeout_ms)
{
    if (!rig || !stream || !status)
    {
        return -RIG_EINVAL;
    }

    /* The write-status event channel is TX-stream only. */
    if (stream->type != RIG_STREAM_TYPE_AUDIO_TX
            && stream->type != RIG_STREAM_TYPE_IQ_TX)
    {
        return -RIG_ENAVAIL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    int ret = -RIG_ETIMEOUT;
    struct timespec ts;
    int have_deadline = 0;

    pthread_mutex_lock(&stream->ringbuf.lock);

    /* Counted so a concurrent rig_stream_close() waits for this call to exit
     * before destroying the condvar and freeing the stream. */
    stream->blocked_waiters++;

    if (timeout_ms > 0)
    {
        clockid_t clk = stream->ringbuf.use_monotonic
                        ? CLOCK_MONOTONIC : CLOCK_REALTIME;
        clock_gettime(clk, &ts);
        ts.tv_sec  += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000L;

        if (ts.tv_nsec >= 1000000000L)
        {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }

        have_deadline = 1;
    }

    for (;;)
    {
        if (stream->write_event_closing)
        {
            ret = -RIG_ENAVAIL;
            break;
        }

        if (stream->write_event_count > 0)
        {
            int idx = (stream->write_event_head - stream->write_event_count
                       + RIG_STREAM_WRITE_EVENT_DEPTH)
                      % RIG_STREAM_WRITE_EVENT_DEPTH;
            *status = stream->write_events[idx];
            stream->write_event_count--;
            ret = RIG_OK;
            break;
        }

        if (timeout_ms == 0)
        {
            ret = -RIG_ETIMEOUT;
            break;
        }

        if (have_deadline)
        {
            if (pthread_cond_timedwait(&stream->write_event_available,
                                       &stream->ringbuf.lock, &ts) == ETIMEDOUT)
            {
                ret = -RIG_ETIMEOUT;
                break;
            }
        }
        else
        {
            pthread_cond_wait(&stream->write_event_available,
                              &stream->ringbuf.lock);
        }
    }

    if (--stream->blocked_waiters == 0 && stream->write_event_closing)
    {
        pthread_cond_signal(&stream->quiesced);
    }

    pthread_mutex_unlock(&stream->ringbuf.lock);

    stream_guard_leave(rig, stream);
    return ret;
}


/* ------------------------------------------------------------------ */
/* Producer position                                                   */
/* ------------------------------------------------------------------ */

uint64_t HAMLIB_API rig_stream_get_samples_written(const rig_stream_t *stream)
{
    if (!stream)
    {
        return 0;
    }

    /* The fields are lock-protected; cast away const for the lock only. */
    struct rig_stream *s = (struct rig_stream *)stream;

    pthread_mutex_lock(&s->ringbuf.lock);

    uint64_t written = s->frame_bytes > 0
                       ? s->ringbuf.write_total / (uint64_t)s->frame_bytes
                       : 0;
    written += s->skipped_samples;

    pthread_mutex_unlock(&s->ringbuf.lock);

    return written;
}


/* ------------------------------------------------------------------ */
/* rig_stream_get_type / get_id                                        */
/* ------------------------------------------------------------------ */

rig_stream_type_t HAMLIB_API rig_stream_get_type(const rig_stream_t *stream)
{
    if (!stream)
    {
        return RIG_STREAM_TYPE_COUNT;
    }

    return stream->type;
}

int HAMLIB_API rig_stream_get_id(const rig_stream_t *stream)
{
    if (!stream)
    {
        return -1;
    }

    return stream->id;
}

int HAMLIB_API rig_stream_get_max_payload(const rig_stream_t *stream)
{
    if (!stream)
    {
        return -1;
    }

    return stream->max_payload;
}


/* ------------------------------------------------------------------ */
/* rig_stream_pause / resume                                           */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_pause(RIG *rig, rig_stream_t *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream)
    {
        return -RIG_EINVAL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    /* Commit the frontend flag only if the backend hook succeeds, so a failed
     * pause does not leave the frontend suppressing reads while the device
     * keeps sending. */
    int ret = RIG_OK;

    if (rig->caps->stream_pause)
    {
        ret = rig->caps->stream_pause(rig, stream);
    }

    if (ret == RIG_OK)
    {
        stream->paused = 1;
    }

    stream_guard_leave(rig, stream);
    return ret;
}

int HAMLIB_API rig_stream_resume(RIG *rig, rig_stream_t *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream)
    {
        return -RIG_EINVAL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    int ret = RIG_OK;

    if (rig->caps->stream_resume)
    {
        ret = rig->caps->stream_resume(rig, stream);
    }

    if (ret == RIG_OK)
    {
        stream->paused = 0;
    }

    stream_guard_leave(rig, stream);
    return ret;
}


/* ------------------------------------------------------------------ */
/* rig_stream_mute / unmute                                            */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_mute(RIG *rig, rig_stream_t *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream)
    {
        return -RIG_EINVAL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    stream->muted = 1;
    stream_guard_leave(rig, stream);
    return RIG_OK;
}

int HAMLIB_API rig_stream_unmute(RIG *rig, rig_stream_t *stream)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream)
    {
        return -RIG_EINVAL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    stream->muted = 0;
    stream_guard_leave(rig, stream);
    return RIG_OK;
}


/* ------------------------------------------------------------------ */
/* Metadata — internal helpers                                         */
/* ------------------------------------------------------------------ */

/* Coarse wire VFO id: 0 for Main/A-class VFOs, 1 for Sub/B-class. */
static uint8_t stream_vfo_to_id(vfo_t vfo)
{
    switch (vfo)
    {
    case RIG_VFO_B:
    case RIG_VFO_SUB:
    case RIG_VFO_MAIN_B:
    case RIG_VFO_SUB_A:
    case RIG_VFO_SUB_B:
    case RIG_VFO_SUB_C:
        return 1;

    default:
        return 0;
    }
}


static int cache_to_metadata(RIG *rig, rig_stream_t *stream,
                             struct rig_stream_metadata *meta)
{
    struct rig_cache *cache = CACHE(rig);
    vfo_t vfo = (stream->vfo != RIG_VFO_NONE) ? stream->vfo : RIG_VFO_CURR;
    freq_t freq = 0;
    int cache_ms = 0;

    memset(meta, 0, sizeof(*meta));

    /* Frequency via Hamlib's canonical VFO->cache-slot mapping.  This is a
     * non-blocking cache read — it never triggers rig I/O in the feeder. */
    rig_get_cache_freq(rig, vfo, &freq, &cache_ms);
    meta->vfo_freq = freq;
    meta->field_mask |= RIG_STREAM_META_VFO_FREQ;

    /* I/Q streams carry the RF window center.  Backends that tune the window
     * independently of the dial (panadapter DAX-IQ) set stream->center_freq;
     * otherwise the window tracks the VFO. */
    if (stream->type == RIG_STREAM_TYPE_IQ_RX
            || stream->type == RIG_STREAM_TYPE_IQ_TX)
    {
        /* Snapshot under ringbuf.lock: center_freq is a freq_t written by a
         * backend feeder thread, so an unlocked read could tear on 32-bit. */
        pthread_mutex_lock(&stream->ringbuf.lock);
        freq_t center = stream->center_freq;
        pthread_mutex_unlock(&stream->ringbuf.lock);

        meta->center_freq = (center != 0) ? center : freq;
        meta->field_mask |= RIG_STREAM_META_CENTER_FREQ;
    }

    /* PTT snapshot.  Hold the rig lock to avoid a torn read. */
    rig_lock(rig, 1);
    meta->ptt = (cache->ptt != RIG_PTT_OFF) ? 1 : 0;
    rig_lock(rig, 0);
    meta->field_mask |= RIG_STREAM_META_PTT;

    meta->vfo_id = stream_vfo_to_id(vfo);
    meta->field_mask |= RIG_STREAM_META_VFO_ID;

    return RIG_OK;
}


/* Compare two metadata structs, ignoring sample_index.
 * Returns 1 if any data field differs, 0 if identical. */
int stream_metadata_changed(const struct rig_stream_metadata *a,
                            const struct rig_stream_metadata *b)
{
    if (a->field_mask != b->field_mask)
    {
        return 1;
    }

    if (a->center_freq != b->center_freq)
    {
        return 1;
    }

    if (a->vfo_freq != b->vfo_freq)
    {
        return 1;
    }

    if (a->ptt != b->ptt)
    {
        return 1;
    }

    if (a->vfo_id != b->vfo_id)
    {
        return 1;
    }

    return 0;
}


/* ------------------------------------------------------------------ */
/* Metadata — public API                                               */
/* ------------------------------------------------------------------ */

int HAMLIB_API rig_stream_read_metadata(RIG *rig,
                                        rig_stream_t *stream,
                                        struct rig_stream_metadata *meta)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream || !meta)
    {
        return -RIG_EINVAL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    int ret = cache_to_metadata(rig, stream, meta);
    stream_guard_leave(rig, stream);
    return ret;
}

int HAMLIB_API rig_stream_write_metadata(RIG *rig,
        rig_stream_t *stream,
        const struct rig_stream_metadata *meta)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !stream || !meta)
    {
        return -RIG_EINVAL;
    }

    /* Reject non-finite or out-of-range frequencies before anything applies
     * them: metadata can arrive from the wire (netrigctl/rigctld TX), and a
     * NaN/Inf/negative freq_t converted to an integer in rig_set_freq() or a
     * backend is undefined behaviour. Centralised so every apply path is
     * covered, including the backend hook below. */
    if ((meta->field_mask & RIG_STREAM_META_VFO_FREQ)
            && (!isfinite(meta->vfo_freq) || meta->vfo_freq <= 0))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: rejecting invalid vfo_freq metadata\n", __func__);
        return -RIG_EINVAL;
    }

    if ((meta->field_mask & RIG_STREAM_META_CENTER_FREQ)
            && (!isfinite(meta->center_freq) || meta->center_freq <= 0))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: rejecting invalid center_freq metadata\n", __func__);
        return -RIG_EINVAL;
    }

    if (stream_guard_enter(rig, stream) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    int ret = RIG_OK;

    /* If backend has a metadata_apply hook, use it for timed commands */
    if (rig->caps->stream_apply_metadata)
    {
        ret = rig->caps->stream_apply_metadata(rig, stream, meta);
        goto out;
    }

    /* Default: apply the metadata fields to the rig, targeting the stream's
     * own VFO (not RIG_VFO_CURR).  Backends override via stream_apply_metadata
     * for timed/device-specific behaviour. */
    vfo_t vfo = (stream->vfo != RIG_VFO_NONE) ? stream->vfo : RIG_VFO_CURR;

    if ((meta->field_mask & RIG_STREAM_META_VFO_ID) && vfo != RIG_VFO_CURR)
    {
        int r = rig_set_vfo(rig, vfo);

        if (r != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: set_vfo failed: %d\n",
                      __func__, r);
        }
    }

    if ((meta->field_mask & RIG_STREAM_META_VFO_FREQ)
            && (!(stream->last_metadata.field_mask & RIG_STREAM_META_VFO_FREQ)
                || meta->vfo_freq != stream->last_metadata.vfo_freq))
    {
        ret = rig_set_freq(rig, vfo, meta->vfo_freq);

        if (ret != RIG_OK)
        {
            goto out;
        }
    }

    if ((meta->field_mask & RIG_STREAM_META_PTT)
            && (!(stream->last_metadata.field_mask & RIG_STREAM_META_PTT)
                || meta->ptt != stream->last_metadata.ptt))
    {
        ret = rig_set_ptt(rig, vfo,
                          meta->ptt ? RIG_PTT_ON : RIG_PTT_OFF);

        if (ret != RIG_OK)
        {
            goto out;
        }
    }

    stream->last_metadata = *meta;
    ret = RIG_OK;

out:
    stream_guard_leave(rig, stream);
    return ret;
}
