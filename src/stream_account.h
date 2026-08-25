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

#ifndef HAMLIB_STREAM_ACCOUNT_H
#define HAMLIB_STREAM_ACCOUNT_H

#include <hamlib/rig.h>
#include <stdint.h>

struct rig_stream;
struct rig_stream_read_info;
struct rig_stream_tx_target;


/* --- Loss accounting (backend- and client-facing) --- */

/* Backend-facing: report a radio/network-side gap of dropped_samples samples
 * (0 = size unknown) detected in the radio protocol. Call BEFORE writing
 * the post-gap data; follow with a DISCONTINUITY anchor push.
 * Returns RIG_OK or -RIG_EINVAL. */
int rig_stream_mark_gap(struct rig_stream *stream, uint64_t dropped_samples);

/* Producer sample index of the oldest readable byte (locks internally).
 * Returns 0 when frame_bytes is unknown (compressed formats). */
uint64_t stream_first_readable_index(struct rig_stream *stream);

/* Consumer-side accounting for one read of frames_read frames starting at
 * producer index first_index: computes the dropped hole vs next_expected,
 * attributes causes, updates per-cause totals, and fills info (may be
 * NULL; position/drop fields only — time fields are the caller's job). */
void stream_consume_account(struct rig_stream *stream, uint64_t first_index,
                            uint64_t frames_read,
                            struct rig_stream_read_info *info);

/* As stream_consume_account(), but the caller already holds ringbuf.lock.
 * Lets a reader snapshot the consume position and account for it atomically,
 * so a concurrent stream_skip_samples() cannot land in the gap between the two
 * and mis-attribute the loss. */
void stream_consume_account_locked(struct rig_stream *stream,
                                   uint64_t first_index,
                                   uint64_t frames_read,
                                   struct rig_stream_read_info *info);


/* --- TX burst targets --- */

/* Queue a TX burst target (frontend write path; drop-oldest FIFO).
 * target->sample_index is the producer index of the burst's first sample.
 * Returns RIG_OK or -RIG_EINVAL. */
int stream_push_tx_target(struct rig_stream *stream,
                          const struct rig_stream_tx_target *target);

/* Backend-facing: pop the oldest pending burst target whose sample_index
 * is <= up_to_index (the backend's current TX position). Returns 1 when a
 * target was returned, 0 when none is pending. */
int rig_stream_pop_tx_target(struct rig_stream *stream, uint64_t up_to_index,
                             struct rig_stream_tx_target *target);


#endif /* HAMLIB_STREAM_ACCOUNT_H */
