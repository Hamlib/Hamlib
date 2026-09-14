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
    stream_conv_free(stream->conv);
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
    free(state->session_caps);
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

/* Format families reachable from one another via rig_stream_convert.
 * Codec formats (Opus, ...) are transport features, not sample-format
 * conversion, and pass through the derivation untouched. */
#define STREAM_PCM_FORMAT_MASK (RIG_STREAM_FORMAT_PCM_S8  \
                                | RIG_STREAM_FORMAT_PCM_U8  \
                                | RIG_STREAM_FORMAT_PCM_S16 \
                                | RIG_STREAM_FORMAT_PCM_F32)
#define STREAM_IQ_FORMAT_MASK  (RIG_STREAM_FORMAT_IQ_CS8  \
                                | RIG_STREAM_FORMAT_IQ_CU8  \
                                | RIG_STREAM_FORMAT_IQ_CS16 \
                                | RIG_STREAM_FORMAT_IQ_CF32)

#ifdef HAVE_SAMPLERATE
/* Standard rates advertised in the effective set when the resampler is
 * built (bounded by the largest native rate; I/Q strictly below it). */
static const int stream_curated_rates[] =
{
    8000, 11025, 16000, 22050, 24000, 44100, 48000, 96000, 192000, 0
};

/* Largest integer-decimation factor advertised (exact divisions only). */
#define STREAM_MAX_DECIM_FACTOR 10
#endif

static int stream_type_is_iq_type(rig_stream_type_t type)
{
    return type == RIG_STREAM_TYPE_IQ_RX || type == RIG_STREAM_TYPE_IQ_TX;
}

#ifdef HAVE_SAMPLERATE
/* Append rate to a 0-terminated list if absent and there is room; the
 * caller adds in priority order, so on overflow the least useful entries
 * (highest decimation factors, added last) are the ones trimmed. */
static void stream_rate_list_add(int *rates, int rate)
{
    int i;

    for (i = 0; i < HAMLIB_MAX_STREAM_RATES - 1 && rates[i] != 0; i++)
    {
        if (rates[i] == rate)
        {
            return;
        }
    }

    if (i < HAMLIB_MAX_STREAM_RATES - 1)
    {
        rates[i] = rate;
        rates[i + 1] = 0;
    }
}

static int stream_rate_list_max(const int *rates)
{
    int max = 0;

    for (int i = 0; i < HAMLIB_MAX_STREAM_RATES && rates[i] != 0; i++)
    {
        if (rates[i] > max)
        {
            max = rates[i];
        }
    }

    return max;
}

#endif /* HAVE_SAMPLERATE */

/* Ascending int comparator for caps list sorting (rates and channel
 * counts alike). */
