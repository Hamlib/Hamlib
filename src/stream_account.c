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

/* Loss accounting and TX burst targets for the Hamlib streaming subsystem. */
/* Tracks dropped/lost samples and queues scheduled timed-TX boundaries. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "stream.h"
#include "stream_ringbuf.h"

#include <string.h>
#include <stdint.h>


/* ------------------------------------------------------------------ */
/* Loss accounting: producer index, gap marking, cause attribution     */
/* ------------------------------------------------------------------ */

uint64_t stream_first_readable_index(struct rig_stream *stream)
{
    pthread_mutex_lock(&stream->ringbuf.lock);
    uint64_t idx = stream_first_readable_index_locked(stream);
    pthread_mutex_unlock(&stream->ringbuf.lock);
    return idx;
}


int rig_stream_mark_gap(struct rig_stream *stream, uint64_t dropped_samples)
{
    if (!stream)
    {
        return -RIG_EINVAL;
    }

    stream_skip_samples(stream, dropped_samples, RIG_STREAM_DROP_GAP);
    return RIG_OK;
}


void stream_consume_account_locked(struct rig_stream *stream,
                                   uint64_t first_index,
                                   uint64_t frames_read,
                                   struct rig_stream_read_info *info)
{
    uint64_t dropped = first_index > stream->next_expected
                       ? first_index - stream->next_expected : 0;

    /* The portion of the hole not explained by announced skips is a
     * local ring-buffer overrun. */
    uint64_t skipped_part = stream->skip_samples_unread;

    if (skipped_part > dropped)
    {
        skipped_part = dropped;
    }

    uint64_t overrun_part = dropped - skipped_part;
    uint8_t flags = stream->pending_drop_flags;

    if (overrun_part > 0)
    {
        flags |= RIG_STREAM_DROP_OVERRUN;
        stream->dropped_samples_overrun += overrun_part;
    }

    stream->next_expected = first_index + frames_read;
    stream->skip_samples_unread = 0;
    stream->pending_drop_flags = 0;

    if (info)
    {
        memset(info, 0, sizeof(*info));
        info->sample_index = first_index;
        info->dropped_samples = dropped > UINT32_MAX
                                ? UINT32_MAX : (uint32_t)dropped;
        info->drop_flags = flags;
    }
}


void stream_consume_account(struct rig_stream *stream, uint64_t first_index,
                            uint64_t frames_read,
                            struct rig_stream_read_info *info)
{
    pthread_mutex_lock(&stream->ringbuf.lock);
    stream_consume_account_locked(stream, first_index, frames_read, info);
    pthread_mutex_unlock(&stream->ringbuf.lock);
}


/* ------------------------------------------------------------------ */
/* TX burst targets and producer position                              */
/* ------------------------------------------------------------------ */

int stream_push_tx_target(struct rig_stream *stream,
                          const struct rig_stream_tx_target *target)
{
    if (!stream || !target)
    {
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&stream->ringbuf.lock);

    stream->tx_targets[stream->tx_target_head] = *target;
    stream->tx_target_head = (stream->tx_target_head + 1)
                             % RIG_STREAM_TX_TARGET_DEPTH;

    if (stream->tx_target_count < RIG_STREAM_TX_TARGET_DEPTH)
    {
        stream->tx_target_count++;
    }

    pthread_mutex_unlock(&stream->ringbuf.lock);

    return RIG_OK;
}


int rig_stream_pop_tx_target(struct rig_stream *stream, uint64_t up_to_index,
                             struct rig_stream_tx_target *target)
{
    if (!stream || !target)
    {
        return 0;
    }

    pthread_mutex_lock(&stream->ringbuf.lock);

    if (stream->tx_target_count == 0)
    {
        pthread_mutex_unlock(&stream->ringbuf.lock);
        return 0;
    }

    int oldest = (stream->tx_target_head + RIG_STREAM_TX_TARGET_DEPTH
                  - stream->tx_target_count) % RIG_STREAM_TX_TARGET_DEPTH;

    if (stream->tx_targets[oldest].sample_index > up_to_index)
    {
        pthread_mutex_unlock(&stream->ringbuf.lock);
        return 0;
    }

    *target = stream->tx_targets[oldest];
    stream->tx_target_count--;

    pthread_mutex_unlock(&stream->ringbuf.lock);

    return 1;
}
