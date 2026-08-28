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

/* Capture-time anchors for the Hamlib streaming subsystem. */
/* Records sample_index <-> UTC anchors and interpolates read-time stamps. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "stream.h"
#include "stream_ringbuf.h"
#include "stream_time.h"

#include <string.h>
#include <stdint.h>


/* ------------------------------------------------------------------ */
/* Time anchors: push/get, interpolation, staleness watchdog           */
/* ------------------------------------------------------------------ */

int rig_stream_push_time_anchor(struct rig_stream *stream,
                                const struct rig_stream_time_anchor *anchor)
{
    if (!stream || !anchor)
    {
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&stream->ringbuf.lock);

    stream->anchors[stream->anchor_head] = *anchor;
    /* Backends anchor in their native sample domain; map into the ring
     * (consumer) domain when RX rate conversion is active. The rounding
     * this introduces is well below the resampler's own group delay. */
    stream->anchors[stream->anchor_head].sample_index =
        stream_scale_backend_samples(stream, anchor->sample_index);
    stream->anchor_head = (stream->anchor_head + 1) % RIG_STREAM_ANCHOR_DEPTH;

    if (stream->anchor_count < RIG_STREAM_ANCHOR_DEPTH)
    {
        stream->anchor_count++;
    }

    pthread_mutex_unlock(&stream->ringbuf.lock);

    return RIG_OK;
}


int HAMLIB_API rig_stream_get_time_anchor(rig_stream_t *stream,
        struct rig_stream_time_anchor *anchor)
{
    if (!stream || !anchor)
    {
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&stream->ringbuf.lock);

    if (stream->anchor_count == 0)
    {
        pthread_mutex_unlock(&stream->ringbuf.lock);
        return -RIG_ENAVAIL;
    }

    int newest = (stream->anchor_head + RIG_STREAM_ANCHOR_DEPTH - 1)
                 % RIG_STREAM_ANCHOR_DEPTH;
    *anchor = stream->anchors[newest];

    pthread_mutex_unlock(&stream->ringbuf.lock);

    return RIG_OK;
}


/* Newest anchor with sample_index <= idx. Caller holds ringbuf.lock.
 * Returns 0 on success, -1 if no usable anchor. */
static int anchor_lookup_locked(struct rig_stream *stream, uint64_t idx,
                                struct rig_stream_time_anchor *out)
{
    for (int back = 1; back <= stream->anchor_count; back++)
    {
        int slot = (stream->anchor_head + RIG_STREAM_ANCHOR_DEPTH - back)
                   % RIG_STREAM_ANCHOR_DEPTH;

        if (stream->anchors[slot].sample_index <= idx)
        {
            *out = stream->anchors[slot];
            return 0;
        }
    }

    return -1;
}


void stream_fill_read_time(struct rig_stream *stream,
                           struct rig_stream_read_info *info)
{
    struct rig_stream_time_anchor a;

    info->time_valid = 0;
    info->seconds = 0;
    info->picoseconds = 0;
    info->time_source = RIG_STREAM_TIME_SRC_NONE;
    info->time_flags = 0;
    info->time_accuracy = RIG_STREAM_TIME_ACC_UNKNOWN;

    /* A discontinuity describes the data, not the clock — report it even
     * when no usable time is available. */
    if (info->dropped_samples > 0
            || (info->drop_flags & RIG_STREAM_DROP_UNSIZED))
    {
        info->time_flags |= RIG_STREAM_TIME_FLAG_DISCONTINUITY;

        if (info->drop_flags & RIG_STREAM_DROP_OVERRUN)
        {
            info->time_flags |= RIG_STREAM_TIME_FLAG_DISC_OVERRUN;
        }
    }

    pthread_mutex_lock(&stream->ringbuf.lock);

    int found = anchor_lookup_locked(stream, info->sample_index, &a) == 0;
    unsigned int coarse_ms = stream->stale_coarse_ms;
    unsigned int invalidate_ms = stream->stale_invalidate_ms;

    pthread_mutex_unlock(&stream->ringbuf.lock);

    if (!found)
    {
        return;
    }

    uint32_t rate = (uint32_t)stream->config.sample_rate;
    uint64_t delta = info->sample_index - a.sample_index;
    int64_t sec = a.seconds;
    uint64_t ps = a.picoseconds;

    stream_time_add_samples(&sec, &ps, delta, rate);

    /* Staleness watchdog: the anchor's age in stream time bounds how far
     * nominal-rate interpolation can drift. Degrade visibly, never
     * fabricate freshness. */
    uint64_t staleness_ms = rate > 0 ? delta * 1000 / rate : 0;

    if (invalidate_ms > 0 && staleness_ms > invalidate_ms)
    {
        return;     /* time_valid stays 0 */
    }

    info->time_valid = 1;
    info->seconds = sec;
    info->picoseconds = ps;
    info->time_source = a.source;
    info->time_flags |= a.flags;
    info->time_accuracy = a.accuracy;

    if (coarse_ms > 0 && staleness_ms > coarse_ms
            && a.accuracy != RIG_STREAM_TIME_ACC_UNKNOWN)
    {
        info->time_accuracy = RIG_STREAM_TIME_ACC_COARSE;
    }
}