static int stream_rate_cmp(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

/* True if count appears in a 0-terminated channel-count list. */
static int chan_in_list(const int *list, int count)
{
    for (int i = 0; i < HAMLIB_MAX_STREAM_CHANNEL_COUNTS && list[i] != 0; i++)
    {
        if (list[i] == count)
        {
            return 1;
        }
    }

    return 0;
}

/* Append count to a 0-terminated list if absent and there is room; the
 * native entries are present before any additions, so an overflow trims
 * only conversion-reachable extras. */
static void chan_list_add(int *list, int count)
{
    int i;

    for (i = 0; i < HAMLIB_MAX_STREAM_CHANNEL_COUNTS - 1 && list[i] != 0; i++)
    {
        if (list[i] == count)
        {
            return;
        }
    }

    if (i < HAMLIB_MAX_STREAM_CHANNEL_COUNTS - 1)
    {
        list[i] = count;
    }
}

/* Widen one backend-declared (native) descriptor into the app-visible
 * derived view: classic fields become the effective set, native_* keeps
 * the hardware truth. Rules per HAMLIB_STREAMING_FORMAT_CONVERSION.md
 * section 3.4. */
static void stream_derive_caps(struct rig_stream_caps *dst,
                               const struct rig_stream_caps *src)
{
    int is_iq = stream_type_is_iq_type(src->type);

    *dst = *src;

    /* A backend that already declares BOTH views (native_formats set) is
     * serving pre-derived caps — netrigctl relays the server's effective
     * and native sets, and the server's derivation is authoritative
     * because conversion happens there. Pass through verbatim rather
     * than re-deriving effective-of-effective. */
    if (src->native_formats != 0)
    {
        return;
    }

    /* Native view = what the backend declared. */
    dst->native_formats = src->formats;
    memcpy(dst->native_sample_rates, src->sample_rates,
           sizeof(dst->native_sample_rates));
    memcpy(dst->native_channels, src->channels,
           sizeof(dst->native_channels));

    /* Formats: whole family reachable via rig_stream_convert; codec bits
     * pass through as declared. */
    if (src->formats & STREAM_PCM_FORMAT_MASK)
    {
        dst->formats |= STREAM_PCM_FORMAT_MASK;
    }

    if (src->formats & STREAM_IQ_FORMAT_MASK)
    {
        dst->formats |= STREAM_IQ_FORMAT_MASK;
    }

    /* A codec-only entry (no raw-family bits) is served verbatim beyond
     * this point: codec streams are opaque packets that no conversion
     * stage applies to, so the rate/channel widening below would only
     * advertise configurations rig_stream_open() must refuse. */
    if (!(src->formats & (STREAM_PCM_FORMAT_MASK | STREAM_IQ_FORMAT_MASK)))
    {
        return;
    }

    /* Channels: the declared list is exact — counts are never invented.
     * The one exception is the audio mono<->stereo map: when the
     * hardware offers either count of the {1, 2} pair, the other is
     * openable through the frontend's upmix/downmix. I/Q channels are
     * coherent captures and never widen; a backend able to open a
     * subset of its hardware channels declares each openable count
     * explicitly. The native entries are already in the list
     * (whole-struct copy above). */
    if (!is_iq
            && (chan_in_list(src->channels, 1)
                || chan_in_list(src->channels, 2)))
    {
        int n;

        chan_list_add(dst->channels, 1);
        chan_list_add(dst->channels, 2);

        for (n = 0; n < HAMLIB_MAX_STREAM_CHANNEL_COUNTS
                && dst->channels[n] != 0; n++)
        {
        }

        qsort(dst->channels, n, sizeof(int), stream_rate_cmp);
    }

#ifdef HAVE_SAMPLERATE
    /* Rates (resampler built): native, then curated standards, then
     * integer divisions of each native rate (factors 2..10, exact results
     * only) — in that priority order, deduplicated, ascending. This list
     * is discoverability; acceptance at open is rule-based. Audio may go
     * up to the largest native rate, I/Q only strictly below it
     * (downward-only: the I/Q sample rate is the represented bandwidth).
     * Without the resampler the effective rates equal the native rates. */
    {
        int native_max = stream_rate_list_max(src->sample_rates);
        int n;

        for (int i = 0; stream_curated_rates[i] != 0; i++)
        {
            int r = stream_curated_rates[i];

            if ((is_iq && r < native_max) || (!is_iq && r <= native_max))
            {
                stream_rate_list_add(dst->sample_rates, r);
            }
        }

        for (int i = 0; i < HAMLIB_MAX_STREAM_RATES
                && src->sample_rates[i] != 0; i++)
        {
            for (int f = 2; f <= STREAM_MAX_DECIM_FACTOR; f++)
            {
                if (src->sample_rates[i] % f == 0)
                {
                    stream_rate_list_add(dst->sample_rates,
                                         src->sample_rates[i] / f);
                }
            }
        }

        for (n = 0; n < HAMLIB_MAX_STREAM_RATES && dst->sample_rates[n] != 0;
                n++)
        {
        }

        qsort(dst->sample_rates, n, sizeof(int), stream_rate_cmp);
    }
#endif /* HAVE_SAMPLERATE */
}

/* Return the app-visible derived caps array, building or refreshing it
 * under stream_mutex. Before rig_open (no stream state yet) the backend's
 * raw declaration is served unchanged — the derived view, including the
 * native_* fields, is available once the rig is open. */
static const struct rig_stream_caps *stream_served_caps(RIG *rig)
{
    if (!rig || !rig->caps)
    {
        return NULL;
    }

    struct rig_stream_state *ss = get_stream_state(rig);

    if (!ss)
    {
        return rig->caps->stream_caps;
    }

    pthread_mutex_lock(&ss->stream_mutex);

    /* The session's own truth when the backend has published one, otherwise
     * the model declaration. A session-caps backend need not declare any
     * model caps at all (netrigctl's whole capability is per-connection),
     * so only the SELECTED source has to exist. */
    const struct rig_stream_caps *want = ss->session_caps
                                         ? ss->session_caps
                                         : rig->caps->stream_caps;

    if (want == NULL)
    {
        pthread_mutex_unlock(&ss->stream_mutex);
        return NULL;
    }

    if (ss->derived_src != want)
    {
        const struct rig_stream_caps *src = want;

        memset(ss->derived_caps, 0, sizeof(ss->derived_caps));

        for (int i = 0; i < HAMLIB_MAX_STREAM_CAPS; i++)
        {
            if (src[i].formats == 0)
            {
                break;  /* 0-terminated */
            }

            stream_derive_caps(&ss->derived_caps[i], &src[i]);
        }

        ss->derived_src = src;
    }

    pthread_mutex_unlock(&ss->stream_mutex);
    return ss->derived_caps;
}


int stream_set_session_caps(RIG *rig, const struct rig_stream_caps *caps,
                            int count)
{
    struct rig_stream_state *ss;
    int i;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    if (caps && (count < 0 || count > HAMLIB_MAX_STREAM_CAPS))
    {
        return -RIG_EINVAL;
    }

    ss = get_stream_state(rig);

    if (!ss)
    {
        /* Backends publish from their rig_open hook, which the frontend
         * calls BEFORE its own stream-state init: create the state on
         * first use here; the later init finds it and leaves it alone. */
        if (rig_stream_state_init((struct rig_stream_state **)
                                  &STATE(rig)->stream_state) != 0)
        {
            return -RIG_ENOMEM;
        }

        ss = get_stream_state(rig);
    }

    pthread_mutex_lock(&ss->stream_mutex);

    if (caps == NULL || count == 0)
    {
        free(ss->session_caps);
        ss->session_caps = NULL;
    }
    else
    {
        if (ss->session_caps == NULL)
        {
            /* Only a backend that negotiates per connection pays for this,
             * and rig_stream_state is allocated for every rig. */
            ss->session_caps = calloc(HAMLIB_MAX_STREAM_CAPS,
                                      sizeof(*ss->session_caps));

            if (ss->session_caps == NULL)
            {
                pthread_mutex_unlock(&ss->stream_mutex);
                return -RIG_ENOMEM;
            }
        }

        memset(ss->session_caps, 0,
               HAMLIB_MAX_STREAM_CAPS * sizeof(*ss->session_caps));

        for (i = 0; i < count; i++)
        {
            ss->session_caps[i] = caps[i];
        }

        /* The array is 0-terminated by type, so a full-length publication has
         * no terminator to spare; readers stop at HAMLIB_MAX_STREAM_CAPS. */
    }

    /* The derived view is rebuilt on the next query rather than compared by
     * source pointer: republishing into the same array would otherwise go
     * unnoticed. */
    ss->derived_src = NULL;

    pthread_mutex_unlock(&ss->stream_mutex);
    return RIG_OK;
}


int HAMLIB_API rig_stream_caps_count(RIG *rig)
{
    const struct rig_stream_caps *src = stream_served_caps(rig);
    int count = 0;

    if (!src)
    {
        return 0;
    }

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
    const struct rig_stream_caps *src = stream_served_caps(rig);

    if (!src || index < 0 || index >= HAMLIB_MAX_STREAM_CAPS)
    {
        return NULL;
    }

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
/* The backend-authored capability source for this rig: the session
 * publication when one exists, else the model declaration. This is the
 * open-time resolution input; stream_served_caps() derives the
 * app-visible view from the same source, so acceptance and advertisement
 * cannot disagree. Session entries keep their authored form here — a
 * local backend's entries resolve rule-based with a conversion pipeline,
 * a relay's pre-derived entries resolve delegated. */
static const struct rig_stream_caps *stream_source_caps(RIG *rig)
{
    struct rig_stream_state *ss = get_stream_state(rig);

    if (ss)
    {
        pthread_mutex_lock(&ss->stream_mutex);
        const struct rig_stream_caps *sc = ss->session_caps;
        pthread_mutex_unlock(&ss->stream_mutex);

        if (sc)
        {
            return sc;
        }
    }

    return rig->caps->stream_caps;
}

static const struct rig_stream_caps *find_stream_caps(RIG *rig,
        rig_stream_type_t type)
{
    const struct rig_stream_caps *caps_arr = stream_source_caps(rig);

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
/* True if rate appears in a 0-terminated rate list. */
static int rate_in_list(const int *rates, int rate)
{
    for (int i = 0; i < HAMLIB_MAX_STREAM_RATES && rates[i] != 0; i++)
    {
        if (rates[i] == rate)
        {
            return 1;
        }
    }

    return 0;
}

/* True if rate appears in the caps' 0-terminated native list. */
static int caps_rate_native(const struct rig_stream_caps *caps, int rate)
{
    return rate_in_list(caps->sample_rates, rate);
}


#ifdef HAVE_SAMPLERATE
/* Smallest native rate >= rate (the cheapest downconversion source), or 0
 * when the request exceeds every native rate — the effective set is
 * bounded by the largest native rate for audio and I/Q alike. */
static int caps_rate_source(const struct rig_stream_caps *caps, int rate)
{
    int best = 0;

    for (int i = 0; i < HAMLIB_MAX_STREAM_RATES
            && caps->sample_rates[i] != 0; i++)
    {
        int r = caps->sample_rates[i];

        if (r >= rate && (best == 0 || r < best))
        {
            best = r;
        }
    }

    return best;
}
#endif

/* Resolution for a pre-derived caps entry (native_formats set — a relaying
 * backend such as netrigctl): conversion happens on the far side of the
 * backend (the server), so the local backend runs at the requested config
 * verbatim and no local pipeline is installed. Acceptance is membership in
 * the relayed EFFECTIVE sets — the server's advertisement is authoritative
 * for what it will serve. The conversion stages are computed against the
 * relayed NATIVE view so the indicator is correct immediately (the relaying
 * backend overrides it with the server-reported value after open). */
static int resolve_delegated_source(const struct rig_stream_config *config,
                                    const struct rig_stream_caps *caps,
                                    int *conversions)
{
    int conv = RIG_STREAM_CONV_NONE;

    if (!(config->format & caps->formats))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: format 0x%x not in the relayed effective set 0x%x\n",
                  __func__, config->format, caps->formats);
        return -RIG_EINVAL;
    }

    if (!(config->format & caps->native_formats))
    {
        conv |= RIG_STREAM_CONV_FORMAT;
    }

    /* Codec formats are native-only on the far side too: same rule as
     * resolve_stream_source(), against the relayed native view. */
    if (!(config->format & (STREAM_PCM_FORMAT_MASK | STREAM_IQ_FORMAT_MASK)))
    {
        if (!rate_in_list(caps->native_sample_rates, config->sample_rate))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: codec format 0x%x requires a native rate "
                      "(got %d)\n",
                      __func__, config->format, config->sample_rate);
            return -RIG_EINVAL;
        }

        if (caps->native_channels[0] != 0
                && !chan_in_list(caps->native_channels, config->channels))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: codec format 0x%x requires a native channel "
                      "count (got %d)\n",
                      __func__, config->format, config->channels);
            return -RIG_EINVAL;
        }

        *conversions = RIG_STREAM_CONV_NONE;
        return RIG_OK;
    }

    if (!rate_in_list(caps->sample_rates, config->sample_rate))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: rate %d not offered by the relayed effective set\n",
                  __func__, config->sample_rate);
        return -RIG_EINVAL;
    }

    if (!rate_in_list(caps->native_sample_rates, config->sample_rate))
    {
        conv |= RIG_STREAM_CONV_RATE;
    }

    if (caps->channels[0] != 0
            && !chan_in_list(caps->channels, config->channels))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: channels %d not in the relayed effective list\n",
                  __func__, config->channels);
        return -RIG_EINVAL;
    }

    if (caps->native_channels[0] != 0
            && !chan_in_list(caps->native_channels, config->channels))
    {
        conv |= RIG_STREAM_CONV_CHANNELS;
    }

    *conversions = conv;
    return RIG_OK;
}


/* Validate the requested config against the backend's native caps under
 * the effective-set acceptance rules (design: HAMLIB_STREAMING_FORMAT_
 * CONVERSION.md section 3.4) and resolve the native SOURCE the backend
 * will run at. On success, *backend_cfg holds the request with format /
 * sample_rate / channels replaced by the selected native values, and
 * *conversions the RIG_STREAM_CONV_* stages that requires.
 * Returns -RIG_EINVAL when the request is outside the effective set. */
static int resolve_stream_source(const struct rig_stream_config *config,
                                 const struct rig_stream_caps *caps,
                                 struct rig_stream_config *backend_cfg,
                                 int *conversions)
{
    int is_iq = config->type == RIG_STREAM_TYPE_IQ_RX
                || config->type == RIG_STREAM_TYPE_IQ_TX;
    rig_stream_format_t family = is_iq ? STREAM_IQ_FORMAT_MASK
                                 : STREAM_PCM_FORMAT_MASK;
    int conv = RIG_STREAM_CONV_NONE;

    *backend_cfg = *config;
    *conversions = RIG_STREAM_CONV_NONE;

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

    /* The requested format must be exactly one format bit. A multi-bit
     * value would otherwise resolve to sample_size 0, silently degrading
     * the stream to compressed handling. */
    if (config->format == 0
            || (config->format & (config->format - 1)) != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: format 0x%x is not a single format\n",
                  __func__, config->format);
        return -RIG_EINVAL;
    }

    /* Pre-derived entry: the relaying backend's far side converts;
     * backend_cfg stays equal to the request. */
    if (caps->native_formats != 0)
    {
        return resolve_delegated_source(config, caps, conversions);
    }

    /* Format: native, or reachable within the family via conversion.
     * Codec formats (outside the family masks) must be native. */
    if (!(config->format & caps->formats))
    {
        if (!(config->format & family) || !(caps->formats & family))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: format 0x%x not reachable from native 0x%x\n",
                      __func__, config->format, caps->formats);
            return -RIG_EINVAL;
        }

        /* Source preference: the float format (lossless staging), then
         * S16, then whatever the hardware has. */
        rig_stream_format_t f32 = is_iq ? RIG_STREAM_FORMAT_IQ_CF32
                                  : RIG_STREAM_FORMAT_PCM_F32;
        rig_stream_format_t s16 = is_iq ? RIG_STREAM_FORMAT_IQ_CS16
                                  : RIG_STREAM_FORMAT_PCM_S16;

        if (caps->formats & f32)
        {
            backend_cfg->format = f32;
        }
        else if (caps->formats & s16)
        {
            backend_cfg->format = s16;
        }
        else
        {
            rig_stream_format_t avail = caps->formats & family;
            backend_cfg->format = avail & ~(avail - 1);  /* lowest set bit */
        }

        conv |= RIG_STREAM_CONV_FORMAT;
    }

    /* Codec (compressed) formats are opaque packet streams, not raw
     * samples: no stage of the conversion pipeline applies to them. The
     * whole config must match the native declaration. Keyed on the family
     * masks so any codec bit — present or future — takes this path. */
    if (!(config->format & (STREAM_PCM_FORMAT_MASK | STREAM_IQ_FORMAT_MASK)))
    {
        if (!caps_rate_native(caps, config->sample_rate))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: codec format 0x%x requires a native rate "
                      "(got %d)\n",
                      __func__, config->format, config->sample_rate);
            return -RIG_EINVAL;
        }

        if (caps->channels[0] != 0
                && !chan_in_list(caps->channels, config->channels))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: codec format 0x%x requires a native channel "
                      "count (got %d)\n",
                      __func__, config->format, config->channels);
            return -RIG_EINVAL;
        }

        return RIG_OK;    /* conv stays RIG_STREAM_CONV_NONE */
    }

    /* Rate: native, or (resampler built) any rate up to the largest
     * native rate — audio and I/Q alike; I/Q upsampling beyond the
     * hardware is impossible by this bound (sample rate = bandwidth). */
    if (!caps_rate_native(caps, config->sample_rate))
    {
#ifdef HAVE_SAMPLERATE
        int src_rate = caps_rate_source(caps, config->sample_rate);

        if (src_rate <= 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: rate %d exceeds the largest native rate\n",
                      __func__, config->sample_rate);
            return -RIG_EINVAL;
        }

        backend_cfg->sample_rate = src_rate;
        conv |= RIG_STREAM_CONV_RATE;
#else
        rig_debug(RIG_DEBUG_ERR,
                  "%s: sample rate %d not native and resampling is not "
                  "built in\n", __func__, config->sample_rate);
        return -RIG_EINVAL;
#endif
    }

    /* Channels: a count in the native list passes through untouched. The
     * only conversion is the audio mono<->stereo map between the {1, 2}
     * pair; any other count is outside the effective set — the declared
     * list is exact and gaps are never filled. */
    if (caps->channels[0] != 0
            && !chan_in_list(caps->channels, config->channels))
    {
        if (!is_iq && config->channels == 2
                && chan_in_list(caps->channels, 1))
        {
            backend_cfg->channels = 1;                   /* mono upmix */
        }
        else if (!is_iq && config->channels == 1
                 && chan_in_list(caps->channels, 2))
        {
            backend_cfg->channels = 2;                   /* stereo downmix */
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: channels %d not in the native count list\n",
                      __func__, config->channels);
            return -RIG_EINVAL;
        }

        conv |= RIG_STREAM_CONV_CHANNELS;
    }

    *conversions = conv;
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

    struct rig_stream_config backend_cfg;
    int conversions = RIG_STREAM_CONV_NONE;
    int cfg_ret = resolve_stream_source(config, found_caps, &backend_cfg,
                                        &conversions);

    if (cfg_ret != RIG_OK)
    {
        return cfg_ret;
    }

    /* The request is servable, but only through conversion: a client that
     * demanded a native stream gets a distinct refusal (-RIG_ENAVAIL, vs
     * -RIG_EINVAL for the outright impossible). */
    if (conversions != RIG_STREAM_CONV_NONE && config->require_native)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: config requires conversions 0x%x but require_native "
                  "is set\n", __func__, conversions);
        return -RIG_ENAVAIL;
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

    /* The backend runs at the resolved native source; equal to the request
     * on a native stream. backend_cfg is a full struct copy of the request
     * with format/rate/channels replaced, so backends read it directly. */
    backend_cfg.struct_size = sizeof(s->backend_config);
    s->backend_config = backend_cfg;
    s->conversions = conversions;

    /* The ring buffer holds the CONSUMER's format: the client's request on
     * RX, the backend-native side on TX. */
    int is_rx = config->type == RIG_STREAM_TYPE_AUDIO_RX
                || config->type == RIG_STREAM_TYPE_IQ_RX;
    const struct rig_stream_config *ring_cfg = is_rx ? config
                                               : &s->backend_config;

    /* Bytes per frame for producer-index accounting; 0 for compressed
     * formats. A codec (compressed) stream's ring carries length-prefixed
     * codec-frame records instead of raw bytes; its producer index is the
     * decoded-sample position accumulated from frame durations. */
    {
        int sample_size = rig_stream_format_sample_size(ring_cfg->format);
        int channels = ring_cfg->channels > 0 ? ring_cfg->channels : 1;
        s->frame_bytes = sample_size > 0 ? sample_size * channels : 0;
        s->is_codec = ring_cfg->format != 0 && sample_size == 0;
    }

    /* Effective sender payload budget from the configured (clamped) MTU. */
    s->max_payload = stream_max_payload_from_mtu(config->mtu, s->frame_bytes);

    s->caps_flags = found_caps->caps_flags;
    s->tx_horizon_ms = found_caps->tx_schedule_horizon_ms;

    resolve_stale_thresholds(rig, config, s);

    /* Conversion pipeline: producer side of the ring — backend->request
     * for RX, request->backend for TX. A pre-derived caps entry delegates
     * the conversion to the backend's far side (the stages are still
     * reported), so no local pipeline is installed for it. */
    if (conversions != RIG_STREAM_CONV_NONE && found_caps->native_formats == 0)
    {
        int is_iq = config->type == RIG_STREAM_TYPE_IQ_RX
                    || config->type == RIG_STREAM_TYPE_IQ_TX;
        int quality = stream_resample_quality(rig);
        int conv_ret;

        if (is_rx)
        {
            conv_ret = stream_conv_init(&s->conv,
                                        backend_cfg.format,
                                        backend_cfg.sample_rate,
                                        backend_cfg.channels,
                                        config->format,
                                        config->sample_rate,
                                        config->channels, is_iq, quality);
        }
        else
        {
            conv_ret = stream_conv_init(&s->conv,
                                        config->format,
                                        config->sample_rate,
                                        config->channels,
                                        backend_cfg.format,
                                        backend_cfg.sample_rate,
                                        backend_cfg.channels, is_iq, quality);
        }

        if (conv_ret != 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: conversion pipeline init failed (0x%x)\n",
                      __func__, conversions);
            free(s);
            pthread_mutex_unlock(&ss->stream_mutex);
            return -RIG_ENOMEM;
        }
    }

    /* Initialize ring buffer. A codec ring is sized from the buffer
     * duration as a worst case — one max_payload frame per 10 ms (the
     * assumed shortest codec cadence) plus record headers — because the
     * sample-rate math of buffer_size_from_config() has no meaning for
     * compressed payloads. buffer_bytes still wins verbatim. */
    size_t buf_size;

    if (s->is_codec && ring_cfg->buffer_bytes == 0)
    {
        unsigned int dur_ms = ring_cfg->buffer_duration_ms > 0
                              ? ring_cfg->buffer_duration_ms
                              : RIG_STREAM_AUDIO_BUF_DEFAULT_MS;
        size_t slots = (dur_ms + 9) / 10;

        if (slots < 1)
        {
            slots = 1;
        }

        buf_size = slots * ((size_t)s->max_payload
                            + STREAM_CODEC_REC_HDR_SIZE);

        if (buf_size < 4096)
        {
            buf_size = 4096;
        }
    }
    else
    {
        buf_size = buffer_size_from_config(ring_cfg);
    }


    if (stream_ringbuf_init(&s->ringbuf, buf_size) != 0)
    {
        stream_conv_free(s->conv);
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
        stream_conv_free(s->conv);
        free(s);
        return ret;
    }

    *stream = s;
    return RIG_OK;
}


/* ------------------------------------------------------------------ */
/* Backend-facing produce path                                         */
/* ------------------------------------------------------------------ */

static size_t conv_ring_sink(void *ctx, const void *buf, size_t len)
{
    struct rig_stream *s = ctx;

    return stream_ringbuf_write(&s->ringbuf, buf, len);
}

size_t stream_backend_write(struct rig_stream *stream, const void *buf,
                            size_t bytes)
{
    if (!stream || !buf)
    {
        return 0;
    }

    if (stream->is_codec)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: codec stream requires stream_backend_write_frame()\n",
                  __func__);
        return 0;
    }

    if (!stream->conv)
    {
        return stream_ringbuf_write(&stream->ringbuf, buf, bytes);
    }

    ssize_t n = stream_conv_process(stream->conv, buf, bytes,
                                    conv_ring_sink, stream);

    return n < 0 ? 0 : (size_t)n;
}


/* ------------------------------------------------------------------ */
/* Codec-frame records (see STREAM_CODEC_REC_HDR_SIZE in stream.h)     */
/* ------------------------------------------------------------------ */

static void codec_rec_pack(uint8_t *hdr, uint16_t len, uint16_t duration,
                           uint64_t start_index)
{
    memcpy(hdr, &len, 2);
    memcpy(hdr + 2, &duration, 2);
    memcpy(hdr + 4, &start_index, 8);
}

static void codec_rec_unpack(const uint8_t *hdr, uint16_t *len,
                             uint16_t *duration, uint64_t *start_index)
{
    memcpy(len, hdr, 2);
    memcpy(duration, hdr + 2, 2);
    memcpy(start_index, hdr + 4, 8);
}

/* Shared produce core: validate and enqueue one codec frame atomically.
 * have_index selects an explicit start index over the accumulator.
 * account_drop: 1 = a full ring drops the frame permanently (RX producer;
 * overrun + dropped-sample accounting, index advances over the hole);
 * 0 = a full ring is a transient failure the caller will retry (TX app
 * write; nothing accounted, index unchanged). Returns len, 0 when the
 * ring is full, or a negative error. */
static ssize_t codec_produce(struct rig_stream *stream, const void *buf,
                             size_t len, uint32_t duration_samples,
                             int have_index, uint64_t start_index,
                             int account_drop)
{
    if (!stream || !buf)
    {
        return -RIG_EINVAL;
    }

    if (!stream->is_codec)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: not a codec stream\n", __func__);
        return -RIG_EINVAL;
    }

    if (len == 0 || len > (size_t)stream->max_payload || len > UINT16_MAX
            || duration_samples > UINT16_MAX)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: codec frame len %zu / duration %u outside limits "
                  "(max_payload %d)\n", __func__, len,
                  (unsigned)duration_samples, stream->max_payload);
        return -RIG_EINVAL;
    }

    /* Muted RX producer: discard, but keep the index advancing so the
     * position stays truthful when unmuted. */
    if (stream->muted)
    {
        pthread_mutex_lock(&stream->ringbuf.lock);
        stream->codec_pos = (have_index ? start_index : stream->codec_pos)
                            + duration_samples;
        pthread_mutex_unlock(&stream->ringbuf.lock);
        return (ssize_t)len;
    }

    uint8_t hdr[STREAM_CODEC_REC_HDR_SIZE];
    size_t stored;

    pthread_mutex_lock(&stream->ringbuf.lock);
    uint64_t idx = have_index ? start_index : stream->codec_pos;
    pthread_mutex_unlock(&stream->ringbuf.lock);

    codec_rec_pack(hdr, (uint16_t)len, (uint16_t)duration_samples, idx);

    stored = stream_ringbuf_write_record(&stream->ringbuf,
                                         hdr, sizeof(hdr), buf, len);

    pthread_mutex_lock(&stream->ringbuf.lock);

    if (stored == 0 && !account_drop)
    {
        /* Transient full for a retrying caller: no accounting, no index
         * movement. */
        pthread_mutex_unlock(&stream->ringbuf.lock);
        return 0;
    }

    if (stored == 0)
    {
        /* Drop-newest: the frame never entered the ring. The producer is
         * authoritative for the dropped-sample total (the consume-side
         * overrun attribution is suppressed for codec streams — the hole
         * sits AFTER the stored records, so the announced-skip mechanism
         * would double count). The index advance below exposes the hole
         * to the consumer as a start-index jump. */
        stream->ringbuf.overrun_count++;
        stream->pending_drop_flags |= RIG_STREAM_DROP_OVERRUN;
        stream->dropped_samples_overrun += duration_samples;
    }

    if (stored != 0)
    {
        stream->codec_frames++;
    }

    stream->codec_pos = idx + duration_samples;
    pthread_mutex_unlock(&stream->ringbuf.lock);

    return stored == 0 ? 0 : (ssize_t)len;
}

ssize_t stream_backend_write_frame(struct rig_stream *stream,
                                   const void *buf, size_t len,
                                   uint32_t duration_samples)
{
    return codec_produce(stream, buf, len, duration_samples, 0, 0, 1);
}

ssize_t stream_backend_write_frame_indexed(struct rig_stream *stream,
                                           const void *buf, size_t len,
                                           uint32_t duration_samples,
                                           uint64_t start_index)
{
    return codec_produce(stream, buf, len, duration_samples, 1, start_index,
                         1);
}

/* Dequeue one whole codec-frame record under a single lock hold. cap too
 * small for the next frame leaves the record unconsumed. */
static int codec_consume_locked(struct rig_stream *stream, void *buf,
                                size_t cap, size_t *len,
                                uint32_t *duration_samples,
                                uint64_t *start_index)
{
    struct rig_stream_ringbuf *rb = &stream->ringbuf;
    uint8_t hdr[STREAM_CODEC_REC_HDR_SIZE];
    uint16_t rlen, rdur;
    uint64_t ridx;

    /* Records are written atomically, so a non-empty codec ring always
     * holds at least one whole record. */
    if (stream_ringbuf_peek_locked(rb, hdr, sizeof(hdr)) != sizeof(hdr))
    {
        return -RIG_EPROTO;   /* ring corrupted — cannot happen by design */
    }

    codec_rec_unpack(hdr, &rlen, &rdur, &ridx);

    if ((size_t)rlen > cap)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: buffer %zu too small for codec frame %u "
                  "(rig_stream_get_max_payload() bytes always suffice)\n",
                  __func__, cap, (unsigned)rlen);
        return -RIG_EINVAL;
    }

    stream_ringbuf_consume_locked(rb, hdr, sizeof(hdr));
    stream_ringbuf_consume_locked(rb, buf, rlen);

    *len = rlen;

    if (duration_samples)
    {
        *duration_samples = rdur;
    }

    if (start_index)
    {
        *start_index = ridx;
    }

    return RIG_OK;
}

int stream_backend_read_frame(struct rig_stream *stream, void *buf,
                              size_t cap, size_t *len,
                              uint32_t *duration_samples,
                              uint64_t *start_index, int timeout_ms)
{
    if (!stream || !buf || !len || !stream->is_codec)
    {
        return -RIG_EINVAL;
    }

    struct rig_stream_ringbuf *rb = &stream->ringbuf;

    *len = 0;
    pthread_mutex_lock(&rb->lock);

    if (stream_ringbuf_wait_data_locked(rb, timeout_ms) < 0)
    {
        pthread_mutex_unlock(&rb->lock);
        return -RIG_ETIMEOUT;
    }

    int ret = codec_consume_locked(stream, buf, cap, len,
                                   duration_samples, start_index);
    pthread_mutex_unlock(&rb->lock);

    return ret;
}

/* Resolve the resampler quality for new conversion pipelines on this rig:
 * the stream_resample_quality conf token (stored as RIG_RESAMPLE_* + 1),
 * or the built-in default when unset. */
int stream_resample_quality(RIG *rig)
{
    struct rig_state *rs = rig ? STATE(rig) : NULL;

    if (rs && rs->stream_resample_quality > 0)
    {
        return rs->stream_resample_quality - 1;
    }

    return RIG_RESAMPLE_MEDIUM;
}

uint64_t stream_scale_backend_samples(const struct rig_stream *stream,
                                      uint64_t samples)
{
    if (!stream || !stream->conv
            || stream->backend_config.sample_rate == stream->config.sample_rate
            || stream->backend_config.sample_rate <= 0)
    {
        return samples;
    }

    /* Backend (native-rate) domain -> ring (client-rate) domain. Only RX
     * streams have backend-side producers pushing counts; TX accounting
     * already runs in the native ring domain. */
    return samples * (uint64_t)stream->config.sample_rate
           / (uint64_t)stream->backend_config.sample_rate;
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

    if (rb->closing || rb->failed)
    {
        int failed = rb->failed;
        pthread_mutex_unlock(&rb->lock);
        *bytes_read = 0;
        ret = failed ? -RIG_EIO : -RIG_ENAVAIL;
        goto out;
    }

    stream->blocked_waiters++;

    if (stream_ringbuf_wait_data_locked(rb, timeout_ms) < 0)
    {
        int closing = rb->closing;
        int failed = rb->failed;

        if (--stream->blocked_waiters == 0 && closing)
        {
            pthread_cond_signal(&stream->quiesced);
        }

        pthread_mutex_unlock(&rb->lock);
        *bytes_read = 0;
        ret = closing ? -RIG_ENAVAIL : (failed ? -RIG_EIO : -RIG_ETIMEOUT);
        goto out;
    }

    if (stream->is_codec)
    {
        /* Codec stream: exactly ONE whole codec frame per call. The
         * record's start index and duration drive the accounting; a
         * buffer of rig_stream_get_max_payload() bytes always
         * suffices. */
        size_t flen = 0;
        uint32_t fdur = 0;
        uint64_t fidx = 0;
        int cret = codec_consume_locked(stream, buffer, buffer_size,
                                        &flen, &fdur, &fidx);

        if (cret != RIG_OK)
        {
            if (--stream->blocked_waiters == 0 && rb->closing)
            {
                pthread_cond_signal(&stream->quiesced);
            }

            pthread_mutex_unlock(&rb->lock);
            *bytes_read = 0;
            ret = cret;
            goto out;
        }

        stream_consume_account_locked(stream, fidx, fdur, info);
        pthread_mutex_unlock(&rb->lock);

        /* Muted: the frame is consumed and DISCARDED — zeroed bytes are
         * not a valid codec frame, so the app gets no data instead. */
        if (stream->muted)
        {
            *bytes_read = 0;
        }
        else
        {
            *bytes_read = flen;
        }

        if (info)
        {
            info->codec_frame_samples = fdur;
            stream_fill_read_time(stream, info);
        }

        pthread_mutex_lock(&rb->lock);

        if (--stream->blocked_waiters == 0 && rb->closing)
        {
            pthread_cond_signal(&stream->quiesced);
        }

        pthread_mutex_unlock(&rb->lock);

        ret = RIG_OK;
        goto out;
    }

    if (stream->is_codec)
    {
        /* Codec stream: exactly ONE whole codec frame per call. The
         * record's start index and duration drive the accounting; a
         * buffer of rig_stream_get_max_payload() bytes always
         * suffices. */
        size_t flen = 0;
        uint32_t fdur = 0;
        uint64_t fidx = 0;
        int cret = codec_consume_locked(stream, buffer, buffer_size,
                                        &flen, &fdur, &fidx);

        if (cret != RIG_OK)
        {
            if (--stream->blocked_waiters == 0 && rb->closing)
            {
                pthread_cond_signal(&stream->quiesced);
            }

            pthread_mutex_unlock(&rb->lock);
            *bytes_read = 0;
            ret = cret;
            goto out;
        }

        stream_consume_account_locked(stream, fidx, fdur, info);
        pthread_mutex_unlock(&rb->lock);

        /* Muted: the frame is consumed and DISCARDED — zeroed bytes are
         * not a valid codec frame, so the app gets no data instead. */
        if (stream->muted)
        {
            *bytes_read = 0;
        }
        else
        {
            *bytes_read = flen;
        }

        if (info)
        {
            info->codec_frame_samples = fdur;
            stream_fill_read_time(stream, info);
        }

        pthread_mutex_lock(&rb->lock);

        if (--stream->blocked_waiters == 0 && rb->closing)
        {
            pthread_cond_signal(&stream->quiesced);
        }

        pthread_mutex_unlock(&rb->lock);

        ret = RIG_OK;
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

    if (stream->is_codec)
    {
        /* Codec stream: exactly ONE whole codec frame per call, duration
         * declared via write_info. Never dropped and never overwritten:
         * on a full ring the call polls for space up to timeout_ms. */
        uint32_t dur = info ? info->codec_frame_samples : 0;
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (;;)
        {
            ssize_t got = codec_produce(stream, buffer, buffer_size, dur,
                                        0, 0, 0);

            if (got < 0)
            {
                ret = (int)got;
                goto out;
            }

            if (got > 0)
            {
                *bytes_written = (size_t)got;
                goto out;
            }

            /* Ring full: poll for space up to the timeout — the
             * app-facing contract is block-not-drop. */
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000
                              + (now.tv_nsec - start.tv_nsec) / 1000000;

            if (timeout_ms >= 0 && elapsed_ms >= timeout_ms)
            {
                *bytes_written = 0;
                ret = -RIG_ETIMEOUT;
                goto out;
            }

            usleep(1000);
        }
    }

    int ovr_before = stream->ringbuf.overrun_count;

    if (stream->conv)
    {
        /* Converted TX stream: the ring holds the backend-native format,
         * so the client's bytes run through the pipeline on the way in. */
        ssize_t consumed = stream_conv_process(stream->conv, buffer,
                                               buffer_size, conv_ring_sink,
                                               stream);

        if (consumed < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: conversion failed\n", __func__);
            ret = -RIG_EINVAL;
            goto out;
        }

        *bytes_written = (size_t)consumed;
    }
    else
    {
        *bytes_written = stream_ringbuf_write(&stream->ringbuf, buffer,
                                              buffer_size);
    }

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
    /* Backends report losses in their own (native) sample domain; the ring
     * accounting runs in the consumer domain, so scale under RX rate
     * conversion (identity otherwise). */
    dropped_samples = stream_scale_backend_samples(stream, dropped_samples);

    pthread_mutex_lock(&stream->ringbuf.lock);

    stream->pending_drop_flags |= drop_flag;

    switch (drop_flag)
    {
    case RIG_STREAM_DROP_GAP:
        stream->gap_count++;

        /* A codec stream's producer index accumulates in codec_pos; a
         * radio-side gap advances it so the next frame's start index
         * exposes the hole. */
        if (stream->is_codec)
        {
            stream->codec_pos += dropped_samples;
        }


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
    stats->codec_frames = stream->codec_frames;

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

    if (s->is_codec)
    {
        /* Decoded-sample producer position: frame durations plus gap
         * skips, all folded into codec_pos as they happen. */
        uint64_t pos = s->codec_pos;
        pthread_mutex_unlock(&s->ringbuf.lock);
        return pos;
    }

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

int HAMLIB_API rig_stream_get_conversions(const rig_stream_t *stream)
{
    if (!stream)
    {
        return -RIG_EINVAL;
    }

    return stream->conversions;
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
